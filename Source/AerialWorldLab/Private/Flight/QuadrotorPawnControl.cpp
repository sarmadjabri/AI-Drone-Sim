// Resolved flight limits, SI aerodynamics, assisted/manual control and rotor forces.
#include "Flight/QuadrotorPawn.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Flight/FlightControl.h"
#include "Flight/SimulationUnits.h"
#include "QuadrotorControlFrame.h"
#include "QuadrotorLayout.h"

FQuadrotorControlFrame AQuadrotorPawn::ResolveControlFrame(const FPilotModeTuning &ModeTuning) const
{
    FQuadrotorControlFrame Frame;
    Frame.DynamicsParameters.massKg = MassKg;
    Frame.DynamicsParameters.armLengthM = ArmLengthMeters;
    Frame.DynamicsParameters.maxMotorThrustN = MaximumMotorThrustNewtons;
    Frame.DynamicsParameters.motorTimeConstantS = MotorTimeConstantSeconds;
    Frame.DynamicsParameters.yawMomentPerNewtonNm = YawMomentPerNewtonNewtonMeters;

    Frame.bUsesLegacyTuning = FlightMode == EDroneFlightMode::LegacyAssisted;
    Frame.PrecisionScale = 1.0F;
    if (PilotControls->bPrecision)
    {
        Frame.PrecisionScale = PilotControls->PrecisionScale;
    }
    // Resolve mode-specific units and limits once, before evaluating control laws.
    Frame.HorizontalSpeedMetersPerSecond = ModeTuning.Speed;
    Frame.PositionHoldGainPerSecond = ModeTuning.HoldGain;
    Frame.VelocityPredictionSeconds = ModeTuning.Prediction;
    Frame.MaximumYawRateRadiansPerSecond =
        FMath::DegreesToRadians(ModeTuning.YawDegrees) * Frame.PrecisionScale;
    Frame.RollAttitudeGainPerSecond = ModeTuning.AttitudeGain;
    Frame.PitchAttitudeGainPerSecond = ModeTuning.AttitudeGain;
    Frame.MaximumRollRateRadiansPerSecond = FMath::DegreesToRadians(ModeTuning.RateDegrees);
    Frame.MaximumPitchRateRadiansPerSecond = Frame.MaximumRollRateRadiansPerSecond;
    Frame.MaximumVerticalAccelerationMetersPerSecondSquared = ModeTuning.VerticalAcceleration;
    Frame.MinimumVerticalThrustFraction = 0.35F;
    if (Frame.bUsesLegacyTuning)
    {
        Frame.HorizontalSpeedMetersPerSecond = MaximumHorizontalSpeedMetersPerSecond;
        Frame.PositionHoldGainPerSecond = PositionHoldGainPerSecond;
        Frame.VelocityPredictionSeconds = VelocityPredictionSeconds;
        Frame.MaximumYawRateRadiansPerSecond = MaximumYawRateRadiansPerSecond;
        Frame.RollAttitudeGainPerSecond = RollAttitudeGainPerSecond;
        Frame.PitchAttitudeGainPerSecond = PitchAttitudeGainPerSecond;
        Frame.MaximumRollRateRadiansPerSecond = MaximumRollRateRadiansPerSecond;
        Frame.MaximumPitchRateRadiansPerSecond = MaximumPitchRateRadiansPerSecond;
        Frame.MaximumVerticalAccelerationMetersPerSecondSquared = MaximumVerticalAcceleration;
        Frame.MinimumVerticalThrustFraction = 0.7F;
    }

    return Frame;
}

FVector AQuadrotorPawn::ApplyAerodynamicDrag(const FQuat &BodyRotation)
{
    // Calculate in the body frame so the broad underside has its own drag area.
    const FVector RelativeAirVelocity =
        BodyRotation.UnrotateVector(GetVelocityMetersPerSecond() - WindVelocityMetersPerSecond);
    FVector DragForceWorld = FVector::ZeroVector;
    std::array<float, 3> DragForceBody{};
    if (Aether::QuadrotorDynamics::CalculateBodyDrag(
            {static_cast<float>(RelativeAirVelocity.X), static_cast<float>(RelativeAirVelocity.Y),
             static_cast<float>(RelativeAirVelocity.Z)},
            {static_cast<float>(DragAreaSquareMeters.X), static_cast<float>(DragAreaSquareMeters.Y),
             static_cast<float>(DragAreaSquareMeters.Z)},
            AirDensityKgPerCubicMeter, DragForceBody))
    {
        DragForceWorld = BodyRotation.RotateVector(
            FVector(DragForceBody[0], DragForceBody[1], DragForceBody[2]));
        Body->AddForce(DragForceWorld * Aether::Units::ChaosForceUnitsPerNewton);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid aerodynamic parameters; drag was not applied"));
    }

    return DragForceWorld;
}

FVector AQuadrotorPawn::CalculateAssistedVelocity(float DeltaSeconds,
                                                  const FQuadrotorControlFrame &Frame,
                                                  FVector &HorizontalVelocity)
{
    const float HeadingRadians = FMath::DegreesToRadians(Frame.CurrentRotation.Yaw);
    const FVector ForwardDirection(FMath::Cos(HeadingRadians), FMath::Sin(HeadingRadians), 0.0F);
    const FVector RightDirection(-FMath::Sin(HeadingRadians), FMath::Cos(HeadingRadians), 0.0F);
    FVector DesiredVelocity = (ForwardDirection * ForwardInput + RightDirection * LateralInput)
                                  .GetClampedToMaxSize(1.0F) *
                              Frame.HorizontalSpeedMetersPerSecond * Frame.PrecisionScale;
    HorizontalVelocity = GetVelocityMetersPerSecond();
    HorizontalVelocity.Z = 0.0;
    // Decide pilot intent before expo/slew shaping: tiny shaped values are still commands.
    const float IntentDeadzone = FMath::Clamp(StickDeadzone, 0.0F, 0.5F);
    bool bTranslationReleased;
    if (Frame.bUsesLegacyTuning && !bUseMappedInput)
    {
        bTranslationReleased = FMath::Abs(RawForwardInput) <= IntentDeadzone &&
                               FMath::Abs(RawLateralInput) <= IntentDeadzone;
    }
    else
    {
        bTranslationReleased =
            FMath::IsNearlyZero(ForwardInputTarget) && FMath::IsNearlyZero(LateralInputTarget);
    }
    if (bTranslationReleased)
    {
        TranslationNeutralSeconds += DeltaSeconds;
    }
    else
    {
        TranslationNeutralSeconds = 0.0F;
    }
    if (!bTranslationReleased)
    {
        bHoldingPosition = false;
    }
    else if (!bHoldingPosition && TranslationNeutralSeconds >= PositionHoldNeutralDelaySeconds &&
             HorizontalVelocity.Size() < PositionHoldCaptureSpeed &&
             Frame.BodyRotation.GetAxisZ().Z >
                 FMath::Cos(FMath::DegreesToRadians(PositionHoldCaptureTiltDegrees)))
    {
        // Capture where braking finishes, never the release point behind the aircraft.
        HoldPositionCentimeters = GetActorLocation();
        bHoldingPosition = true;
    }
    if (bTranslationReleased && bHoldingPosition)
    {
        FVector PositionError =
            (HoldPositionCentimeters - GetActorLocation()) / Aether::Units::CentimetersPerMeter;
        PositionError.Z = 0.0;
        DesiredVelocity = (PositionError * Frame.PositionHoldGainPerSecond)
                              .GetClampedToMaxSize(PositionHoldMaximumSpeed);
    }

    return DesiredVelocity;
}

FVector AQuadrotorPawn::CalculateAssistedBodyRates(float DeltaSeconds,
                                                   const FQuadrotorControlFrame &Frame,
                                                   const FPilotModeTuning &ModeTuning,
                                                   const FVector &DesiredVelocity,
                                                   const FVector &HorizontalVelocity)
{
    FVector DesiredBodyRate = FVector::ZeroVector;
    const bool bBraking =
        FVector::DotProduct(DesiredVelocity - HorizontalVelocity, HorizontalVelocity) < 0.0;
    float VelocityGain = ModeTuning.VelocityGain;
    float TiltDegrees = ModeTuning.Tilt;
    float JerkLimit = ModeTuning.Jerk;
    float AccelerationLimit = ModeTuning.Acceleration;
    if (bBraking)
    {
        VelocityGain = ModeTuning.BrakeGain;
        TiltDegrees = ModeTuning.BrakeTilt;
        JerkLimit = ModeTuning.BrakeJerk;
        AccelerationLimit = ModeTuning.Braking;
    }
    if (Frame.bUsesLegacyTuning)
    {
        if (bBraking)
        {
            VelocityGain = BrakingVelocityGainPerSecond;
            TiltDegrees = MaximumBrakingTiltDegrees;
            JerkLimit = BrakingJerkMetersPerSecondCubed;
        }
        else
        {
            VelocityGain = HorizontalVelocityGainPerSecond;
            TiltDegrees = MaximumTiltDegrees;
            JerkLimit = AccelerationJerkMetersPerSecondCubed;
        }
    }
    const float TiltAcceleration =
        GravityMetersPerSecondSquared * FMath::Tan(FMath::DegreesToRadians(TiltDegrees));
    float MaximumHorizontalAcceleration = TiltAcceleration;
    if (!Frame.bUsesLegacyTuning)
    {
        MaximumHorizontalAcceleration = FMath::Min(TiltAcceleration, AccelerationLimit);
    }
    FVector AccelerationTarget = Aether::VelocityAcceleration(
        DesiredVelocity, HorizontalVelocity, Frame.BodyRotation.GetAxisZ(),
        GravityMetersPerSecondSquared, VelocityGain, MaximumHorizontalAcceleration,
        Frame.VelocityPredictionSeconds, Frame.DragForceWorldNewtons / FMath::Max(MassKg, 0.1F));
    // Assisted control compensates the modeled air load so wind does not produce
    // a permanent velocity error. Manual modes receive the raw disturbance only.
    FVector DragCompensation = -Frame.DragForceWorldNewtons / FMath::Max(MassKg, 0.1F);
    DragCompensation.Z = 0;
    AccelerationTarget =
        (AccelerationTarget + DragCompensation).GetClampedToMaxSize(MaximumHorizontalAcceleration);
    // Limit physical acceleration change in m/s^3, with separate braking tuning.
    HorizontalAccelerationCommand += (AccelerationTarget - HorizontalAccelerationCommand)
                                         .GetClampedToMaxSize(JerkLimit * DeltaSeconds);
    // The target is bounded above. Preserve the jerk bound when switching to a lower tilt
    // limit; an immediate clamp here would introduce an acceleration discontinuity.

    YawTargetDegrees = FRotator::NormalizeAxis(
        YawTargetDegrees +
        FMath::RadiansToDegrees(YawInput * Frame.MaximumYawRateRadiansPerSecond * DeltaSeconds));
    const FVector RotationError =
        Aether::AttitudeError(Frame.BodyRotation, HorizontalAccelerationCommand,
                              GravityMetersPerSecondSquared, YawTargetDegrees);
    // Quaternion error is expressed in physics body axes; Euler roll/pitch signs are not.
    DesiredBodyRate = FVector(
        FMath::Clamp(RotationError.X * Frame.RollAttitudeGainPerSecond,
                     -Frame.MaximumRollRateRadiansPerSecond, Frame.MaximumRollRateRadiansPerSecond),
        FMath::Clamp(RotationError.Y * Frame.PitchAttitudeGainPerSecond,
                     -Frame.MaximumPitchRateRadiansPerSecond,
                     Frame.MaximumPitchRateRadiansPerSecond),
        FMath::Clamp(RotationError.Z * YawHeadingGainPerSecond,
                     -Frame.MaximumYawRateRadiansPerSecond, Frame.MaximumYawRateRadiansPerSecond));

    return DesiredBodyRate;
}

void AQuadrotorPawn::UpdateAssistedAltitude(float DeltaSeconds, const FQuadrotorControlFrame &Frame,
                                            const FPilotModeTuning &ModeTuning,
                                            const FVector &DesiredVelocity)
{
    const float CurrentAltitudeMeters =
        Body->GetComponentLocation().Z / Aether::Units::CentimetersPerMeter;
    const float VerticalVelocityMetersPerSecond =
        Body->GetPhysicsLinearVelocity().Z / Aether::Units::CentimetersPerMeter;
    if (bLanding && IsGrounded())
    {
        MotorState = EMotorState::Disarmed;
        bLanding = false;
    }
    float VerticalCommand = ThrottleInput;
    float MinimumTargetAltitude = MinimumAltitudeMeters;
    if (bLanding)
    {
        VerticalCommand = -0.5F;
        MinimumTargetAltitude = -10000.0F;
    }
    float VerticalSpeed = ModeTuning.Descent;
    if (VerticalCommand >= 0)
    {
        VerticalSpeed = ModeTuning.Ascent;
    }
    if (Frame.bUsesLegacyTuning)
    {
        VerticalSpeed = VerticalSpeedMetersPerSecond;
    }
    CommandedVelocity = DesiredVelocity;
    CommandedVelocity.Z = VerticalCommand * VerticalSpeed * Frame.PrecisionScale;
    AltitudeTargetMeters = FMath::Clamp(AltitudeTargetMeters + CommandedVelocity.Z * DeltaSeconds,
                                        MinimumTargetAltitude, MaximumAltitudeMeters);
    const float AltitudeError = AltitudeTargetMeters - CurrentAltitudeMeters;
    const float DesiredVerticalAcceleration =
        FMath::Clamp(AltitudeError * AltitudePositionGainPerSecondSquared -
                         VerticalVelocityMetersPerSecond * AltitudeVelocityGainPerSecond,
                     -Frame.MaximumVerticalAccelerationMetersPerSecondSquared,
                     Frame.MaximumVerticalAccelerationMetersPerSecondSquared);
    const float VerticalThrustFraction =
        FMath::Max(Frame.BodyRotation.GetAxisZ().Z, Frame.MinimumVerticalThrustFraction);
    const float DesiredCollectiveThrust =
        (MassKg * (GravityMetersPerSecondSquared + DesiredVerticalAcceleration) -
         Frame.DragForceWorldNewtons.Z) /
        VerticalThrustFraction;
    const float BoundedCollectiveTarget =
        FMath::Clamp(DesiredCollectiveThrust, 0.0F, 4.0F * MaximumMotorThrustNewtons);
    CollectiveCommandNewtons =
        Aether::SlewTowards(CollectiveCommandNewtons, BoundedCollectiveTarget,
                            CollectiveThrustRampNewtonsPerSecond, DeltaSeconds);
}

FVector AQuadrotorPawn::UpdateManualFlightCommands(float DeltaSeconds,
                                                   const FQuadrotorControlFrame &Frame,
                                                   const FPilotModeTuning &ModeTuning)
{
    FVector DesiredBodyRate = FVector::ZeroVector;
    // These control laws never execute the velocity/position/altitude controllers.
    bHoldingPosition = false;
    HorizontalAccelerationCommand = FVector::ZeroVector;
    const float Rate = FMath::DegreesToRadians(ModeTuning.RateDegrees) * Frame.PrecisionScale;
    DesiredBodyRate =
        FVector(-LateralInput * Rate, ForwardInput * Rate,
                YawInput * FMath::DegreesToRadians(ModeTuning.YawDegrees) * Frame.PrecisionScale);
    if (FlightMode == EDroneFlightMode::Angle)
    {
        const float Heading = FMath::DegreesToRadians(Frame.CurrentRotation.Yaw);
        const FVector Forward(FMath::Cos(Heading), FMath::Sin(Heading), 0);
        const FVector Right(-FMath::Sin(Heading), FMath::Cos(Heading), 0);
        const FVector Tilt =
            (Forward * FMath::Tan(FMath::DegreesToRadians(ForwardInput * ModeTuning.Tilt *
                                                          Frame.PrecisionScale)) +
             Right * FMath::Tan(FMath::DegreesToRadians(LateralInput * ModeTuning.Tilt *
                                                        Frame.PrecisionScale))) *
            GravityMetersPerSecondSquared;
        const FVector Error = Aether::AttitudeError(
            Frame.BodyRotation, Tilt, GravityMetersPerSecondSquared, Frame.CurrentRotation.Yaw);
        DesiredBodyRate.X = FMath::Clamp(Error.X * ModeTuning.AttitudeGain, -Rate, Rate);
        DesiredBodyRate.Y = FMath::Clamp(Error.Y * ModeTuning.AttitudeGain, -Rate, Rate);
    }
    if (MotorState == EMotorState::Armed && !bAwaitingNeutralInput)
    {
        if (PilotControls->CollectiveMode == ECollectiveMode::Incremental)
        {
            float ThrottleRate = PilotControls->ThrottleDecrease;
            if (ThrottleInput >= 0)
            {
                ThrottleRate = PilotControls->ThrottleIncrease;
            }
            PilotControls->Throttle = FMath::Clamp(
                PilotControls->Throttle + ThrottleInput * ThrottleRate * DeltaSeconds, 0.0F, 1.0F);
        }
        else
        {
            PilotControls->Throttle = FMath::Clamp((ThrottleInput + 1.0F) * 0.5F, 0.0F, 1.0F);
        }
    }
    CollectiveCommandNewtons = Aether::SlewTowards(
        CollectiveCommandNewtons, PilotControls->Throttle * GetMaximumCollectiveThrustNewtons(),
        CollectiveThrustRampNewtonsPerSecond, DeltaSeconds);

    return DesiredBodyRate;
}

void AQuadrotorPawn::ApplyMotorCommands(float DeltaSeconds, const FQuadrotorControlFrame &Frame,
                                        FVector DesiredBodyRate)
{
    CommandedBodyRate = DesiredBodyRate;
    if (MotorState != EMotorState::Armed)
    {
        CollectiveCommandNewtons = 0;
        DesiredBodyRate = FVector::ZeroVector;
    }

    Aether::DesiredWrench Desired;
    Desired.collectiveThrustN = CollectiveCommandNewtons;
    Desired.rollTorqueNm = RollRateGainNewtonMeterSeconds *
                           (DesiredBodyRate.X - Frame.AngularVelocityBodyRadiansPerSecond.X);
    Desired.pitchTorqueNm = PitchRateGainNewtonMeterSeconds *
                            (DesiredBodyRate.Y - Frame.AngularVelocityBodyRadiansPerSecond.Y);
    Desired.yawTorqueNm = YawRateGainNewtonMeterSeconds *
                          (DesiredBodyRate.Z - Frame.AngularVelocityBodyRadiansPerSecond.Z);

    if (MotorState != EMotorState::Armed || bTakeoffRequired ||
        (IsManualMode() && PilotControls->Throttle <= 0.001F))
    {
        Desired = Aether::DesiredWrench{};
    }
    Aether::MotorThrusts TargetThrusts{};
    if (!Aether::QuadrotorDynamics::AllocateMotorThrusts(Desired, Frame.DynamicsParameters,
                                                         TargetThrusts))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid quadrotor parameters or desired motor wrench"));
        return;
    }

    bMotorSaturated = false;
    for (float Thrust : TargetThrusts)
    {
        if (Thrust >= MaximumMotorThrustNewtons - 0.001F)
        {
            bMotorSaturated = true;
        }
    }
    Aether::MotorThrusts UpdatedThrusts{};
    if (!Aether::QuadrotorDynamics::UpdateMotorThrusts(MotorThrustsNewtons, TargetThrusts,
                                                       DeltaSeconds, Frame.DynamicsParameters,
                                                       UpdatedThrusts))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid quadrotor motor state or timestep"));
        return;
    }
    MotorThrustsNewtons = UpdatedThrusts;

    for (int32 Motor = 0; Motor < 4; ++Motor)
    {
        const FVector LocalPosition = Aether::QuadrotorLayout::MotorArmUnitOffsets[Motor] *
                                      ArmLengthMeters * Aether::Units::CentimetersPerMeter;
        const FVector WorldPosition =
            Body->GetComponentTransform().TransformPosition(LocalPosition);
        const FVector WorldThrust = Frame.BodyRotation.RotateVector(FVector(
            0.0, 0.0, MotorThrustsNewtons[Motor] * Aether::Units::ChaosForceUnitsPerNewton));
        Body->AddForceAtLocation(WorldThrust, WorldPosition);

        const FVector WorldYawTorque = Frame.BodyRotation.RotateVector(FVector(
            0.0, 0.0,
            Aether::QuadrotorLayout::RotorYawSigns[Motor] * YawMomentPerNewtonNewtonMeters *
                MotorThrustsNewtons[Motor] * Aether::Units::ChaosTorqueUnitsPerNewtonMeter));
        Body->AddTorqueInRadians(WorldYawTorque);

        float ThrustRatio = 0.0F;
        if (MaximumMotorThrustNewtons > 0.0F)
        {
            ThrustRatio =
                FMath::Clamp(MotorThrustsNewtons[Motor] / MaximumMotorThrustNewtons, 0.0F, 1.0F);
        }
        const float RotorSpeedDegreesPerSecond =
            MaximumVisualRotorRpm * 6.0F * FMath::Sqrt(ThrustRatio);
        for (int32 Blade = 0; Blade < 2; ++Blade)
        {
            const int32 BladeIndex = Motor * 2 + Blade;
            if (PropellerBlades.IsValidIndex(BladeIndex))
            {
                PropellerBlades[BladeIndex]->AddLocalRotation(
                    FRotator(0.0,
                             Aether::QuadrotorLayout::RotorYawSigns[Motor] *
                                 RotorSpeedDegreesPerSecond * DeltaSeconds,
                             0.0));
            }
        }
    }
}
