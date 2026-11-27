// Neutral-input gates, grounded checks and deliberate motor/mode transitions.
#include "Flight/QuadrotorPawn.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Flight/SimulationUnits.h"

void AQuadrotorPawn::UpdateResetHold(float DeltaSeconds)
{
    if (!bResetHeld)
    {
        return;
    }
    ResetHeldSeconds += FMath::Max(0.0F, DeltaSeconds);
    if (ResetHeldSeconds >= FMath::Max(ResetHoldSeconds, 0.25F))
    {
        EndResetHold();
        ResetDrone();
        RequireNeutralInput();
    }
}

bool AQuadrotorPawn::IsGrounded() const
{
    if (!GetWorld() || GetVelocityMetersPerSecond().Size() > 0.7F || Body->GetUpVector().Z < 0.85F)
    {
        return false;
    }
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DroneGround), false, this);
    return GetWorld()->LineTraceSingleByChannel(Hit, Body->GetComponentLocation(),
                                                Body->GetComponentLocation() - FVector(0, 0, 12),
                                                ECC_Visibility, Query);
}

bool AQuadrotorPawn::RequestFlightMode(EDroneFlightMode NewMode)
{
    if (static_cast<int>(NewMode) >= Aether::FlightModeCount)
    {
        return false;
    }
    if (NewMode == FlightMode)
    {
        return true;
    }
    if (PilotControls->IsMenuOpen())
    {
        UpdatePilotInput();
    }
    const bool bRequestedModeIsManual =
        NewMode == EDroneFlightMode::Angle || NewMode == EDroneFlightMode::Acro;
    if (bRequestedModeIsManual)
    {
        float CurrentCollective = 0.0F;
        if (MotorState == EMotorState::Armed)
        {
            CurrentCollective = FMath::Clamp(
                CollectiveCommandNewtons / GetMaximumCollectiveThrustNewtons(), 0.0F, 1.0F);
        }
        bool bThrottleReady =
            FMath::Abs(PilotControls->Processed[Aether::CommandAxis::Vertical]) < 0.001F;
        if (PilotControls->CollectiveMode != ECollectiveMode::Incremental)
        {
            bThrottleReady =
                FMath::Abs((PilotControls->Processed[Aether::CommandAxis::Vertical] + 1) * 0.5F -
                           CurrentCollective) < 0.03F;
        }
        if (bControllerDisconnected ||
            FMath::Abs(PilotControls->Processed[Aether::CommandAxis::Forward]) > 0.001F ||
            FMath::Abs(PilotControls->Processed[Aether::CommandAxis::Lateral]) > 0.001F ||
            FMath::Abs(PilotControls->Processed[Aether::CommandAxis::Yaw]) > 0.001F ||
            !bThrottleReady || bLanding)
        {
            PilotControls->Status = TEXT("Manual entry refused: center rotation axes; center "
                                         "incremental throttle or match direct collective");
            return false;
        }
        PilotControls->Throttle = CurrentCollective;
    }
    else if (IsManualMode())
    {
        if (Body->GetUpVector().Z < 0.5F)
        {
            PilotControls->Status = TEXT("Level within 60 degrees before engaging assisted flight");
            return false;
        }
        HorizontalAccelerationCommand =
            Body->GetUpVector() * (GravityMetersPerSecondSquared / Body->GetUpVector().Z);
        HorizontalAccelerationCommand.Z = 0;
    }
    const bool bPreviousModeWasManual = IsManualMode();
    FlightMode = NewMode;
    if (bRequestedModeIsManual)
    {
        bTakeoffRequired = false;
    }
    AltitudeTargetMeters = Body->GetComponentLocation().Z / Aether::Units::CentimetersPerMeter;
    YawTargetDegrees = Body->GetComponentRotation().Yaw;
    HoldPositionCentimeters = GetActorLocation();
    bHoldingPosition = false;
    TranslationNeutralSeconds = 0;
    if (bRequestedModeIsManual || bPreviousModeWasManual)
    {
        RequireNeutralInput();
    }
    if (bRequestedModeIsManual)
    {
        PilotControls->Status = TEXT(
            "Manual control: no altitude/position hold; collective initialized to current thrust");
    }
    else
    {
        PilotControls->Status = TEXT("Assisted mode selected");
    }
    return true;
}

void AQuadrotorPawn::ToggleArm()
{
    if (MotorState == EMotorState::Armed)
    {
        if (!IsGrounded())
        {
            PilotControls->Status = TEXT("Disarm refused: aircraft must be grounded");
            return;
        }
        MotorState = EMotorState::Disarmed;
        PilotControls->Throttle = 0;
        PilotControls->Status = TEXT("DISARMED");
        return;
    }
    const bool bNeutral =
        FMath::Abs(PilotControls->Processed[Aether::CommandAxis::Forward]) < 0.001F &&
        FMath::Abs(PilotControls->Processed[Aether::CommandAxis::Lateral]) < 0.001F &&
        FMath::Abs(PilotControls->Processed[Aether::CommandAxis::Yaw]) < 0.001F;
    bool bThrottleSafe =
        FMath::Abs(PilotControls->Processed[Aether::CommandAxis::Vertical]) < 0.001F;
    if (IsManualMode() && PilotControls->CollectiveMode != ECollectiveMode::Incremental)
    {
        bThrottleSafe = PilotControls->Processed[Aether::CommandAxis::Vertical] < -0.95F;
    }
    if (bControllerDisconnected || !bNeutral || !bThrottleSafe || !IsGrounded() ||
        GetMaximumCollectiveThrustNewtons() <= MassKg * GravityMetersPerSecondSquared)
    {
        PilotControls->Status = TEXT("Arm refused: ground, neutral rotation axes, safe throttle "
                                     "and sufficient thrust required");
        return;
    }
    MotorState = EMotorState::Armed;
    bTakeoffRequired = !IsManualMode();
    PilotControls->Throttle = 0;
    CollectiveCommandNewtons = 0;
    AltitudeTargetMeters = Body->GetComponentLocation().Z / Aether::Units::CentimetersPerMeter;
    YawTargetDegrees = Body->GetComponentRotation().Yaw;
    bAwaitingNeutralInput = false;
    PilotControls->Status = TEXT("ARMED");
}

void AQuadrotorPawn::RequestTakeoff()
{
    if (IsManualMode())
    {
        PilotControls->Status = TEXT("Assisted takeoff unavailable in Angle/Acro");
        return;
    }
    if (MotorState != EMotorState::Armed)
    {
        PilotControls->Status = TEXT("Hold arm first, then request takeoff");
        return;
    }
    if (!IsGrounded())
    {
        PilotControls->Status = TEXT("Takeoff requires grounded aircraft");
        return;
    }
    bLanding = false;
    bTakeoffRequired = false;
    AltitudeTargetMeters =
        Body->GetComponentLocation().Z / Aether::Units::CentimetersPerMeter + 1.5F;
    PilotControls->Status = TEXT("Assisted takeoff to 1.5 m above launch");
}

void AQuadrotorPawn::RequestLanding()
{
    if (IsManualMode())
    {
        PilotControls->Status = TEXT("Controlled landing unavailable in Angle/Acro");
        return;
    }
    if (MotorState != EMotorState::Armed)
    {
        PilotControls->Status = TEXT("Landing requires an armed aircraft");
        return;
    }
    bLanding = true;
    RequireNeutralInput();
    PilotControls->Status = TEXT("Controlled descent; motors stop near ground");
}
