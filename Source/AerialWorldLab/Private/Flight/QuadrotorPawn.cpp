#include "Flight/QuadrotorPawn.h"

#include "Camera/CameraComponent.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
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
    PilotControls = CreateDefaultSubobject<UPilotControlsComponent>(TEXT("PilotControls"));

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
    FString ConfigError;
    if (!PilotControls->Validate(ConfigError)) PilotControls->RestoreDefaults();
    if (IsManualMode()) MotorState = EMotorState::Disarmed;
    ConnectionChangedHandle = IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddWeakLambda(this,
        [this](EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId Device)
        {
            const APlayerController* Player = Cast<APlayerController>(GetController());
            if (Player && Player->GetLocalPlayer() && Player->GetLocalPlayer()->GetPlatformUserId() == User)
            {
                HandleControllerConnection(State == EInputDeviceConnectionState::Connected);
            }
        });
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
    CollectiveCommandNewtons = MotorState==EMotorState::Armed ? MassKg * GravityMetersPerSecondSquared : 0;
    AltitudeTargetMeters = InitialDroneTransform.GetLocation().Z / MetersToCentimeters;
    YawTargetDegrees = InitialDroneTransform.Rotator().Yaw;
    MotorThrustsNewtons.fill(CollectiveCommandNewtons * 0.25F);
    if (CollectiveCommandNewtons > 4.0F * MaximumMotorThrustNewtons)
    {
        UE_LOG(LogTemp, Warning, TEXT("Configured maximum thrust is below hover thrust"));
    }
}

void AQuadrotorPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(ConnectionChangedHandle);
    Super::EndPlay(EndPlayReason);
}

void AQuadrotorPawn::HandleControllerConnection(bool bConnected)
{
    bControllerDisconnected = !bConnected;
    PadLeftX = PadLeftY = PadRightX = PadRightY = PadTriggers = PadLeftTrigger = PadRightTrigger = 0.0F;
    EndResetHold();
    bLanding=false;
    PilotControls->CancelHeld();
    if (!bConnected && IsManualMode())
    {
        MotorState = EMotorState::Failsafe;
        PilotControls->Throttle = 0;
        PilotControls->Status = TEXT("Manual failsafe: motors stopped; land/reset and deliberately re-arm");
    }
    RequireNeutralInput();
}

void AQuadrotorPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Body->IsSimulatingPhysics() || DeltaSeconds <= 0.0F)
    {
        return;
    }

    PilotControls->StepHolds(DeltaSeconds);
    UpdateResetHold(DeltaSeconds);
    if (bUseMappedInput)
    {
        UpdatePilotInput();
    }
    if (UsesMode2Axes())
    {
        float Left=PadLeftTrigger, Right=PadRightTrigger;
        if (InputProfile==EDroneInputProfile::Custom)
            for (const auto& Axis : PilotControls->Axes) { if (Axis.Source==4) Left=0; if (Axis.Source==5) Right=0; }
        const float Values[]={Left,Right};
        const ETriggerRole Roles[]={PilotControls->LeftTriggerRole,PilotControls->TriggerRole};
        for (int I=0;I<2;++I)
        {
            const float Auxiliary=Values[I]*(I==0 ? -1 : 1)*PilotControls->CameraSensitivity*FMath::Min(DeltaSeconds,0.05F);
            if (Roles[I]==ETriggerRole::Zoom) AdjustPilotCameraZoom(Auxiliary*6);
            else if (Roles[I]==ETriggerRole::Gimbal) FirstPersonCameraController->AdjustGimbalPitch(Auxiliary*45);
        }
    }
    Aether::DroneParameters Parameters;
    Parameters.massKg = MassKg;
    Parameters.armLengthM = ArmLengthMeters;
    Parameters.maxMotorThrustN = MaximumMotorThrustNewtons;
    Parameters.motorTimeConstantS = MotorTimeConstantSeconds;
    Parameters.yawMomentPerNewtonNm = YawMomentPerNewtonNewtonMeters;

    // Bound command integration across render hitches. Chaos owns physics time.
    DeltaSeconds = FMath::Min(DeltaSeconds, 1.0F / 30.0F);
    const bool bLegacy = FlightMode == EDroneFlightMode::LegacyAssisted;
    const FPilotModeTuning& Mode = PilotControls->Tuning(FlightMode);
    const float Precision = PilotControls->bPrecision ? PilotControls->PrecisionScale : 1.0F;
    // A true slew bound, with separately tunable release/reversal speed.
    const auto SlewInput = [this, DeltaSeconds, bLegacy, &Mode](float Current, float Target)
    {
        if (!bLegacy && Mode.Slew == 0) return Target;
        const bool bReleasing = Current * Target < 0.0F || FMath::Abs(Target) < FMath::Abs(Current);
        return Aether::SlewTowards(Current, Target,
            (bLegacy ? StickResponseSpeed : Mode.Slew) * (bReleasing ? StickReleaseMultiplier : 1.0F), DeltaSeconds);
    };
    LateralInput = SlewInput(LateralInput, LateralInputTarget);
    ForwardInput = SlewInput(ForwardInput, ForwardInputTarget);
    YawInput = SlewInput(YawInput, YawInputTarget);
    // Direct collective is absolute: slewing from neutral after matching would command 50% thrust.
    ThrottleInput = IsManualMode() && PilotControls->CollectiveMode==ECollectiveMode::Direct
        ? ThrottleInputTarget : SlewInput(ThrottleInput, ThrottleInputTarget);
    CameraBoom->TargetArmLength = FMath::Clamp(
        CameraBoom->TargetArmLength - (IsFirstPerson() ? 0.0F : CameraZoomInput) * CameraZoomSpeed * DeltaSeconds,
        300.0F,
        900.0F);

    const FRotator CurrentRotation = Body->GetComponentRotation();
    const FQuat BodyRotation = Body->GetComponentQuat();
    const FVector AngularVelocityBody = BodyRotation.UnrotateVector(
        Body->GetPhysicsAngularVelocityInRadians());

    FVector DesiredBodyRate = FVector::ZeroVector;
    if (!IsManualMode())
    {
    const float HeadingRadians = FMath::DegreesToRadians(CurrentRotation.Yaw);
    const FVector ForwardDirection(FMath::Cos(HeadingRadians), FMath::Sin(HeadingRadians), 0.0F);
    const FVector RightDirection(-FMath::Sin(HeadingRadians), FMath::Cos(HeadingRadians), 0.0F);
    FVector DesiredVelocity =
        (ForwardDirection * ForwardInput + RightDirection * LateralInput).GetClampedToMaxSize(1.0F) * (bLegacy ? MaximumHorizontalSpeedMetersPerSecond : Mode.Speed) * Precision;
    FVector HorizontalVelocity = GetVelocityMetersPerSecond();
    HorizontalVelocity.Z = 0.0;
    // Decide pilot intent before expo/slew shaping: tiny shaped values are still commands.
    const float IntentDeadzone = FMath::Clamp(StickDeadzone, 0.0F, 0.5F);
    const bool bTranslationReleased = bLegacy && !bUseMappedInput ? (FMath::Abs(RawForwardInput) <= IntentDeadzone &&
        FMath::Abs(RawLateralInput) <= IntentDeadzone) : (FMath::IsNearlyZero(ForwardInputTarget) && FMath::IsNearlyZero(LateralInputTarget));
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
        DesiredVelocity = (PositionError * (bLegacy ? PositionHoldGainPerSecond : Mode.HoldGain)).GetClampedToMaxSize(PositionHoldMaximumSpeed);
    }

    const bool bBraking = FVector::DotProduct(DesiredVelocity - HorizontalVelocity, HorizontalVelocity) < 0.0;
    const float VelocityGain = bBraking ? (bLegacy ? BrakingVelocityGainPerSecond : Mode.BrakeGain) : (bLegacy ? HorizontalVelocityGainPerSecond : Mode.VelocityGain);
    const float TiltAcceleration = GravityMetersPerSecondSquared *
        FMath::Tan(FMath::DegreesToRadians(bBraking ? (bLegacy ? MaximumBrakingTiltDegrees : Mode.BrakeTilt) : (bLegacy ? MaximumTiltDegrees : Mode.Tilt)));
    const float MaximumHorizontalAcceleration = bLegacy ? TiltAcceleration :
        FMath::Min(TiltAcceleration,bBraking ? Mode.Braking : Mode.Acceleration);
    const FVector AccelerationTarget = Aether::VelocityAcceleration(DesiredVelocity,
        HorizontalVelocity, BodyRotation.GetAxisZ(), GravityMetersPerSecondSquared,
        VelocityGain, MaximumHorizontalAcceleration, bLegacy ? VelocityPredictionSeconds : Mode.Prediction);
    // Limit jerk (m/s^3), not just joystick values. Braking gets a faster ramp.
    const float JerkLimit = bBraking ? (bLegacy ? BrakingJerkMetersPerSecondCubed : Mode.BrakeJerk) : (bLegacy ? AccelerationJerkMetersPerSecondCubed : Mode.Jerk);
    HorizontalAccelerationCommand += (AccelerationTarget - HorizontalAccelerationCommand)
        .GetClampedToMaxSize(JerkLimit * DeltaSeconds);
    // The target is bounded above. Preserve the jerk bound when switching to a lower tilt limit;
    // an immediate clamp here would introduce an acceleration discontinuity.

    YawTargetDegrees = FRotator::NormalizeAxis(
        YawTargetDegrees + FMath::RadiansToDegrees(YawInput * (bLegacy ? MaximumYawRateRadiansPerSecond : FMath::DegreesToRadians(Mode.YawDegrees) * Precision) * DeltaSeconds));
    const FVector RotationError = Aether::AttitudeError(BodyRotation,
        HorizontalAccelerationCommand, GravityMetersPerSecondSquared, YawTargetDegrees);
    // Quaternion error is expressed in physics body axes; Euler roll/pitch signs are not.
    DesiredBodyRate = FVector(
        FMath::Clamp(RotationError.X * (bLegacy ? RollAttitudeGainPerSecond : Mode.AttitudeGain), -(bLegacy ? MaximumRollRateRadiansPerSecond : FMath::DegreesToRadians(Mode.RateDegrees)), (bLegacy ? MaximumRollRateRadiansPerSecond : FMath::DegreesToRadians(Mode.RateDegrees))),
        FMath::Clamp(RotationError.Y * (bLegacy ? PitchAttitudeGainPerSecond : Mode.AttitudeGain), -(bLegacy ? MaximumPitchRateRadiansPerSecond : FMath::DegreesToRadians(Mode.RateDegrees)), (bLegacy ? MaximumPitchRateRadiansPerSecond : FMath::DegreesToRadians(Mode.RateDegrees))),
        FMath::Clamp(RotationError.Z * YawHeadingGainPerSecond, -(bLegacy ? MaximumYawRateRadiansPerSecond : FMath::DegreesToRadians(Mode.YawDegrees) * Precision), (bLegacy ? MaximumYawRateRadiansPerSecond : FMath::DegreesToRadians(Mode.YawDegrees) * Precision)));

    const float CurrentAltitudeMeters =
        Body->GetComponentLocation().Z / MetersToCentimeters;
    const float VerticalVelocityMetersPerSecond =
        Body->GetPhysicsLinearVelocity().Z / MetersToCentimeters;
    if (bLanding && IsGrounded())
    {
        MotorState = EMotorState::Disarmed;
        bLanding = false;
    }
    const float VerticalCommand = bLanding ? -0.5F : ThrottleInput;
    CommandedVelocity = DesiredVelocity;
    CommandedVelocity.Z = VerticalCommand * (bLegacy ? VerticalSpeedMetersPerSecond : (VerticalCommand >= 0 ? Mode.Ascent : Mode.Descent)) * Precision;
    AltitudeTargetMeters = FMath::Clamp(
        AltitudeTargetMeters + CommandedVelocity.Z * DeltaSeconds,
        bLanding ? -10000.0F : MinimumAltitudeMeters,
        MaximumAltitudeMeters);
    const float AltitudeError = AltitudeTargetMeters - CurrentAltitudeMeters;
    const float DesiredVerticalAcceleration = FMath::Clamp(
        AltitudeError * AltitudePositionGainPerSecondSquared -
            VerticalVelocityMetersPerSecond * AltitudeVelocityGainPerSecond,
        -(bLegacy ? MaximumVerticalAcceleration : Mode.VerticalAcceleration),
        bLegacy ? MaximumVerticalAcceleration : Mode.VerticalAcceleration);
    const float VerticalThrustFraction = FMath::Max(BodyRotation.GetAxisZ().Z, bLegacy ? 0.7F : 0.35F);
    const float DesiredCollectiveThrust = MassKg *
        (GravityMetersPerSecondSquared + DesiredVerticalAcceleration) / VerticalThrustFraction;
    const float BoundedCollectiveTarget = FMath::Clamp(
        DesiredCollectiveThrust, 0.0F, 4.0F * MaximumMotorThrustNewtons);
    CollectiveCommandNewtons = Aether::SlewTowards(
        CollectiveCommandNewtons, BoundedCollectiveTarget,
        CollectiveThrustRampNewtonsPerSecond, DeltaSeconds);

    }
    else
    {
        // These control laws never execute the velocity/position/altitude controllers.
        bHoldingPosition = false;
        HorizontalAccelerationCommand = FVector::ZeroVector;
        const float Rate = FMath::DegreesToRadians(Mode.RateDegrees) * Precision;
        DesiredBodyRate = FVector(-LateralInput * Rate, ForwardInput * Rate,
            YawInput * FMath::DegreesToRadians(Mode.YawDegrees) * Precision);
        if (FlightMode == EDroneFlightMode::Angle)
        {
            const float Heading = FMath::DegreesToRadians(CurrentRotation.Yaw);
            const FVector Forward(FMath::Cos(Heading), FMath::Sin(Heading), 0);
            const FVector Right(-FMath::Sin(Heading), FMath::Cos(Heading), 0);
            const FVector Tilt = (Forward * FMath::Tan(FMath::DegreesToRadians(ForwardInput * Mode.Tilt * Precision)) +
                Right * FMath::Tan(FMath::DegreesToRadians(LateralInput * Mode.Tilt * Precision))) * GravityMetersPerSecondSquared;
            const FVector Error = Aether::AttitudeError(BodyRotation, Tilt, GravityMetersPerSecondSquared, CurrentRotation.Yaw);
            DesiredBodyRate.X = FMath::Clamp(Error.X * Mode.AttitudeGain, -Rate, Rate);
            DesiredBodyRate.Y = FMath::Clamp(Error.Y * Mode.AttitudeGain, -Rate, Rate);
        }
        if (MotorState == EMotorState::Armed && !bAwaitingNeutralInput)
        {
            if (PilotControls->CollectiveMode == ECollectiveMode::Incremental)
                PilotControls->Throttle = FMath::Clamp(PilotControls->Throttle + ThrottleInput *
                    (ThrottleInput >= 0 ? PilotControls->ThrottleIncrease : PilotControls->ThrottleDecrease) * DeltaSeconds, 0.0F, 1.0F);
            else PilotControls->Throttle = FMath::Clamp((ThrottleInput + 1.0F) * 0.5F, 0.0F, 1.0F);
        }
        CollectiveCommandNewtons = Aether::SlewTowards(CollectiveCommandNewtons,
            PilotControls->Throttle * GetMaximumCollectiveThrustNewtons(), CollectiveThrustRampNewtonsPerSecond, DeltaSeconds);
    }
    CommandedBodyRate = DesiredBodyRate;
    if (MotorState != EMotorState::Armed)
    {
        CollectiveCommandNewtons = 0;
        DesiredBodyRate = FVector::ZeroVector;
    }

    Aether::DesiredWrench Desired;
    Desired.collectiveThrustN = CollectiveCommandNewtons;
    Desired.rollTorqueNm = RollRateGainNewtonMeterSeconds * (DesiredBodyRate.X - AngularVelocityBody.X);
    Desired.pitchTorqueNm = PitchRateGainNewtonMeterSeconds * (DesiredBodyRate.Y - AngularVelocityBody.Y);
    Desired.yawTorqueNm = YawRateGainNewtonMeterSeconds * (DesiredBodyRate.Z - AngularVelocityBody.Z);

    if (MotorState != EMotorState::Armed || bTakeoffRequired || (IsManualMode() && PilotControls->Throttle <= 0.001F))
        Desired = Aether::DesiredWrench{};
    Aether::MotorThrusts TargetThrusts{};
    if (!Aether::QuadrotorDynamics::AllocateMotorThrusts(Desired, Parameters, TargetThrusts))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid quadrotor parameters or desired motor wrench"));
        return;
    }

    bMotorSaturated = false;
    for (float Thrust : TargetThrusts) bMotorSaturated |= Thrust >= MaximumMotorThrustNewtons - 0.001F;
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
    MotorState=EMotorState::Disarmed;
    CollectiveCommandNewtons=0;
    MotorThrustsNewtons.fill(0);
    RequireNeutralInput();
    PlayerInputComponent->BindAxis(TEXT("PadLeftX"), this, &AQuadrotorPawn::SetPadLeftX);
    PlayerInputComponent->BindAxis(TEXT("PadLeftY"), this, &AQuadrotorPawn::SetPadLeftY);
    PlayerInputComponent->BindAxis(TEXT("PadRightX"), this, &AQuadrotorPawn::SetPadRightX);
    PlayerInputComponent->BindAxis(TEXT("PadRightY"), this, &AQuadrotorPawn::SetPadRightY);
    PlayerInputComponent->BindAxis(TEXT("PadTriggers"), this, &AQuadrotorPawn::SetPadTriggers);
    PlayerInputComponent->BindAxis(TEXT("FirstPersonThrottle"), this, &AQuadrotorPawn::SetFirstPersonKeyboardThrottle);

    PlayerInputComponent->BindAxis(TEXT("DroneStrafe"), this, &AQuadrotorPawn::SetKeyboardLateral);
    PlayerInputComponent->BindAxis(TEXT("DroneForward"), this, &AQuadrotorPawn::SetKeyboardForward);
    PlayerInputComponent->BindAxis(TEXT("DroneYaw"), this, &AQuadrotorPawn::SetKeyboardYaw);
    PlayerInputComponent->BindAxis(TEXT("DroneThrottle"), this, &AQuadrotorPawn::SetKeyboardThrottle);
    PlayerInputComponent->BindAxis(TEXT("PadLeftTrigger"), this, &AQuadrotorPawn::SetLeftTrigger);
    PlayerInputComponent->BindAxis(TEXT("PadRightTrigger"), this, &AQuadrotorPawn::SetRightTrigger);
    PilotControls->Bind(PlayerInputComponent);
    for (auto& Binding : PlayerInputComponent->AxisBindings) Binding.bExecuteWhenPaused=true;
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
    bLanding = false;
    MotorState = IsManualMode() || bUseMappedInput ? EMotorState::Disarmed : EMotorState::Armed;
    bTakeoffRequired=false;
    PilotControls->Throttle = 0;
    if (MotorState!=EMotorState::Armed) { CollectiveCommandNewtons = 0; MotorThrustsNewtons.fill(0); }
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
    LateralInputTarget = FlightMode == EDroneFlightMode::LegacyAssisted && !bUseMappedInput ? ApplyDeadzone(RawLateralInput) : PilotControls->Shape(1, RawLateralInput, FlightMode);
    if (FMath::Abs(RawLateralInput) > FMath::Clamp(StickDeadzone, 0.0F, 0.5F))
    {
        bHoldingPosition = false;
        TranslationNeutralSeconds = 0.0F;
    }
}

void AQuadrotorPawn::SetForwardInput(float Value)
{
    RawForwardInput = FMath::Clamp(Value, -1.0F, 1.0F);
    ForwardInputTarget = FlightMode == EDroneFlightMode::LegacyAssisted && !bUseMappedInput ? ApplyDeadzone(RawForwardInput) : PilotControls->Shape(0, RawForwardInput, FlightMode);
    if (FMath::Abs(RawForwardInput) > FMath::Clamp(StickDeadzone, 0.0F, 0.5F))
    {
        bHoldingPosition = false;
        TranslationNeutralSeconds = 0.0F;
    }
}

void AQuadrotorPawn::SetYawInput(float Value)
{
    YawInputTarget = FlightMode == EDroneFlightMode::LegacyAssisted && !bUseMappedInput ? ApplyDeadzone(Value) : PilotControls->Shape(2, Value, FlightMode);
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
    if (InputProfile == EDroneInputProfile::LegacyViewDependent)
    {
        RequireNeutralInput();
    }
}

void AQuadrotorPawn::RequireNeutralInput()
{
    bAwaitingNeutralInput = true;
    SetForwardInput(0.0F);
    SetLateralInput(0.0F);
    SetYawInput(0.0F);
    SetThrottleInput(0.0F);
    ForwardInput = LateralInput = YawInput = ThrottleInput = 0.0F;
}

void AQuadrotorPawn::UpdatePilotInput()
{
    if (bControllerDisconnected) PadLeftX = PadLeftY = PadRightX = PadRightY = PadTriggers = PadLeftTrigger = PadRightTrigger = 0;
    const float Physical[] = {PadLeftX,PadLeftY,PadRightX,PadRightY,PadLeftTrigger,PadRightTrigger};
    for (int I=0; I<6; ++I) PilotControls->Raw[I] = Physical[I];
    bool LeftAvailable=UsesMode2Axes(), RightAvailable=UsesMode2Axes();
    Aether::PilotAxes Axes = Aether::RoutePilotAxes(InputProfile, IsFirstPerson(), PadLeftX, PadLeftY, PadRightX, PadRightY, PadTriggers);
    if (InputProfile == EDroneInputProfile::Custom)
    {
        float Values[4];
        for (int I=0; I<4; ++I)
        {
            const int Source = PilotControls->Axes[I].Source;
            Values[I] = Source >= 4 ? Physical[Source] * 2.0F - 1.0F : Physical[Source];
        }
        Axes = {Values[0],Values[1],Values[2],Values[3]};
        for (const auto& Axis : PilotControls->Axes)
        { if (Axis.Source==4) LeftAvailable=false; if (Axis.Source==5) RightAvailable=false; }
    }
    PilotControls->UpdateAuxiliary(LeftAvailable,RightAvailable);

    const bool bLegacy = FlightMode == EDroneFlightMode::LegacyAssisted && !bUseMappedInput;
    SetForwardInput(Axes.Forward * (bInvertForward ? -1 : 1));
    SetLateralInput(Axes.Right * (bInvertLateral ? -1 : 1));
    SetYawInput(Axes.Yaw * (bInvertYaw ? -1 : 1));
    // Calibration and inversion describe hardware axes, never keyboard directions.
    ForwardInputTarget=FMath::Clamp(ForwardInputTarget+KeyboardForward,-1.0F,1.0F);
    LateralInputTarget=FMath::Clamp(LateralInputTarget+KeyboardLateral,-1.0F,1.0F);
    YawInputTarget=FMath::Clamp(YawInputTarget+KeyboardYaw,-1.0F,1.0F);
    float Vertical = bLegacy ? (UsesMode2Axes() ? ApplyDeadzone(Axes.Vertical) : Axes.Vertical) :
        PilotControls->Axes[3].Process(Axes.Vertical, IsManualMode() && PilotControls->CollectiveMode==ECollectiveMode::Direct ? 0.0F : PilotControls->Tuning(FlightMode).Expo);
    Vertical = FMath::Clamp((UsesMode2Axes() ? FirstPersonKeyboardThrottle : KeyboardThrottle) + Vertical * (bInvertVertical ? -1 : 1), -1.0F, 1.0F);
    SetThrottleInput(Vertical);
    PilotControls->Processed[0]=ForwardInputTarget; PilotControls->Processed[1]=LateralInputTarget;
    PilotControls->Processed[2]=YawInputTarget; PilotControls->Processed[3]=Vertical;
    if (PilotControls->IsMenuOpen())
    {
        ForwardInputTarget=LateralInputTarget=YawInputTarget=ThrottleInputTarget=0;
        return;
    }
    if (bAwaitingNeutralInput)
    {
        const bool bVerticalSafe = IsManualMode() && PilotControls->CollectiveMode==ECollectiveMode::Direct ?
            FMath::Abs((Vertical+1)*0.5F-PilotControls->Throttle)<0.03F : FMath::Abs(Vertical)<0.001F;
        const bool bNeutral = FMath::Abs(ForwardInputTarget)<0.001F && FMath::Abs(LateralInputTarget)<0.001F &&
            FMath::Abs(YawInputTarget)<0.001F && bVerticalSafe &&
            (InputProfile==EDroneInputProfile::Custom || (FMath::Abs(PadTriggers)<0.08F && FMath::Abs(KeyboardThrottle)<0.001F && FMath::Abs(FirstPersonKeyboardThrottle)<0.001F));
        if (!bNeutral)
        {
            ForwardInputTarget=LateralInputTarget=YawInputTarget=ThrottleInputTarget=0;
            return;
        }
        bAwaitingNeutralInput=false;
        if (IsManualMode() && PilotControls->CollectiveMode==ECollectiveMode::Direct)
            ThrottleInput=ThrottleInputTarget;
    }
}

bool AQuadrotorPawn::UsesMode2Axes() const
{
    return InputProfile == EDroneInputProfile::Custom || InputProfile == EDroneInputProfile::ProfessionalMode2 ||
        InputProfile == EDroneInputProfile::LegacyFPV ||
        (InputProfile == EDroneInputProfile::LegacyViewDependent && IsFirstPerson());
}

FString AQuadrotorPawn::GetInputProfileName() const
{
    switch (InputProfile)
    {
    case EDroneInputProfile::Custom: return TEXT("Custom");
    case EDroneInputProfile::Legacy3D: return TEXT("Legacy 3D");
    case EDroneInputProfile::LegacyFPV: return TEXT("Legacy FPV");
    case EDroneInputProfile::LegacyViewDependent: return TEXT("Legacy view-dependent");
    default: return TEXT("Professional Mode 2");
    }
}

FString AQuadrotorPawn::GetFlightModeName() const
{
    switch (FlightMode)
    {
    case EDroneFlightMode::HighPerformance: return TEXT("High Performance (assisted)");
    case EDroneFlightMode::Angle: return TEXT("Angle / collective");
    case EDroneFlightMode::Acro: return TEXT("Acro / body rate / collective");
    case EDroneFlightMode::Cinematic: return TEXT("Cinematic (assisted)");
    case EDroneFlightMode::Sport: return TEXT("Sport (assisted)");
    case EDroneFlightMode::Training: return TEXT("Training (assisted)");
    default: return TEXT("Legacy assisted");
    }
}

void AQuadrotorPawn::CycleInputProfile()
{
    if (IsManualMode() && MotorState != EMotorState::Disarmed)
    { PilotControls->Status=TEXT("Disarm before changing the manual control layout"); return; }
    InputProfile = static_cast<EDroneInputProfile>((static_cast<uint8>(InputProfile) + 1) % 5);
    RequireNeutralInput();
    SaveConfig();
}

void AQuadrotorPawn::CycleFlightMode()
{
    RequestFlightMode(static_cast<EDroneFlightMode>((static_cast<int>(FlightMode)+1)%7));
}

void AQuadrotorPawn::UpdateResetHold(float DeltaSeconds)
{
    if (!bResetHeld) return;
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
    if (!GetWorld() || GetVelocityMetersPerSecond().Size() > 0.7F || Body->GetUpVector().Z < 0.85F) return false;
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DroneGround),false,this);
    return GetWorld()->LineTraceSingleByChannel(Hit,Body->GetComponentLocation(),
        Body->GetComponentLocation()-FVector(0,0,12),ECC_Visibility,Query);
}
bool AQuadrotorPawn::RequestFlightMode(EDroneFlightMode NewMode)
{
    if (static_cast<int>(NewMode)>6) return false;
    if (NewMode == FlightMode) return true;
    if (PilotControls->IsMenuOpen()) UpdatePilotInput();
    const bool bNewManual = NewMode==EDroneFlightMode::Angle || NewMode==EDroneFlightMode::Acro;
    if (bNewManual)
    {
        const float CurrentCollective = MotorState==EMotorState::Armed ?
            FMath::Clamp(CollectiveCommandNewtons/GetMaximumCollectiveThrustNewtons(),0.0F,1.0F) : 0.0F;
        const bool bThrottleReady = PilotControls->CollectiveMode==ECollectiveMode::Incremental ?
            FMath::Abs(PilotControls->Processed[3])<0.001F : FMath::Abs((PilotControls->Processed[3]+1)*0.5F-CurrentCollective)<0.03F;
        if (bControllerDisconnected || FMath::Abs(PilotControls->Processed[0])>0.001F || FMath::Abs(PilotControls->Processed[1])>0.001F ||
            FMath::Abs(PilotControls->Processed[2])>0.001F || !bThrottleReady || bLanding)
        {
            PilotControls->Status=TEXT("Manual entry refused: center rotation axes; center incremental throttle or match direct collective");
            return false;
        }
        PilotControls->Throttle=CurrentCollective;
    }
    else if (IsManualMode())
    {
        if (Body->GetUpVector().Z < 0.5F)
        { PilotControls->Status=TEXT("Level within 60 degrees before engaging assisted flight"); return false; }
        HorizontalAccelerationCommand=Body->GetUpVector()*(GravityMetersPerSecondSquared/Body->GetUpVector().Z);
        HorizontalAccelerationCommand.Z=0;
    }
    const bool bWasManual=IsManualMode();
    FlightMode=NewMode;
    if (bNewManual) bTakeoffRequired=false;
    AltitudeTargetMeters=Body->GetComponentLocation().Z/MetersToCentimeters;
    YawTargetDegrees=Body->GetComponentRotation().Yaw;
    HoldPositionCentimeters=GetActorLocation(); bHoldingPosition=false; TranslationNeutralSeconds=0;
    if (bNewManual || bWasManual) RequireNeutralInput();
    PilotControls->Status=bNewManual ? TEXT("Manual control: no altitude/position hold; collective initialized to current thrust") : TEXT("Assisted mode selected");
    return true;
}
void AQuadrotorPawn::ToggleArm()
{
    if (MotorState==EMotorState::Armed)
    {
        if (!IsGrounded()) { PilotControls->Status=TEXT("Disarm refused: aircraft must be grounded"); return; }
        MotorState=EMotorState::Disarmed;
        PilotControls->Throttle=0;
        PilotControls->Status=TEXT("DISARMED");
        return;
    }
    const bool bNeutral = FMath::Abs(PilotControls->Processed[0])<0.001F &&
        FMath::Abs(PilotControls->Processed[1])<0.001F && FMath::Abs(PilotControls->Processed[2])<0.001F;
    const bool bThrottleSafe = !IsManualMode() ? FMath::Abs(PilotControls->Processed[3])<0.001F : (PilotControls->CollectiveMode==ECollectiveMode::Incremental ?
        FMath::Abs(PilotControls->Processed[3])<0.001F : PilotControls->Processed[3] < -0.95F);
    if (bControllerDisconnected || !bNeutral || !bThrottleSafe || !IsGrounded() ||
        GetMaximumCollectiveThrustNewtons()<=MassKg*GravityMetersPerSecondSquared)
    { PilotControls->Status=TEXT("Arm refused: ground, neutral rotation axes, safe throttle and sufficient thrust required"); return; }
    MotorState=EMotorState::Armed;
    bTakeoffRequired=!IsManualMode();
    PilotControls->Throttle=0;
    CollectiveCommandNewtons=0;
    AltitudeTargetMeters=Body->GetComponentLocation().Z/MetersToCentimeters;
    YawTargetDegrees=Body->GetComponentRotation().Yaw;
    bAwaitingNeutralInput=false;
    PilotControls->Status=TEXT("ARMED");
}
void AQuadrotorPawn::RequestTakeoff()
{
    if (IsManualMode()) { PilotControls->Status=TEXT("Assisted takeoff unavailable in Angle/Acro"); return; }
    if (MotorState!=EMotorState::Armed) { PilotControls->Status=TEXT("Hold arm first, then request takeoff"); return; }
    if (!IsGrounded()) { PilotControls->Status=TEXT("Takeoff requires grounded aircraft"); return; }
    bLanding=false; bTakeoffRequired=false;
    AltitudeTargetMeters=Body->GetComponentLocation().Z/MetersToCentimeters+1.5F;
    PilotControls->Status=TEXT("Assisted takeoff to 1.5 m above launch");
}
void AQuadrotorPawn::RequestLanding()
{
    if (IsManualMode()) { PilotControls->Status=TEXT("Controlled landing unavailable in Angle/Acro"); return; }
    if (MotorState!=EMotorState::Armed) { PilotControls->Status=TEXT("Landing requires an armed aircraft"); return; }
    bLanding=true; RequireNeutralInput();
    PilotControls->Status=TEXT("Controlled descent; motors stop near ground");
}
void AQuadrotorPawn::AdjustPilotCameraZoom(float Amount)
{
    if (IsFirstPerson()) FirstPersonCameraController->AdjustFieldOfView(-Amount*3);
    else CameraBoom->TargetArmLength=FMath::Clamp(CameraBoom->TargetArmLength-Amount*30,300.0F,900.0F);
}
FString AQuadrotorPawn::GetControlStatus() const
{
    const FVector Rates=Body->GetComponentQuat().UnrotateVector(Body->GetPhysicsAngularVelocityInRadians())*(180.0F/PI);
    const TCHAR* Armed=MotorState==EMotorState::Armed ? TEXT("ARMED") : MotorState==EMotorState::Failsafe ? TEXT("FAILSAFE / MOTORS OFF") : TEXT("DISARMED");
    FString Out=FString::Printf(TEXT("%s | %s | %s | %s\nInput pitch/roll/yaw: %+.2f %+.2f %+.2f | speed %.2f m/s | vertical %+.2f m/s\nPosition hold: %s | motor saturation: %s | input saturation: %s\n"),
        *GetFlightModeName(),*GetInputProfileName(),*GetViewModeName(),Armed,ForwardInput,LateralInput,YawInput,
        GetVelocityMetersPerSecond().Size2D(),GetVelocityMetersPerSecond().Z,bHoldingPosition ? TEXT("on") : TEXT("off"),bMotorSaturated ? TEXT("yes") : TEXT("no"),
        FMath::Max(FMath::Max(FMath::Abs(ForwardInputTarget),FMath::Abs(LateralInputTarget)),FMath::Max(FMath::Abs(YawInputTarget),FMath::Abs(ThrottleInputTarget)))>=0.999F ? TEXT("yes") : TEXT("no"));
    if (IsManualMode()) Out+=FString::Printf(TEXT("Collective %.1f%% (%s) | rates command/measured deg/s\nRoll %.1f/%.1f Pitch %.1f/%.1f Yaw %.1f/%.1f\n"),
        PilotControls->Throttle*100,PilotControls->CollectiveMode==ECollectiveMode::Incremental ? TEXT("latched") : TEXT("direct"),
        FMath::RadiansToDegrees(CommandedBodyRate.X),Rates.X,FMath::RadiansToDegrees(CommandedBodyRate.Y),Rates.Y,FMath::RadiansToDegrees(CommandedBodyRate.Z),Rates.Z);
    else Out+=FString::Printf(TEXT("Velocity command X/Y/Z %.2f %.2f %.2f m/s | actual %.2f %.2f %.2f m/s\nAltitude target %.2f m\nClimb request %+.2f (not collective)\n"),
        CommandedVelocity.X,CommandedVelocity.Y,CommandedVelocity.Z,GetVelocityMetersPerSecond().X,GetVelocityMetersPerSecond().Y,GetVelocityMetersPerSecond().Z,AltitudeTargetMeters,ThrottleInput);
    if (MotorState==EMotorState::Failsafe)
        Out+=TEXT("MOTORS OFF: reconnect, land/reset, then deliberately re-arm.\n");
    else if (MotorState==EMotorState::Disarmed)
        Out+=FString::Printf(TEXT("DISARMED: release sticks on ground; hold %s for %.1fs to arm.\n"),
            *PilotControls->ActionBindings(EPilotAction::Arm),PilotControls->HoldSeconds);
    else if (bAwaitingNeutralInput)
        Out+=TEXT("INPUT HELD: center sticks (direct collective: match the displayed throttle).\n");
    else if (bTakeoffRequired)
        Out+=TEXT("ARMED, WAITING FOR TAKEOFF: press ")+PilotControls->ActionBindings(EPilotAction::Takeoff)+TEXT(".\n");
    if (!PilotControls->Status.IsEmpty()) Out+=PilotControls->Status+TEXT("\n");
    return Out;
}
