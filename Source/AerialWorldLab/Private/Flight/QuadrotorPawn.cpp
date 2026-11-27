// Aircraft construction and lifecycle; Tick reads as the ordered simulation pipeline.
#include "Flight/QuadrotorPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Flight/FlightControl.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "UObject/ConstructorHelpers.h"
#include "QuadrotorLayout.h"
#include "Flight/SimulationUnits.h"
#include "QuadrotorControlFrame.h"

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
    TArray<UStaticMeshComponent *> PrimitiveDroneParts;
    Body->SetBoxExtent(FVector(18.0, 13.0, 5.0));
    Body->SetCollisionProfileName(TEXT("PhysicsActor"));
    Body->SetSimulatePhysics(true);
    Body->SetEnableGravity(true);
    Body->SetNotifyRigidBodyCollision(true);
    Body->BodyInstance.bUseCCD = true;
    // Translational resistance is modeled explicitly using air-relative drag below.
    Body->SetLinearDamping(0.0F);
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

    UStaticMeshComponent *ArmX = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArmX"));
    ArmX->SetupAttachment(Body);
    if (CubeMesh.Succeeded())
    {
        ArmX->SetStaticMesh(CubeMesh.Object);
    }
    ArmX->SetRelativeScale3D(FVector(0.50, 0.035, 0.02));
    ArmX->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PrimitiveDroneParts.Add(ArmX);

    UStaticMeshComponent *ArmY = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArmY"));
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
        UStaticMeshComponent *LandingLeg = CreateDefaultSubobject<UStaticMeshComponent>(
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

    FirstPersonCameraController = CreateDefaultSubobject<UDroneCameraControllerComponent>(
        TEXT("FirstPersonCameraController"));
    FirstPersonCameraController->Configure(Body, FirstPersonCamera);

    for (int32 Motor = 0; Motor < 4; ++Motor)
    {
        UStaticMeshComponent *Rotor = CreateDefaultSubobject<UStaticMeshComponent>(
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
            UStaticMeshComponent *Propeller = CreateDefaultSubobject<UStaticMeshComponent>(
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

    TArray<UStaticMesh *> F450Parts;
    bool bF450ModelAvailable = true;
    for (const TCHAR *PartName : Aether::QuadrotorLayout::F450PartNames)
    {
        const FString AssetPath =
            FString::Printf(TEXT("/Game/External/F450Drone/%s.%s"), PartName, PartName);
        UStaticMesh *PartMesh = LoadObject<UStaticMesh>(nullptr, *AssetPath);
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
                TEXT("ImportedF450%s"), Aether::QuadrotorLayout::F450PartNames[PartIndex]);
            UStaticMeshComponent *Part =
                CreateDefaultSubobject<UStaticMeshComponent>(*ComponentName);
            Part->SetupAttachment(Body);
            Part->SetStaticMesh(F450Parts[PartIndex]);
            Part->SetRelativeLocation(
                FVector(0.0, 0.0, Aether::QuadrotorLayout::F450ModelOriginOffsetCentimeters));
            Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            ImportedDroneParts.Add(Part);
        }

        for (UStaticMeshComponent *PrimitivePart : PrimitiveDroneParts)
        {
            PrimitivePart->SetVisibility(false, true);
            PrimitivePart->SetHiddenInGame(true, true);
            PrimitivePart->SetCastShadow(false);
        }

        Rotors.Empty();
        PropellerBlades.Empty();
        for (int32 Motor = 0; Motor < UE_ARRAY_COUNT(Aether::QuadrotorLayout::MotorAssetIndices);
             ++Motor)
        {
            const FBox MotorBounds =
                F450Parts[Aether::QuadrotorLayout::MotorAssetIndices[Motor]]->GetBoundingBox();
            const FVector MotorLocation(
                MotorBounds.GetCenter().X, MotorBounds.GetCenter().Y,
                MotorBounds.Max.Z + 1.0F +
                    Aether::QuadrotorLayout::F450ModelOriginOffsetCentimeters);

            for (int32 Blade = 0; Blade < 2; ++Blade)
            {
                UStaticMeshComponent *Propeller = CreateDefaultSubobject<UStaticMeshComponent>(
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

void AQuadrotorPawn::OnConstruction(const FTransform &Transform)
{
    Super::OnConstruction(Transform);
    Camera->SetFieldOfView(ThirdPersonFieldOfView);
    ApplyViewMode();
    for (int32 Motor = 0; Motor < Rotors.Num() && Motor < 4; ++Motor)
    {
        const FVector MotorLocation = Aether::QuadrotorLayout::MotorArmUnitOffsets[Motor] *
                                      ArmLengthMeters * Aether::Units::CentimetersPerMeter;
        Rotors[Motor]->SetRelativeLocation(MotorLocation);
        for (int32 Blade = 0; Blade < 2; ++Blade)
        {
            const int32 BladeIndex = Motor * 2 + Blade;
            if (PropellerBlades.IsValidIndex(BladeIndex))
            {
                PropellerBlades[BladeIndex]->SetRelativeLocation(MotorLocation +
                                                                 FVector(0.0, 0.0, 3.0));
            }
        }
    }
}

void AQuadrotorPawn::BeginPlay()
{
    Super::BeginPlay();
    FString ConfigError;
    if (!PilotControls->Validate(ConfigError))
    {
        PilotControls->RestoreDefaults();
    }
    if (IsManualMode())
    {
        MotorState = EMotorState::Disarmed;
    }
    ConnectionChangedHandle =
        IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddWeakLambda(
            this,
            [this](EInputDeviceConnectionState State, FPlatformUserId User, FInputDeviceId Device)
            {
                const APlayerController *Player = Cast<APlayerController>(GetController());
                if (Player && Player->GetLocalPlayer() &&
                    Player->GetLocalPlayer()->GetPlatformUserId() == User)
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
    const FVector TargetInertia(
        InertiaXKgMetersSquared * Aether::Units::ChaosInertiaUnitsPerKilogramMeterSquared,
        InertiaYKgMetersSquared * Aether::Units::ChaosInertiaUnitsPerKilogramMeterSquared,
        InertiaZKgMetersSquared * Aether::Units::ChaosInertiaUnitsPerKilogramMeterSquared);
    FPhysicsCommand::ExecuteWrite(Body->BodyInstance.ActorHandle,
                                  [TargetInertia](FPhysicsActorHandle Actor)
                                  {
                                      FPhysicsInterface::SetMassSpaceInertiaTensor_AssumesLocked(
                                          Actor, TargetInertia);
                                  });
    InitialDroneTransform = GetActorTransform();
    HoldPositionCentimeters = GetActorLocation();
    bHoldingPosition = true;
    CollectiveCommandNewtons = 0.0F;
    if (MotorState == EMotorState::Armed)
    {
        CollectiveCommandNewtons = MassKg * GravityMetersPerSecondSquared;
    }
    AltitudeTargetMeters =
        InitialDroneTransform.GetLocation().Z / Aether::Units::CentimetersPerMeter;
    YawTargetDegrees = InitialDroneTransform.Rotator().Yaw;
    MotorThrustsNewtons.fill(CollectiveCommandNewtons * 0.25F);
    if (CollectiveCommandNewtons > 4.0F * MaximumMotorThrustNewtons)
    {
        UE_LOG(LogTemp, Warning, TEXT("Configured maximum thrust is below hover thrust"));
    }
}

void AQuadrotorPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(
        ConnectionChangedHandle);
    Super::EndPlay(EndPlayReason);
}

void AQuadrotorPawn::HandleControllerConnection(bool bConnected)
{
    bControllerDisconnected = !bConnected;
    PadLeftX = 0.0F;
    PadLeftY = 0.0F;
    PadRightX = 0.0F;
    PadRightY = 0.0F;
    PadTriggers = 0.0F;
    PadLeftTrigger = 0.0F;
    PadRightTrigger = 0.0F;
    EndResetHold();
    bLanding = false;
    PilotControls->CancelHeld();
    if (!bConnected && IsManualMode())
    {
        MotorState = EMotorState::Failsafe;
        PilotControls->Throttle = 0;
        PilotControls->Status =
            TEXT("Manual failsafe: motors stopped; land/reset and deliberately re-arm");
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

    // Hold actions and raw input use elapsed frame time, before controller hitch clamping.
    UpdatePilotCommands(DeltaSeconds);
    const FPilotModeTuning &ModeTuning = PilotControls->Tuning(FlightMode);
    FQuadrotorControlFrame Frame = ResolveControlFrame(ModeTuning);

    // Chaos still owns physical integration. Bound only command/actuator integration.
    DeltaSeconds = FMath::Min(DeltaSeconds, 1.0F / 30.0F);
    UpdateSmoothedPilotCommands(DeltaSeconds, Frame, ModeTuning);
    UpdateChaseCameraZoom(DeltaSeconds);
    Frame.CurrentRotation = Body->GetComponentRotation();
    Frame.BodyRotation = Body->GetComponentQuat();
    Frame.AngularVelocityBodyRadiansPerSecond =
        Frame.BodyRotation.UnrotateVector(Body->GetPhysicsAngularVelocityInRadians());
    Frame.DragForceWorldNewtons = ApplyAerodynamicDrag(Frame.BodyRotation);

    FVector DesiredBodyRate = FVector::ZeroVector;
    if (IsManualMode())
    {
        DesiredBodyRate = UpdateManualFlightCommands(DeltaSeconds, Frame, ModeTuning);
    }
    else
    {
        FVector HorizontalVelocity = FVector::ZeroVector;
        const FVector DesiredVelocity =
            CalculateAssistedVelocity(DeltaSeconds, Frame, HorizontalVelocity);
        DesiredBodyRate = CalculateAssistedBodyRates(DeltaSeconds, Frame, ModeTuning,
                                                     DesiredVelocity, HorizontalVelocity);
        UpdateAssistedAltitude(DeltaSeconds, Frame, ModeTuning, DesiredVelocity);
    }
    ApplyMotorCommands(DeltaSeconds, Frame, DesiredBodyRate);
}

FVector AQuadrotorPawn::GetVelocityMetersPerSecond() const
{
    return Body->GetPhysicsLinearVelocity() / Aether::Units::CentimetersPerMeter;
}

float AQuadrotorPawn::GetMotorThrustNewtons(int32 MotorIndex) const
{
    if (MotorIndex >= 0 && MotorIndex < static_cast<int32>(MotorThrustsNewtons.size()))
    {
        return MotorThrustsNewtons[MotorIndex];
    }
    return 0.0F;
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
    RawForwardInput = 0.0F;
    RawLateralInput = 0.0F;
    TranslationNeutralSeconds = 0.0F;
    LateralInput = 0.0F;
    LateralInputTarget = 0.0F;
    ForwardInput = 0.0F;
    ForwardInputTarget = 0.0F;
    YawInput = 0.0F;
    YawInputTarget = 0.0F;
    ThrottleInput = 0.0F;
    ThrottleInputTarget = 0.0F;
    CameraZoomInput = 0.0F;
    HorizontalAccelerationCommand = FVector::ZeroVector;
    FirstPersonCameraController->ResetCameraPose();
    HoldPositionCentimeters = InitialDroneTransform.GetLocation();
    bHoldingPosition = true;
    CollectiveCommandNewtons = MassKg * GravityMetersPerSecondSquared;
    MotorThrustsNewtons.fill(CollectiveCommandNewtons * 0.25F);
    AltitudeTargetMeters =
        InitialDroneTransform.GetLocation().Z / Aether::Units::CentimetersPerMeter;
    YawTargetDegrees = InitialDroneTransform.Rotator().Yaw;
    bLanding = false;
    if (IsManualMode() || bUseMappedInput)
    {
        MotorState = EMotorState::Disarmed;
    }
    else
    {
        MotorState = EMotorState::Armed;
    }
    bTakeoffRequired = false;
    PilotControls->Throttle = 0;
    if (MotorState != EMotorState::Armed)
    {
        CollectiveCommandNewtons = 0;
        MotorThrustsNewtons.fill(0);
    }
    UE_LOG(LogTemp, Display, TEXT("Drone reset to the launch point"));
}

void AQuadrotorPawn::ApplyTint(UPrimitiveComponent *Component, const FLinearColor &Color) const
{
    if (!Component || !DroneMaterial)
    {
        return;
    }
    UMaterialInstanceDynamic *Material = Component->CreateDynamicMaterialInstance(0, DroneMaterial);
    if (Material)
    {
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Component->SetMaterial(0, Material);
    }
}
