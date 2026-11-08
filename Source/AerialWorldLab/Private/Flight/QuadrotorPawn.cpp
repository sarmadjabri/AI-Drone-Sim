#include "Flight/QuadrotorPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Flight/FlightControl.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr float MetersToCentimeters = 100.0F;
    constexpr float NewtonMetersToChaosTorque = 10000.0F;

    const FVector MotorPositionsMeters[4] = {
        FVector(1.0, 0.0, 0.0),
        FVector(0.0, 1.0, 0.0),
        FVector(-1.0, 0.0, 0.0),
        FVector(0.0, -1.0, 0.0)
    };

    const float RotorYawSigns[4] = { 1.0F, -1.0F, 1.0F, -1.0F };
    const TCHAR* F450PartNames[] = {
        TEXT("Arm1"), TEXT("Arm2"), TEXT("Arm3"), TEXT("Arm4"),
        TEXT("Battery"), TEXT("Body7"), TEXT("Bottom"), TEXT("Leg1"),
        TEXT("Leg2"), TEXT("Leg3"), TEXT("Leg4"), TEXT("Motor1"),
        TEXT("Motor2"), TEXT("Motor3"), TEXT("Motor4"), TEXT("Top")
    };
    constexpr float F450ModelOriginOffsetCentimeters = -11.0F;
    constexpr int32 MotorAssetIndices[4] = { 14, 11, 12, 13 };
}

AQuadrotorPawn::AQuadrotorPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    AutoPossessPlayer = EAutoReceiveInput::Player0;

    Body = CreateDefaultSubobject<UBoxComponent>(TEXT("PhysicsBody"));
    SetRootComponent(Body);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    TArray<UStaticMeshComponent*> PrimitiveDroneParts;
    Body->SetBoxExtent(FVector(18.0, 13.0, 5.0));
    Body->SetCollisionProfileName(TEXT("PhysicsActor"));
    Body->SetSimulatePhysics(true);
    Body->SetEnableGravity(true);
    Body->SetNotifyRigidBodyCollision(true);
    Body->BodyInstance.bUseCCD = true;
    Body->SetLinearDamping(0.05F);
    Body->SetAngularDamping(0.1F);

    Fuselage = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Fuselage"));
    Fuselage->SetupAttachment(Body);
    if (CubeMesh.Succeeded())
    {
        Fuselage->SetStaticMesh(CubeMesh.Object);
    }
    Fuselage->SetRelativeScale3D(FVector(0.30, 0.22, 0.075));
    Fuselage->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PrimitiveDroneParts.Add(Fuselage);

    UStaticMeshComponent* ArmX = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArmX"));
    ArmX->SetupAttachment(Body);
    if (CubeMesh.Succeeded())
    {
        ArmX->SetStaticMesh(CubeMesh.Object);
    }
    ArmX->SetRelativeScale3D(FVector(0.50, 0.035, 0.02));
    ArmX->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PrimitiveDroneParts.Add(ArmX);

    UStaticMeshComponent* ArmY = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArmY"));
    ArmY->SetupAttachment(Body);
    if (CubeMesh.Succeeded())
    {
        ArmY->SetStaticMesh(CubeMesh.Object);
    }
    ArmY->SetRelativeScale3D(FVector(0.035, 0.50, 0.02));
    ArmY->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PrimitiveDroneParts.Add(ArmY);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (ShapeMaterial.Succeeded())
    {
        DroneMaterial = ShapeMaterial.Object;
    }

    ApplyTint(Fuselage, FLinearColor(0.055F, 0.075F, 0.095F));

    NoseCamera = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrontCameraHousing"));
    NoseCamera->SetupAttachment(Body);
    if (SphereMesh.Succeeded())
    {
        NoseCamera->SetStaticMesh(SphereMesh.Object);
    }
    NoseCamera->SetRelativeLocation(FVector(18.0, 0.0, 2.0));
    NoseCamera->SetRelativeScale3D(FVector(0.11, 0.095, 0.075));
    NoseCamera->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ApplyTint(NoseCamera, FLinearColor(0.92F, 0.30F, 0.055F));

    CameraLens = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CameraLens"));
    CameraLens->SetupAttachment(NoseCamera);
    if (SphereMesh.Succeeded())
    {
        CameraLens->SetStaticMesh(SphereMesh.Object);
    }
    CameraLens->SetRelativeLocation(FVector(0.78, 0.0, 0.0));
    CameraLens->SetRelativeScale3D(FVector(0.36, 0.42, 0.42));
    CameraLens->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ApplyTint(CameraLens, FLinearColor(0.025F, 0.32F, 0.42F));

    for (int32 Side = -1; Side <= 1; Side += 2)
    {
        UStaticMeshComponent* LandingLeg = CreateDefaultSubobject<UStaticMeshComponent>(
            *FString::Printf(TEXT("LandingLeg%d"), Side));
        LandingLeg->SetupAttachment(Body);
        if (CubeMesh.Succeeded())
        {
            LandingLeg->SetStaticMesh(CubeMesh.Object);
        }
        LandingLeg->SetRelativeLocation(FVector(0.0, Side * 16.0, -11.0));
        LandingLeg->SetRelativeScale3D(FVector(0.055, 0.018, 0.11));
        LandingLeg->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ApplyTint(LandingLeg, FLinearColor(0.10F, 0.12F, 0.14F));
        PrimitiveDroneParts.Add(LandingLeg);
    }

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(Body);
    CameraBoom->TargetArmLength = 550.0F;
    CameraBoom->SetRelativeLocation(FVector(0.0, 0.0, 55.0));
    CameraBoom->SetRelativeRotation(FRotator(-12.0, 0.0, 0.0));
    CameraBoom->bDoCollisionTest = true;
    CameraBoom->bInheritPitch = false;
    CameraBoom->bInheritRoll = false;
    CameraBoom->bInheritYaw = true;
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = ThirdPersonCameraPositionLagSpeed;
    CameraBoom->bEnableCameraRotationLag = true;
    CameraBoom->CameraRotationLagSpeed = ThirdPersonCameraRotationLagSpeed;
    CameraBoom->bUseCameraLagSubstepping = true;
    CameraBoom->CameraLagMaxTimeStep = 1.0F / 120.0F;
    CameraBoom->PrimaryComponentTick.TickGroup = TG_PostPhysics;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("DroneCamera"));
    Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;

    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(Body);
    FirstPersonCamera->SetFieldOfView(95.0F);
    FirstPersonCamera->bUsePawnControlRotation = false;
    FirstPersonCamera->SetAutoActivate(false);

    FirstPersonCameraController =
        CreateDefaultSubobject<UDroneCameraControllerComponent>(TEXT("FirstPersonCameraController"));
    FirstPersonCameraController->Configure(Body, FirstPersonCamera);

    for (int32 Motor = 0; Motor < 4; ++Motor)
    {
        UStaticMeshComponent* Rotor = CreateDefaultSubobject<UStaticMeshComponent>(
            *FString::Printf(TEXT("MotorHousing%d"), Motor));
        Rotor->SetupAttachment(Body);
        if (SphereMesh.Succeeded())
        {
            Rotor->SetStaticMesh(SphereMesh.Object);
        }
        Rotor->SetRelativeScale3D(FVector(0.07, 0.07, 0.045));
        Rotor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ApplyTint(Rotor, FLinearColor(0.92F, 0.30F, 0.055F));
        Rotors.Add(Rotor);
        PrimitiveDroneParts.Add(Rotor);

        for (int32 Blade = 0; Blade < 2; ++Blade)
        {
            UStaticMeshComponent* Propeller = CreateDefaultSubobject<UStaticMeshComponent>(
                *FString::Printf(TEXT("Propeller%dBlade%d"), Motor, Blade));
            Propeller->SetupAttachment(Body);
            if (CubeMesh.Succeeded())
            {
                Propeller->SetStaticMesh(CubeMesh.Object);
            }
            Propeller->SetRelativeScale3D(FVector(0.14, 0.018, 0.008));
            Propeller->SetRelativeRotation(FRotator(0.0, Blade * 90.0, 0.0));
            Propeller->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            ApplyTint(Propeller, FLinearColor(0.10F, 0.13F, 0.15F));
            PropellerBlades.Add(Propeller);
            PrimitiveDroneParts.Add(Propeller);
        }
    }

    ApplyTint(ArmX, FLinearColor(0.16F, 0.19F, 0.21F));
    ApplyTint(ArmY, FLinearColor(0.16F, 0.19F, 0.21F));

    TArray<UStaticMesh*> F450Parts;
    bool bF450ModelAvailable = true;
    for (const TCHAR* PartName : F450PartNames)
    {
        const FString AssetPath = FString::Printf(
            TEXT("/Game/External/F450Drone/%s.%s"), PartName, PartName);
        UStaticMesh* PartMesh = LoadObject<UStaticMesh>(nullptr, *AssetPath);
        if (!PartMesh)
        {
            UE_LOG(LogTemp, Error, TEXT("Cannot load F450 drone mesh: %s"), *AssetPath);
            bF450ModelAvailable = false;
            break;
        }
        F450Parts.Add(PartMesh);
    }

    if (bF450ModelAvailable)
    {
        for (int32 PartIndex = 0; PartIndex < F450Parts.Num(); ++PartIndex)
        {
            const FString ComponentName = FString::Printf(
                TEXT("ImportedF450%s"), F450PartNames[PartIndex]);
            UStaticMeshComponent* Part =
                CreateDefaultSubobject<UStaticMeshComponent>(*ComponentName);
            Part->SetupAttachment(Body);
            Part->SetStaticMesh(F450Parts[PartIndex]);
            Part->SetRelativeLocation(FVector(0.0, 0.0, F450ModelOriginOffsetCentimeters));
            Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            ImportedDroneParts.Add(Part);
        }

        for (UStaticMeshComponent* PrimitivePart : PrimitiveDroneParts)
        {
            PrimitivePart->SetVisibility(false, true);
            PrimitivePart->SetHiddenInGame(true, true);
            PrimitivePart->SetCastShadow(false);
        }

        Rotors.Empty();
        PropellerBlades.Empty();
        for (int32 Motor = 0; Motor < UE_ARRAY_COUNT(MotorAssetIndices); ++Motor)
        {
            const FBox MotorBounds = F450Parts[MotorAssetIndices[Motor]]->GetBoundingBox();
            const FVector MotorLocation(
                MotorBounds.GetCenter().X,
                MotorBounds.GetCenter().Y,
                MotorBounds.Max.Z + 1.0F + F450ModelOriginOffsetCentimeters);

            for (int32 Blade = 0; Blade < 2; ++Blade)
            {
                UStaticMeshComponent* Propeller = CreateDefaultSubobject<UStaticMeshComponent>(
                    *FString::Printf(TEXT("F450Propeller%dBlade%d"), Motor, Blade));
                Propeller->SetupAttachment(Body);
                if (CubeMesh.Succeeded())
                {
                    Propeller->SetStaticMesh(CubeMesh.Object);
                }
                Propeller->SetRelativeLocation(MotorLocation);
                Propeller->SetRelativeScale3D(FVector(0.14, 0.018, 0.008));
                Propeller->SetRelativeRotation(FRotator(0.0, Blade * 90.0, 0.0));
                Propeller->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                ApplyTint(Propeller, FLinearColor(0.10F, 0.13F, 0.15F));
                PropellerBlades.Add(Propeller);
            }
        }
    }
}

void AQuadrotorPawn::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Camera->SetFieldOfView(ThirdPersonFieldOfView);
    ApplyViewMode();
    for (int32 Motor = 0; Motor < Rotors.Num() && Motor < 4; ++Motor)
    {
        const FVector MotorLocation =
            MotorPositionsMeters[Motor] * ArmLengthMeters * MetersToCentimeters;
        Rotors[Motor]->SetRelativeLocation(MotorLocation);
        for (int32 Blade = 0; Blade < 2; ++Blade)
        {
            const int32 BladeIndex = Motor * 2 + Blade;
            if (PropellerBlades.IsValidIndex(BladeIndex))
            {
                PropellerBlades[BladeIndex]->SetRelativeLocation(
                    MotorLocation + FVector(0.0, 0.0, 3.0));
            }
        }
    }
}

void AQuadrotorPawn::BeginPlay()
{
    Super::BeginPlay();
    CameraBoom->CameraLagSpeed = ThirdPersonCameraPositionLagSpeed;
    CameraBoom->CameraRotationLagSpeed = ThirdPersonCameraRotationLagSpeed;
    ApplyViewMode();
    Body->SetMassOverrideInKg(NAME_None, MassKg, true);
    // Chaos InertiaTensorScale rescales an equivalent shape, not tensor components.
    // Write the desired principal inertia directly, in kg*cm^2, under the physics lock.
    const FVector TargetInertia(InertiaXKgMetersSquared * 10000.0F,
        InertiaYKgMetersSquared * 10000.0F, InertiaZKgMetersSquared * 10000.0F);
    FPhysicsCommand::ExecuteWrite(Body->BodyInstance.ActorHandle,
        [TargetInertia](FPhysicsActorHandle Actor)
        {
            FPhysicsInterface::SetMassSpaceInertiaTensor_AssumesLocked(Actor, TargetInertia);
        });
    InitialDroneTransform = GetActorTransform();
    HoldPositionCentimeters = GetActorLocation();
    bHoldingPosition = true;
    CollectiveCommandNewtons = MassKg * GravityMetersPerSecondSquared;
    AltitudeTargetMeters = InitialDroneTransform.GetLocation().Z / MetersToCentimeters;
    YawTargetDegrees = InitialDroneTransform.Rotator().Yaw;
    MotorThrustsNewtons.fill(CollectiveCommandNewtons * 0.25F);
    if (CollectiveCommandNewtons > 4.0F * MaximumMotorThrustNewtons)
    {
        UE_LOG(LogTemp, Warning, TEXT("Configured maximum thrust is below hover thrust"));
    }
}

void AQuadrotorPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Body->IsSimulatingPhysics() || DeltaSeconds <= 0.0F)
    {
        return;
    }

    if (bUseMappedInput)
    {
        UpdatePilotInput();
    }
    Aether::DroneParameters Parameters;
    Parameters.massKg = MassKg;
    Parameters.armLengthM = ArmLengthMeters;
    Parameters.maxMotorThrustN = MaximumMotorThrustNewtons;
    Parameters.motorTimeConstantS = MotorTimeConstantSeconds;
    Parameters.yawMomentPerNewtonNm = YawMomentPerNewtonNewtonMeters;

    // Bound command integration across render hitches. Chaos owns physics time.
    DeltaSeconds = FMath::Min(DeltaSeconds, 1.0F / 30.0F);
    // A true slew bound, with separately tunable release/reversal speed.
    const auto SlewInput = [this, DeltaSeconds](float Current, float Target)
    {
        const bool bReleasing = Current * Target < 0.0F || FMath::Abs(Target) < FMath::Abs(Current);
        return Aether::SlewTowards(Current, Target,
            StickResponseSpeed * (bReleasing ? StickReleaseMultiplier : 1.0F), DeltaSeconds);
    };
    LateralInput = SlewInput(LateralInput, LateralInputTarget);
    ForwardInput = SlewInput(ForwardInput, ForwardInputTarget);
    YawInput = SlewInput(YawInput, YawInputTarget);
    ThrottleInput = SlewInput(ThrottleInput, ThrottleInputTarget);
    CameraBoom->TargetArmLength = FMath::Clamp(
        CameraBoom->TargetArmLength - (IsFirstPerson() ? 0.0F : CameraZoomInput) * CameraZoomSpeed * DeltaSeconds,
        300.0F,
        900.0F);

    const FRotator CurrentRotation = Body->GetComponentRotation();
    const FQuat BodyRotation = Body->GetComponentQuat();
    const FVector AngularVelocityBody = BodyRotation.UnrotateVector(
        Body->GetPhysicsAngularVelocityInRadians());

    const float HeadingRadians = FMath::DegreesToRadians(CurrentRotation.Yaw);
    const FVector ForwardDirection(FMath::Cos(HeadingRadians), FMath::Sin(HeadingRadians), 0.0F);
    const FVector RightDirection(-FMath::Sin(HeadingRadians), FMath::Cos(HeadingRadians), 0.0F);
    FVector DesiredVelocity =
        (ForwardDirection * ForwardInput + RightDirection * LateralInput).GetClampedToMaxSize(1.0F) * MaximumHorizontalSpeedMetersPerSecond;
    FVector HorizontalVelocity = GetVelocityMetersPerSecond();
    HorizontalVelocity.Z = 0.0;
    // Decide pilot intent before expo/slew shaping: tiny shaped values are still commands.
    const float IntentDeadzone = FMath::Clamp(StickDeadzone, 0.0F, 0.5F);
    const bool bTranslationReleased = FMath::Abs(RawForwardInput) <= IntentDeadzone &&
        FMath::Abs(RawLateralInput) <= IntentDeadzone;
    TranslationNeutralSeconds = bTranslationReleased ? TranslationNeutralSeconds + DeltaSeconds : 0.0F;
    if (!bTranslationReleased)
    {
        bHoldingPosition = false;
    }
    else if (!bHoldingPosition && TranslationNeutralSeconds >= PositionHoldNeutralDelaySeconds &&
        HorizontalVelocity.Size() < PositionHoldCaptureSpeed &&
        BodyRotation.GetAxisZ().Z > FMath::Cos(FMath::DegreesToRadians(PositionHoldCaptureTiltDegrees)))
    {
        // Capture where braking finishes, never the release point behind the aircraft.
        HoldPositionCentimeters = GetActorLocation();
        bHoldingPosition = true;
    }
    if (bTranslationReleased && bHoldingPosition)
    {
        FVector PositionError = (HoldPositionCentimeters - GetActorLocation()) / MetersToCentimeters;
        PositionError.Z = 0.0;
        DesiredVelocity = (PositionError * PositionHoldGainPerSecond).GetClampedToMaxSize(PositionHoldMaximumSpeed);
    }

    const bool bBraking = FVector::DotProduct(DesiredVelocity - HorizontalVelocity, HorizontalVelocity) < 0.0;
    const float VelocityGain = bBraking ? BrakingVelocityGainPerSecond : HorizontalVelocityGainPerSecond;
    const float MaximumHorizontalAcceleration = GravityMetersPerSecondSquared *
        FMath::Tan(FMath::DegreesToRadians(bBraking ? MaximumBrakingTiltDegrees : MaximumTiltDegrees));
    const FVector AccelerationTarget = Aether::VelocityAcceleration(DesiredVelocity,
        HorizontalVelocity, BodyRotation.GetAxisZ(), GravityMetersPerSecondSquared,
        VelocityGain, MaximumHorizontalAcceleration, VelocityPredictionSeconds);
    // Limit jerk (m/s^3), not just joystick values. Braking gets a faster ramp.
    const float JerkLimit = bBraking ? BrakingJerkMetersPerSecondCubed : AccelerationJerkMetersPerSecondCubed;
    HorizontalAccelerationCommand += (AccelerationTarget - HorizontalAccelerationCommand)
        .GetClampedToMaxSize(JerkLimit * DeltaSeconds);
    HorizontalAccelerationCommand = HorizontalAccelerationCommand.GetClampedToMaxSize(MaximumHorizontalAcceleration);

    YawTargetDegrees = FRotator::NormalizeAxis(
        YawTargetDegrees + FMath::RadiansToDegrees(YawInput * MaximumYawRateRadiansPerSecond * DeltaSeconds));
    const FVector RotationError = Aether::AttitudeError(BodyRotation,
        HorizontalAccelerationCommand, GravityMetersPerSecondSquared, YawTargetDegrees);
    // Quaternion error is expressed in physics body axes; Euler roll/pitch signs are not.
    const FVector DesiredBodyRate(
        FMath::Clamp(RotationError.X * RollAttitudeGainPerSecond, -MaximumRollRateRadiansPerSecond, MaximumRollRateRadiansPerSecond),
        FMath::Clamp(RotationError.Y * PitchAttitudeGainPerSecond, -MaximumPitchRateRadiansPerSecond, MaximumPitchRateRadiansPerSecond),
        FMath::Clamp(RotationError.Z * YawHeadingGainPerSecond, -MaximumYawRateRadiansPerSecond, MaximumYawRateRadiansPerSecond));

    const float CurrentAltitudeMeters =
        Body->GetComponentLocation().Z / MetersToCentimeters;
    const float VerticalVelocityMetersPerSecond =
        Body->GetPhysicsLinearVelocity().Z / MetersToCentimeters;
    AltitudeTargetMeters = FMath::Clamp(
        AltitudeTargetMeters + ThrottleInput * VerticalSpeedMetersPerSecond * DeltaSeconds,
        MinimumAltitudeMeters,
        MaximumAltitudeMeters);
    const float AltitudeError = AltitudeTargetMeters - CurrentAltitudeMeters;
    const float DesiredVerticalAcceleration = FMath::Clamp(
        AltitudeError * AltitudePositionGainPerSecondSquared -
            VerticalVelocityMetersPerSecond * AltitudeVelocityGainPerSecond,
        -MaximumVerticalAcceleration,
        MaximumVerticalAcceleration);
    const float VerticalThrustFraction = FMath::Max(BodyRotation.GetAxisZ().Z, 0.7F);
    const float DesiredCollectiveThrust = MassKg *
        (GravityMetersPerSecondSquared + DesiredVerticalAcceleration) / VerticalThrustFraction;
    const float BoundedCollectiveTarget = FMath::Clamp(
        DesiredCollectiveThrust, 0.0F, 4.0F * MaximumMotorThrustNewtons);
    CollectiveCommandNewtons = Aether::SlewTowards(
        CollectiveCommandNewtons, BoundedCollectiveTarget,
        CollectiveThrustRampNewtonsPerSecond, DeltaSeconds);

    Aether::DesiredWrench Desired;
    Desired.collectiveThrustN = CollectiveCommandNewtons;
    Desired.rollTorqueNm = RollRateGainNewtonMeterSeconds * (DesiredBodyRate.X - AngularVelocityBody.X);
    Desired.pitchTorqueNm = PitchRateGainNewtonMeterSeconds * (DesiredBodyRate.Y - AngularVelocityBody.Y);
    Desired.yawTorqueNm = YawRateGainNewtonMeterSeconds * (DesiredBodyRate.Z - AngularVelocityBody.Z);

    Aether::MotorThrusts TargetThrusts{};
    if (!Aether::QuadrotorDynamics::AllocateMotorThrusts(Desired, Parameters, TargetThrusts))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid quadrotor parameters or desired motor wrench"));
        return;
    }

    Aether::MotorThrusts UpdatedThrusts{};
    if (!Aether::QuadrotorDynamics::UpdateMotorThrusts(
            MotorThrustsNewtons, TargetThrusts, DeltaSeconds, Parameters, UpdatedThrusts))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid quadrotor motor state or timestep"));
        return;
    }
    MotorThrustsNewtons = UpdatedThrusts;

    for (int32 Motor = 0; Motor < 4; ++Motor)
    {
        const FVector LocalPosition = MotorPositionsMeters[Motor] *
            ArmLengthMeters * MetersToCentimeters;
        const FVector WorldPosition = Body->GetComponentTransform().TransformPosition(LocalPosition);
        const FVector WorldThrust = BodyRotation.RotateVector(
            FVector(0.0, 0.0, MotorThrustsNewtons[Motor] * MetersToCentimeters));
        Body->AddForceAtLocation(WorldThrust, WorldPosition);

        const FVector WorldYawTorque = BodyRotation.RotateVector(FVector(
            0.0,
            0.0,
            RotorYawSigns[Motor] * YawMomentPerNewtonNewtonMeters *
                MotorThrustsNewtons[Motor] * NewtonMetersToChaosTorque));
        Body->AddTorqueInRadians(WorldYawTorque);

        const float ThrustRatio = MaximumMotorThrustNewtons > 0.0F
            ? FMath::Clamp(MotorThrustsNewtons[Motor] / MaximumMotorThrustNewtons, 0.0F, 1.0F)
            : 0.0F;
        const float RotorSpeedDegreesPerSecond =
            MaximumVisualRotorRpm * 6.0F * FMath::Sqrt(ThrustRatio);
        for (int32 Blade = 0; Blade < 2; ++Blade)
        {
            const int32 BladeIndex = Motor * 2 + Blade;
            if (PropellerBlades.IsValidIndex(BladeIndex))
            {
                PropellerBlades[BladeIndex]->AddLocalRotation(FRotator(
                    0.0,
                    RotorYawSigns[Motor] * RotorSpeedDegreesPerSecond * DeltaSeconds,
                    0.0));
            }
        }
    }
}

void AQuadrotorPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    check(PlayerInputComponent);
    bUseMappedInput = true;
    PlayerInputComponent->BindAxis(TEXT("PadLeftX"), this, &AQuadrotorPawn::SetPadLeftX);
    PlayerInputComponent->BindAxis(TEXT("PadLeftY"), this, &AQuadrotorPawn::SetPadLeftY);
    PlayerInputComponent->BindAxis(TEXT("PadRightX"), this, &AQuadrotorPawn::SetPadRightX);
    PlayerInputComponent->BindAxis(TEXT("PadRightY"), this, &AQuadrotorPawn::SetPadRightY);
    PlayerInputComponent->BindAxis(TEXT("PadTriggers"), this, &AQuadrotorPawn::SetPadTriggers);
    PlayerInputComponent->BindAxis(TEXT("FirstPersonThrottle"), this, &AQuadrotorPawn::SetFirstPersonKeyboardThrottle);
    PlayerInputComponent->BindAction(TEXT("ToggleView"), IE_Pressed, this, &AQuadrotorPawn::ToggleViewMode);
    PlayerInputComponent->BindAxis(TEXT("DroneStrafe"), this, &AQuadrotorPawn::SetKeyboardLateral);
    PlayerInputComponent->BindAxis(TEXT("DroneForward"), this, &AQuadrotorPawn::SetKeyboardForward);
    PlayerInputComponent->BindAxis(TEXT("DroneYaw"), this, &AQuadrotorPawn::SetKeyboardYaw);
    PlayerInputComponent->BindAxis(TEXT("DroneThrottle"), this, &AQuadrotorPawn::SetKeyboardThrottle);
    PlayerInputComponent->BindAxis(TEXT("CameraZoom"), this, &AQuadrotorPawn::SetCameraZoomInput);
    PlayerInputComponent->BindAction(
        TEXT("ResetDrone"), IE_Pressed, this, &AQuadrotorPawn::ResetDrone);
}

FVector AQuadrotorPawn::GetVelocityMetersPerSecond() const
{
    return Body->GetPhysicsLinearVelocity() / MetersToCentimeters;
}

float AQuadrotorPawn::GetMotorThrustNewtons(int32 MotorIndex) const
{
    return MotorIndex >= 0 && MotorIndex < static_cast<int32>(MotorThrustsNewtons.size())
        ? MotorThrustsNewtons[MotorIndex]
        : 0.0F;
}

float AQuadrotorPawn::GetCollectiveThrustNewtons() const
{
    float Total = 0.0F;
    for (const float Thrust : MotorThrustsNewtons)
    {
        Total += Thrust;
    }
    return Total;
}

float AQuadrotorPawn::GetMaximumCollectiveThrustNewtons() const
{
    return 4.0F * MaximumMotorThrustNewtons;
}

void AQuadrotorPawn::SetCameraZoomInput(float Value)
{
    CameraZoomInput = FMath::Clamp(Value, -1.0F, 1.0F);
}

void AQuadrotorPawn::ResetDrone()
{
    if (!Body->IsSimulatingPhysics())
    {
        return;
    }

    SetActorTransform(InitialDroneTransform, false, nullptr, ETeleportType::TeleportPhysics);
    Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
    Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    Body->WakeAllRigidBodies();
    RawForwardInput = RawLateralInput = TranslationNeutralSeconds = 0.0F;
    LateralInput = LateralInputTarget = 0.0F;
    ForwardInput = ForwardInputTarget = 0.0F;
    YawInput = YawInputTarget = 0.0F;
    ThrottleInput = ThrottleInputTarget = 0.0F;
    CameraZoomInput = 0.0F;
    HorizontalAccelerationCommand = FVector::ZeroVector;
    FirstPersonCameraController->ResetCameraPose();
    HoldPositionCentimeters = InitialDroneTransform.GetLocation();
    bHoldingPosition = true;
    CollectiveCommandNewtons = MassKg * GravityMetersPerSecondSquared;
    MotorThrustsNewtons.fill(CollectiveCommandNewtons * 0.25F);
    AltitudeTargetMeters = InitialDroneTransform.GetLocation().Z / MetersToCentimeters;
    YawTargetDegrees = InitialDroneTransform.Rotator().Yaw;
    UE_LOG(LogTemp, Display, TEXT("Drone reset to the launch point"));
}

void AQuadrotorPawn::ApplyTint(UPrimitiveComponent* Component, const FLinearColor& Color) const
{
    if (!Component || !DroneMaterial)
    {
        return;
    }
    UMaterialInstanceDynamic* Material =
        Component->CreateDynamicMaterialInstance(0, DroneMaterial);
    if (Material)
    {
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Component->SetMaterial(0, Material);
    }
}

float AQuadrotorPawn::ApplyDeadzone(float Value) const
{
    const float Deadzone = FMath::Clamp(StickDeadzone, 0.0F, 0.5F);
    Value = FMath::Clamp(Value, -1.0F, 1.0F);
    const float Magnitude = FMath::Abs(Value);
    if (Magnitude <= Deadzone)
    {
        return 0.0F;
    }
    const float Rescaled = (Magnitude - Deadzone) / (1.0F - Deadzone);
    const float Expo = (1.0F - FMath::Clamp(StickExpo, 0.0F, 1.0F)) * Rescaled + FMath::Clamp(StickExpo, 0.0F, 1.0F) * Rescaled * Rescaled * Rescaled;
    return FMath::Sign(Value) * Expo;
}

void AQuadrotorPawn::SetLateralInput(float Value)
{
    RawLateralInput = FMath::Clamp(Value, -1.0F, 1.0F);
    LateralInputTarget = ApplyDeadzone(RawLateralInput);
    if (FMath::Abs(RawLateralInput) > FMath::Clamp(StickDeadzone, 0.0F, 0.5F))
    {
        bHoldingPosition = false;
        TranslationNeutralSeconds = 0.0F;
    }
}

void AQuadrotorPawn::SetForwardInput(float Value)
{
    RawForwardInput = FMath::Clamp(Value, -1.0F, 1.0F);
    ForwardInputTarget = ApplyDeadzone(RawForwardInput);
    if (FMath::Abs(RawForwardInput) > FMath::Clamp(StickDeadzone, 0.0F, 0.5F))
    {
        bHoldingPosition = false;
        TranslationNeutralSeconds = 0.0F;
    }
}

void AQuadrotorPawn::SetYawInput(float Value)
{
    YawInputTarget = ApplyDeadzone(Value);
}

void AQuadrotorPawn::SetThrottleInput(float Value)
{
    ThrottleInputTarget = FMath::Clamp(Value, -1.0F, 1.0F);
}

void AQuadrotorPawn::ApplyViewMode()
{
    Camera->SetActive(!IsFirstPerson());
    FirstPersonCamera->SetActive(IsFirstPerson());
    Camera->SetFieldOfView(ThirdPersonFieldOfView);
    FirstPersonCameraController->SetViewMode(ViewMode);
}

void AQuadrotorPawn::ToggleViewMode()
{
    switch (ViewMode)
    {
    case EDroneViewMode::ThirdPerson:
        ViewMode = EDroneViewMode::StabilizedFirstPerson;
        break;
    case EDroneViewMode::StabilizedFirstPerson:
        ViewMode = EDroneViewMode::BodyLockedFirstPerson;
        break;
    default:
        ViewMode = EDroneViewMode::ThirdPerson;
        break;
    }
    ApplyViewMode();
    // Do not reinterpret a held forward stick as a climb command during a view change.
    bAwaitingNeutralInput = true;
    SetForwardInput(0.0F);
    SetLateralInput(0.0F);
    SetYawInput(0.0F);
    SetThrottleInput(0.0F);
    ForwardInput = LateralInput = YawInput = ThrottleInput = 0.0F;
}

void AQuadrotorPawn::UpdatePilotInput()
{
    if (bAwaitingNeutralInput)
    {
        const float Deadzone = FMath::Clamp(StickDeadzone, 0.0F, 0.5F);
        const bool bNeutral = FMath::Abs(PadLeftX) <= Deadzone && FMath::Abs(PadLeftY) <= Deadzone &&
            FMath::Abs(PadRightX) <= Deadzone && FMath::Abs(PadRightY) <= Deadzone &&
            FMath::Abs(PadTriggers) <= Deadzone && FMath::IsNearlyZero(KeyboardForward) &&
            FMath::IsNearlyZero(KeyboardLateral) && FMath::IsNearlyZero(KeyboardYaw) &&
            FMath::IsNearlyZero(KeyboardThrottle) && FMath::IsNearlyZero(FirstPersonKeyboardThrottle);
        if (!bNeutral)
        {
            return;
        }
        bAwaitingNeutralInput = false;
    }
    SetForwardInput(KeyboardForward + (IsFirstPerson() ? PadRightY : PadLeftY));
    SetLateralInput(KeyboardLateral + (IsFirstPerson() ? PadRightX : PadLeftX));
    SetYawInput(KeyboardYaw + (IsFirstPerson() ? PadLeftX : PadRightX));
    SetThrottleInput(IsFirstPerson() ? FirstPersonKeyboardThrottle + ApplyDeadzone(PadLeftY)
                                    : KeyboardThrottle + PadTriggers);
}
