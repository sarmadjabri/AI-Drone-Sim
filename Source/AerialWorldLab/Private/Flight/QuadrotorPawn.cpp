#include "Flight/QuadrotorPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
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
    Body->SetBoxExtent(FVector(18.0, 13.0, 5.0));
    Body->SetCollisionProfileName(TEXT("PhysicsActor"));
    Body->SetSimulatePhysics(true);
    Body->SetEnableGravity(true);
    Body->SetNotifyRigidBodyCollision(true);

    Fuselage = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Fuselage"));
    Fuselage->SetupAttachment(Body);
    if (CubeMesh.Succeeded())
    {
        Fuselage->SetStaticMesh(CubeMesh.Object);
    }
    Fuselage->SetRelativeScale3D(FVector(0.30, 0.22, 0.075));
    Fuselage->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    UStaticMeshComponent* ArmX = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArmX"));
    ArmX->SetupAttachment(Body);
    if (CubeMesh.Succeeded())
    {
        ArmX->SetStaticMesh(CubeMesh.Object);
    }
    ArmX->SetRelativeScale3D(FVector(0.50, 0.035, 0.02));
    ArmX->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    UStaticMeshComponent* ArmY = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArmY"));
    ArmY->SetupAttachment(Body);
    if (CubeMesh.Succeeded())
    {
        ArmY->SetStaticMesh(CubeMesh.Object);
    }
    ArmY->SetRelativeScale3D(FVector(0.035, 0.50, 0.02));
    ArmY->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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
    CameraBoom->CameraLagSpeed = 15.0F;
    CameraBoom->bEnableCameraRotationLag = true;
    CameraBoom->CameraRotationLagSpeed = 12.0F;
    CameraBoom->bUseCameraLagSubstepping = true;
    CameraBoom->CameraLagMaxTimeStep = 1.0F / 60.0F;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("DroneCamera"));
    Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;

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
        }
    }

    ApplyTint(ArmX, FLinearColor(0.16F, 0.19F, 0.21F));
    ApplyTint(ArmY, FLinearColor(0.16F, 0.19F, 0.21F));
}

void AQuadrotorPawn::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
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
    Body->SetMassOverrideInKg(NAME_None, MassKg, true);
    const FVector CurrentInertia = Body->GetInertiaTensor();
    if (CurrentInertia.X <= SMALL_NUMBER ||
        CurrentInertia.Y <= SMALL_NUMBER ||
        CurrentInertia.Z <= SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot configure drone inertia from the current collision shape"));
        return;
    }
    const FVector TargetInertia(
        InertiaXKgMetersSquared * 10000.0F,
        InertiaYKgMetersSquared * 10000.0F,
        InertiaZKgMetersSquared * 10000.0F);
    Body->BodyInstance.InertiaTensorScale = TargetInertia / CurrentInertia;
    Body->RecreatePhysicsState();
    InitialDroneTransform = GetActorTransform();
    CollectiveCommandNewtons = MassKg * GravityMetersPerSecondSquared;
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

    Aether::DroneParameters Parameters;
    Parameters.massKg = MassKg;
    Parameters.armLengthM = ArmLengthMeters;
    Parameters.maxMotorThrustN = MaximumMotorThrustNewtons;
    Parameters.motorTimeConstantS = MotorTimeConstantSeconds;
    Parameters.yawMomentPerNewtonNm = YawMomentPerNewtonNewtonMeters;

    const float TiltLimit = FlightModeIndex == 0 ? 15.0F : FlightModeIndex == 2 ? 40.0F :
        MaximumTiltDegrees;
    const float YawRateLimit = FlightModeIndex == 0 ? 0.9F : FlightModeIndex == 2 ? 3.2F :
        MaximumYawRateRadiansPerSecond;
    const float ResponseSpeed = FlightModeIndex == 0 ? StickResponseSpeed * 0.55F :
        FlightModeIndex == 2 ? StickResponseSpeed * 1.7F : StickResponseSpeed;
    RollInput = FMath::FInterpTo(RollInput, RollInputTarget, DeltaSeconds, ResponseSpeed);
    PitchInput = FMath::FInterpTo(PitchInput, PitchInputTarget, DeltaSeconds, ResponseSpeed);
    YawInput = FMath::FInterpTo(YawInput, YawInputTarget, DeltaSeconds, ResponseSpeed);
    ThrottleInput = FMath::FInterpTo(
        ThrottleInput, ThrottleInputTarget, DeltaSeconds, ResponseSpeed);
    CameraBoom->TargetArmLength = FMath::Clamp(
        CameraBoom->TargetArmLength - CameraZoomInput * CameraZoomSpeed * DeltaSeconds,
        300.0F,
        900.0F);

    CollectiveCommandNewtons = FMath::Clamp(
        CollectiveCommandNewtons + ThrottleInput * ThrottleRampNewtonsPerSecond *
            (FlightModeIndex == 2 ? 1.6F : 1.0F) * DeltaSeconds,
        0.0F,
        4.0F * MaximumMotorThrustNewtons);

    const FRotator CurrentRotation = Body->GetComponentRotation();
    const FQuat BodyRotation = Body->GetComponentQuat();
    const FVector AngularVelocityBody = BodyRotation.UnrotateVector(
        Body->GetPhysicsAngularVelocityInRadians());

    const float DesiredRollDegrees = RollInput * TiltLimit;
    const float DesiredPitchDegrees = PitchInput * TiltLimit;
    const float RollErrorRadians = FMath::DegreesToRadians(
        FMath::FindDeltaAngleDegrees(CurrentRotation.Roll, DesiredRollDegrees));
    const float PitchErrorRadians = FMath::DegreesToRadians(
        FMath::FindDeltaAngleDegrees(CurrentRotation.Pitch, DesiredPitchDegrees));
    const float DesiredYawRate = YawInput * YawRateLimit;

    Aether::DesiredWrench Desired;
    Desired.collectiveThrustN = CollectiveCommandNewtons;
    Desired.rollTorqueNm = AngleGainNewtonMetersPerRadian * RollErrorRadians -
        RateDampingNewtonMeterSeconds * AngularVelocityBody.X;
    Desired.pitchTorqueNm = AngleGainNewtonMetersPerRadian * PitchErrorRadians -
        RateDampingNewtonMeterSeconds * AngularVelocityBody.Y;
    Desired.yawTorqueNm = YawRateGainNewtonMeterSeconds *
        (DesiredYawRate - AngularVelocityBody.Z);

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
    PlayerInputComponent->BindAxis(TEXT("DroneRoll"), this, &AQuadrotorPawn::SetRollInput);
    PlayerInputComponent->BindAxis(TEXT("DronePitch"), this, &AQuadrotorPawn::SetPitchInput);
    PlayerInputComponent->BindAxis(TEXT("DroneYaw"), this, &AQuadrotorPawn::SetYawInput);
    PlayerInputComponent->BindAxis(TEXT("DroneThrottle"), this, &AQuadrotorPawn::SetThrottleInput);
    PlayerInputComponent->BindAxis(TEXT("CameraZoom"), this, &AQuadrotorPawn::SetCameraZoomInput);
    PlayerInputComponent->BindAction(
        TEXT("ToggleFlightMode"), IE_Pressed, this, &AQuadrotorPawn::CycleFlightMode);
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

FString AQuadrotorPawn::GetFlightModeName() const
{
    switch (FlightModeIndex)
    {
    case 0:
        return TEXT("CINEMATIC");
    case 2:
        return TEXT("SPORT");
    default:
        return TEXT("NORMAL");
    }
}

int32 AQuadrotorPawn::GetFlightModeIndex() const
{
    return FlightModeIndex;
}

void AQuadrotorPawn::SetCameraZoomInput(float Value)
{
    CameraZoomInput = FMath::Clamp(Value, -1.0F, 1.0F);
}

void AQuadrotorPawn::CycleFlightMode()
{
    FlightModeIndex = (FlightModeIndex + 1) % 3;
    UE_LOG(LogTemp, Display, TEXT("Flight mode: %s"), *GetFlightModeName());
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
    RollInput = RollInputTarget = 0.0F;
    PitchInput = PitchInputTarget = 0.0F;
    YawInput = YawInputTarget = 0.0F;
    ThrottleInput = ThrottleInputTarget = 0.0F;
    CameraZoomInput = 0.0F;
    CollectiveCommandNewtons = MassKg * GravityMetersPerSecondSquared;
    MotorThrustsNewtons.fill(CollectiveCommandNewtons * 0.25F);
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
    const float Expo = 0.35F * Rescaled + 0.65F * Rescaled * Rescaled * Rescaled;
    return FMath::Sign(Value) * Expo;
}

void AQuadrotorPawn::SetRollInput(float Value)
{
    RollInputTarget = ApplyDeadzone(Value);
}

void AQuadrotorPawn::SetPitchInput(float Value)
{
    PitchInputTarget = ApplyDeadzone(Value);
}

void AQuadrotorPawn::SetYawInput(float Value)
{
    YawInputTarget = ApplyDeadzone(Value);
}

void AQuadrotorPawn::SetThrottleInput(float Value)
{
    ThrottleInputTarget = FMath::Clamp(Value, -1.0F, 1.0F);
}
