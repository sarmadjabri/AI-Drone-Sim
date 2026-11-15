#if WITH_DEV_AUTOMATION_TESTS
#include "Flight/FlightControl.h"
#include "Flight/QuadrotorDynamics.h"
#include "Misc/AutomationTest.h"
#include "Flight/QuadrotorPawn.h"
#include "Flight/AetherGameMode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/BoxComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/SpringArmComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightAttitudeTest, "Aether.Flight.AttitudeDirection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightAttitudeTest::RunTest(const FString& Parameters)
{
    for (const float Heading : {0.0F, 90.0F, -179.0F})
    {
        const FQuat Level = FRotator(0.0, Heading, 0.0).Quaternion();
        for (const FVector Acceleration : {FVector(3, 0, 0), FVector(-3, 0, 0), FVector(0, 3, 0), FVector(0, -3, 0)})
        {
            const FVector Error = Aether::AttitudeError(Level, Acceleration, 9.81F, Heading);
            const FQuat Corrected = Level * FQuat(Error.GetSafeNormal(), Error.Size());
            TestTrue(TEXT("Torque error points thrust toward requested acceleration"),
                Corrected.GetAxisZ().Equals((Acceleration + FVector(0, 0, 9.81)).GetSafeNormal(), 1.e-5));
        }
        for (const FRotator Tilt : {FRotator(25, Heading, 0), FRotator(0, Heading, -25), FRotator(65, Heading, 40)})
        {
            const FQuat Current = Tilt.Quaternion();
            const FVector Error = Aether::AttitudeError(Current, FVector::ZeroVector, 9.81F, Heading);
            const FQuat Step = Current * FQuat(Error.GetSafeNormal(), Error.Size() * 0.1);
            TestTrue(TEXT("Restoring torque reduces tilt"), Step.GetAxisZ().Z > Current.GetAxisZ().Z);
        }
    }
    const FVector Wrap = Aether::AttitudeError(FRotator(0, 179, 0).Quaternion(), FVector::ZeroVector, 9.81F, -179);
    TestTrue(TEXT("Heading wrap follows shortest rotation"), Wrap.Size() < FMath::DegreesToRadians(3.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightResponseSmoothingTest, "Aether.Flight.ResponseSmoothing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightResponseSmoothingTest::RunTest(const FString& Parameters)
{
    const float SlewedInput = Aether::SlewTowards(0.0F, 1.0F, 4.0F, 0.1F);
    TestTrue(TEXT("Input slew respects the configured rate"), FMath::IsNearlyEqual(SlewedInput, 0.4F));
    TestEqual(TEXT("Release can reach neutral at its configured rate"),
        Aether::SlewTowards(1.0F, 0.0F, 12.0F, 0.1F), 0.0F);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightStabilityTest, "Aether.Flight.StepAndRelease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightStabilityTest::RunTest(const FString& Parameters)
{
    // Integrate the physical motor torques with actuator lag at multiple render rates.
    // This checks feedback sign, damping and recovery without a rendered map.
    for (const int32 Hz : {30, 60, 120})
    {
        const double Dt = 1.0 / Hz;
        FQuat Orientation = FQuat::Identity;
        FVector Rate = FVector::ZeroVector;
        Aether::DroneParameters Physics;
        Physics.motorTimeConstantS = 0.02F;
        Aether::MotorThrusts Motors{2.4525F, 2.4525F, 2.4525F, 2.4525F};
        for (int32 Frame = 0; Frame < Hz * 8; ++Frame)
        {
            const FVector Acceleration = Frame < Hz * 2 ? FVector(3, 0, 0) :
                Frame < Hz * 4 ? FVector(-3, 1, 0) : FVector::ZeroVector;
            const FVector Error = Aether::AttitudeError(Orientation, Acceleration, 9.81F, 0.0F);
            const FVector DesiredRate(FMath::Clamp(Error.X * 8, -2.5, 2.5),
                FMath::Clamp(Error.Y * 8, -2.5, 2.5), FMath::Clamp(Error.Z * 2, -1.0, 1.0));
            const FVector Torque = (DesiredRate - Rate) * FVector(0.22, 0.22, 0.12);
            Aether::DesiredWrench Wrench{9.81F, float(Torque.X), float(Torque.Y), float(Torque.Z)};
            Aether::MotorThrusts Target{}, Updated{};
            Aether::QuadrotorDynamics::AllocateMotorThrusts(Wrench, Physics, Target);
            Aether::QuadrotorDynamics::UpdateMotorThrusts(Motors, Target, Dt, Physics, Updated);
            Motors = Updated;
            const FVector ActualTorque((Motors[1] - Motors[3]) * 0.23,
                (Motors[2] - Motors[0]) * 0.23, (Motors[0] - Motors[1] + Motors[2] - Motors[3]) * 0.018);
            Rate += ActualTorque / FVector(0.02, 0.02, 0.04) * Dt;
            if (!Rate.IsNearlyZero())
            {
                Orientation = (Orientation * FQuat(Rate.GetSafeNormal(), Rate.Size() * Dt)).GetNormalized();
            }
            if (Orientation.GetAxisZ().Z < FMath::Cos(FMath::DegreesToRadians(24.0)))
            {
                AddError(FString::Printf(TEXT("Excess tilt at %d Hz"), Hz));
                return false;
            }
        }
        TestTrue(TEXT("Release settles level"), Orientation.GetAxisZ().Z > 0.999);
        TestTrue(TEXT("Release settles angular rate"), Rate.Size() < 0.01);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightVelocityTest, "Aether.Flight.VelocityNoRebound",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightVelocityTest::RunTest(const FString& Parameters)
{
    for (const int32 Hz : {30, 60, 120})
    for (const bool ReduceBeforeRelease : {false, true})
    for (const FVector Direction : {FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0), FVector(0.70710678, 0.70710678, 0)})
    {
        const double Dt = 1.0 / Hz;
        const float Speed = 10.0F;
        FQuat Orientation = FQuat::Identity;
        FVector Rate = FVector::ZeroVector;
        FVector Velocity = FVector::ZeroVector;
        FVector Command = FVector::ZeroVector;
        Aether::DroneParameters Physics;
        Physics.motorTimeConstantS = 0.02F;
        Aether::MotorThrusts Motors{2.4525F, 2.4525F, 2.4525F, 2.4525F};
        for (int32 Frame = 0; Frame < Hz * 30; ++Frame)
        {
            const float TargetSpeed = Frame < Hz * 12 ? Speed : ReduceBeforeRelease && Frame < Hz * 20 ? Speed * 0.25F : 0.0F;
            const FVector TargetVelocity = Direction * TargetSpeed;
            const bool Braking = FVector::DotProduct(TargetVelocity - Velocity, Velocity) < 0.0;
            const float MaxAcceleration = 9.81F * FMath::Tan(FMath::DegreesToRadians(Braking ? 40.0F : 35.0F));
            const FVector Acceleration = Aether::VelocityAcceleration(TargetVelocity, Velocity,
                Orientation.GetAxisZ(), 9.81F, Braking ? 6.0F : 4.0F, MaxAcceleration, 0.20F);
            const double Jerk = (Braking ? 70.0 : 45.0);
            Command += (Acceleration - Command).GetClampedToMaxSize(Jerk * Dt);
            Command = Command.GetClampedToMaxSize(MaxAcceleration);
            const FVector Error = Aether::AttitudeError(Orientation, Command, 9.81F, 0.0F);
            const FVector DesiredRate(FMath::Clamp(Error.X * 8, -2.5, 2.5),
                FMath::Clamp(Error.Y * 8, -2.5, 2.5), FMath::Clamp(Error.Z * 2, -1.0, 1.0));
            const FVector Torque = (DesiredRate - Rate) * FVector(0.22, 0.22, 0.12);
            const float Thrust = 9.81F / FMath::Max(Orientation.GetAxisZ().Z, 0.7);
            Aether::DesiredWrench Wrench{Thrust, float(Torque.X), float(Torque.Y), float(Torque.Z)};
            Aether::MotorThrusts Target{}, Updated{};
            Aether::QuadrotorDynamics::AllocateMotorThrusts(Wrench, Physics, Target);
            Aether::QuadrotorDynamics::UpdateMotorThrusts(Motors, Target, Dt, Physics, Updated);
            Motors = Updated;
            const FVector ActualTorque((Motors[1] - Motors[3]) * 0.23,
                (Motors[2] - Motors[0]) * 0.23, (Motors[0] - Motors[1] + Motors[2] - Motors[3]) * 0.018);
            Rate += ActualTorque / FVector(0.02, 0.02, 0.04) * Dt;
            if (!Rate.IsNearlyZero())
            {
                Orientation = (Orientation * FQuat(Rate.GetSafeNormal(), Rate.Size() * Dt)).GetNormalized();
            }
            FVector ActualAcceleration = Orientation.GetAxisZ() * (Motors[0] + Motors[1] + Motors[2] + Motors[3]);
            ActualAcceleration.Z = 0.0;
            Velocity += (ActualAcceleration - Velocity * 0.05) * Dt;
            const double Progress = FVector::DotProduct(Velocity, Direction);
            if (!ReduceBeforeRelease && Frame == Hz * 15)
            {
                TestTrue(TEXT("Full-speed release stops within three seconds"), Velocity.Size() < 0.12);
            }
            if (Progress < -0.12 || (Frame > Hz * 3 && Frame < Hz * 12 && FMath::Abs(Progress - Speed) > 0.5))
            {
                AddError(FString::Printf(TEXT("Speed/rebound failure: Hz=%d t=%.2f speed=%.3f"), Hz, Frame * Dt, Progress));
                return false;
            }
        }
        TestTrue(TEXT("Release settles to rest"), Velocity.Size() < 0.12);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightChaosStabilityTest, "Aether.Flight.ChaosStability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightChaosStabilityTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    World->bBegunPlay = true;
    AQuadrotorPawn* Pawn = World->SpawnActor<AQuadrotorPawn>(FVector(0, 0, 2000), FRotator::ZeroRotator);
    Pawn->FlightMode = EDroneFlightMode::LegacyAssisted;
    TestTrue(TEXT("Chaos uses configured inertia"), Pawn->Body->GetInertiaTensor().Equals(
        FVector(Pawn->InertiaXKgMetersSquared, Pawn->InertiaYKgMetersSquared,
            Pawn->InertiaZKgMetersSquared) * 10000.0, 0.1));
    for (const int32 Hz : {30, 60, 120})
    for (const FVector Direction : {FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0)})
    {
        Pawn->ResetDrone();
        bool bReversed = false;
        bool bReachedSpeed = false;
        bool bReducedSpeed = false;
        FVector StopPosition = FVector::ZeroVector;
        for (int32 Frame = 0; Frame < Hz * 80; ++Frame)
        {
            const float Input = Frame < Hz * 60 ? 1.0F : Frame < Hz * 66 ? 0.25F : 0.0F;
            Pawn->SetForwardInput(Input * Direction.X);
            Pawn->SetLateralInput(Input * Direction.Y);
            ++GFrameCounter; // Synchronous world ticks must be distinct engine frames.
            World->Tick(LEVELTICK_All, 1.0F / Hz);
            const FVector Velocity = Pawn->GetVelocityMetersPerSecond();
            const double Along = FVector::DotProduct(Velocity, Direction);
            if (Frame > Hz * 4 && Frame < Hz * 60 && (Along < 9.0 || Pawn->bHoldingPosition))
            {
                AddError(TEXT("Sustained input stopped or engaged position hold"));
                break;
            }
            if (Frame < Hz * 72 && Along < -0.15)
            {
                bReversed = true;
            }
            if (Frame == Hz * 59)
            {
                bReachedSpeed = FMath::Abs(Along - Pawn->MaximumHorizontalSpeedMetersPerSecond) < 0.5;
            }
            if (Frame == Hz * 65)
            {
                bReducedSpeed = FMath::Abs(Along - Pawn->MaximumHorizontalSpeedMetersPerSecond * Pawn->ApplyDeadzone(0.25F)) < 0.3;
            }
            if (Frame == Hz * 72)
            {
                TestTrue(TEXT("Release stops in Chaos"), Velocity.Size2D() < 0.15);
                TestTrue(TEXT("Release captures position hold"), Pawn->bHoldingPosition);
                StopPosition = Pawn->GetActorLocation();
                // Disturb a settled drone: the stabilizer should reject residual momentum.
                Pawn->Body->SetPhysicsLinearVelocity(FVector(60, -40, 0));
            }
        }
        TestFalse(TEXT("No opposite-direction swing under hold/reduction/release"), bReversed);
        TestTrue(TEXT("Held input reaches requested speed in Chaos"), bReachedSpeed);
        TestTrue(TEXT("Reduced input reaches lower speed in Chaos"), bReducedSpeed);
        TestTrue(TEXT("Stabilizer removes disturbance velocity"), Pawn->GetVelocityMetersPerSecond().Size2D() < 0.15);
        TestTrue(TEXT("Stabilizer holds stopping position"),
            FVector::Dist2D(Pawn->GetActorLocation(), StopPosition) < 20.0);
    }
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightImportedAssetsTest, "Aether.Flight.ImportedAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightImportedAssetsTest::RunTest(const FString& Parameters)
{
    const TCHAR* DroneParts[] = {
        TEXT("Arm1"), TEXT("Arm2"), TEXT("Arm3"), TEXT("Arm4"),
        TEXT("Battery"), TEXT("Body7"), TEXT("Bottom"), TEXT("Leg1"),
        TEXT("Leg2"), TEXT("Leg3"), TEXT("Leg4"), TEXT("Motor1"),
        TEXT("Motor2"), TEXT("Motor3"), TEXT("Motor4"), TEXT("Top")
    };
    FBox DroneBounds(ForceInit);
    for (const TCHAR* PartName : DroneParts)
    {
        const FString AssetPath = FString::Printf(
            TEXT("/Game/External/F450Drone/%s.%s"), PartName, PartName);
        UStaticMesh* Part = LoadObject<UStaticMesh>(nullptr, *AssetPath);
        TestNotNull(*FString::Printf(TEXT("Loads drone part %s"), PartName), Part);
        if (!Part)
        {
            continue;
        }

        const FBox Bounds = Part->GetBoundingBox();
        TestTrue(*FString::Printf(TEXT("Drone part %s has valid bounds"), PartName), Bounds.IsValid);
        if (Bounds.IsValid)
        {
            DroneBounds += Bounds;
            AddInfo(FString::Printf(TEXT("%s bounds: %s to %s"), PartName,
                *Bounds.Min.ToCompactString(), *Bounds.Max.ToCompactString()));
        }
    }

    TestTrue(TEXT("Imported drone parts have combined bounds"), DroneBounds.IsValid);
    AddInfo(FString::Printf(TEXT("Combined drone bounds: %s to %s"),
        *DroneBounds.Min.ToCompactString(), *DroneBounds.Max.ToCompactString()));

    const TCHAR* FacadeAssetName =
        TEXT("modular_urban_apartments_facade_1k_wall_window_centered_small_01");
    const FString FacadeAssetPath = FString::Printf(
        TEXT("/Game/External/PolyHaven/ModularUrbanApartments/%s.%s"),
        FacadeAssetName, FacadeAssetName);
    UStaticMesh* FacadeMesh = LoadObject<UStaticMesh>(nullptr, *FacadeAssetPath);
    TestNotNull(TEXT("Loads the Poly Haven windowed facade module"), FacadeMesh);
    if (FacadeMesh)
    {
        const FBox FacadeBounds = FacadeMesh->GetBoundingBox();
        TestTrue(TEXT("Facade module has valid bounds"), FacadeBounds.IsValid);
        AddInfo(FString::Printf(TEXT("Facade bounds: %s to %s; %d materials"),
            *FacadeBounds.Min.ToCompactString(), *FacadeBounds.Max.ToCompactString(),
            FacadeMesh->GetStaticMaterials().Num()));
    }

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    AQuadrotorPawn* Pawn = World->SpawnActor<AQuadrotorPawn>();
    TestNotNull(TEXT("Spawns the imported drone pawn"), Pawn);
    if (Pawn)
    {
        TestEqual(TEXT("Uses all imported F450 frame parts"), Pawn->ImportedDroneParts.Num(), 16);
        TestEqual(TEXT("Retains animated propellers"), Pawn->PropellerBlades.Num(), 8);
        TestTrue(TEXT("Hides the primitive frame fallback"), Pawn->Fuselage->bHiddenInGame);
        TestTrue(TEXT("Imported mesh origin aligns with the physics body"),
            Pawn->ImportedDroneParts[0]->GetRelativeLocation().Z == -11.0F);
    }

    AAetherGameMode* GameMode = World->SpawnActor<AAetherGameMode>();
    TestNotNull(TEXT("Spawns the game mode with imported facade references"), GameMode);
    if (GameMode)
    {
        GameMode->SpawnBuilding(
            FVector(1000.0F, 2000.0F, 0.0F),
            FVector(7.0F, 6.0F, 15.0F),
            FLinearColor::White,
            5,
            4);

        AStaticMeshActor* Building = nullptr;
        for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
        {
            Building = *It;
            break;
        }
        TestNotNull(TEXT("Spawns a grounded city building"), Building);
        if (Building)
        {
            const FBox BuildingBounds = Building->GetStaticMeshComponent()->Bounds.GetBox();
            TestTrue(TEXT("Building base rests on the ground plane"),
                FMath::IsNearlyEqual(BuildingBounds.Min.Z, 0.0F, 1.0F));

            UHierarchicalInstancedStaticMeshComponent* FacadePanels = nullptr;
            for (UActorComponent* Component : Building->GetComponents())
            {
                FacadePanels = Cast<UHierarchicalInstancedStaticMeshComponent>(Component);
                if (FacadePanels)
                {
                    break;
                }
            }
            TestNotNull(TEXT("Adds Poly Haven facade panels"), FacadePanels);
            if (FacadePanels && FacadePanels->GetInstanceCount() > 0)
            {
                FTransform FirstPanelTransform;
                const bool bHasPanelTransform =
                    FacadePanels->GetInstanceTransform(0, FirstPanelTransform, true);
                TestTrue(TEXT("Facade instance has a world transform"), bHasPanelTransform);
                if (bHasPanelTransform && FacadeMesh)
                {
                    const FBox FirstPanelBounds =
                        FacadeMesh->GetBoundingBox().TransformBy(FirstPanelTransform);
                    TestTrue(TEXT("Lowest facade panel is grounded"),
                        FMath::IsNearlyEqual(FirstPanelBounds.Min.Z, 0.0F, 1.0F));
                }
            }
            else
            {
                AddError(TEXT("Facade panel instances were not created"));
            }
        }
    }
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightViewModeTest, "Aether.Flight.ViewModes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightViewModeTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    AQuadrotorPawn* Pawn = World->SpawnActor<AQuadrotorPawn>();
    Pawn->FlightMode = EDroneFlightMode::LegacyAssisted;
    Pawn->InputProfile = EDroneInputProfile::LegacyViewDependent;
    TestFalse(TEXT("Starts in 3D View"), Pawn->IsFirstPerson());
    TestTrue(TEXT("Chase camera is active"), Pawn->Camera->IsActive() && !Pawn->FirstPersonCamera->IsActive());
    Pawn->SetPadLeftY(1.0F);
    Pawn->SetPadLeftX(0.5F);
    Pawn->SetPadRightX(-0.5F);
    Pawn->SetPadTriggers(0.7F);
    Pawn->UpdatePilotInput();
    TestEqual(TEXT("3D forward"), Pawn->ForwardInputTarget, 1.0F);
    TestEqual(TEXT("3D strafe"), Pawn->LateralInputTarget, Pawn->ApplyDeadzone(0.5F));
    TestEqual(TEXT("3D yaw"), Pawn->YawInputTarget, Pawn->ApplyDeadzone(-0.5F));
    TestEqual(TEXT("3D altitude from triggers"), Pawn->ThrottleInputTarget, 0.7F);
    Pawn->ToggleViewMode();
    Pawn->UpdatePilotInput();
    TestTrue(TEXT("FirstPerson camera only"), !Pawn->Camera->IsActive() && Pawn->FirstPersonCamera->IsActive());
    TestEqual(TEXT("First-person default is horizon stabilized"),
        Pawn->ViewMode, EDroneViewMode::StabilizedFirstPerson);
    TestTrue(TEXT("First-person camera is offset from the body and updated post-physics"),
        Pawn->FirstPersonCamera->GetAttachParent() == Pawn->Body &&
        Pawn->FirstPersonCameraController->PrimaryComponentTick.TickGroup == TG_PostPhysics);
    TestEqual(TEXT("Stabilized FPV has its own field of view"),
        Pawn->FirstPersonCamera->FieldOfView, Pawn->FirstPersonCameraController->StabilizedFieldOfView);
    TestTrue(TEXT("First-person camera excludes the drone visuals"),
        Pawn->Fuselage->bOwnerNoSee && Pawn->ImportedDroneParts[0]->bOwnerNoSee);
    UStaticMeshComponent* RuntimeDroneVisual =
        NewObject<UStaticMeshComponent>(Pawn, TEXT("RuntimeDroneVisual"));
    Pawn->AddInstanceComponent(RuntimeDroneVisual);
    RuntimeDroneVisual->SetupAttachment(Pawn->Body);
    RuntimeDroneVisual->RegisterComponent();
    Pawn->FirstPersonCameraController->UpdateFirstPersonVisibility();
    TestTrue(TEXT("FPV hides primitive visuals added at runtime"), RuntimeDroneVisual->bOwnerNoSee);
    TestTrue(TEXT("Held controls wait for neutral"), Pawn->bAwaitingNeutralInput);
    TestEqual(TEXT("Switch does not reinterpret forward as climb"), Pawn->ThrottleInputTarget, 0.0F);
    TestTrue(TEXT("Gimbal angular motion has physical rate and acceleration limits"),
        Pawn->FirstPersonCameraController->MaximumGimbalAngularVelocityDegreesPerSecond > 0.0F &&
        Pawn->FirstPersonCameraController->MaximumGimbalAngularAccelerationDegreesPerSecondSquared > 0.0F);

    Pawn->Body->SetWorldLocationAndRotation(
        FVector(150.0F, -80.0F, 250.0F), FRotator(20.0F, 65.0F, 35.0F));
    Pawn->FirstPersonCameraController->UpdateCameraPose(1.0F / 60.0F);
    const FVector ExpectedCameraPosition = Pawn->Body->GetComponentTransform().TransformPosition(
        Pawn->FirstPersonCameraController->MountOffsetCentimeters);
    TestTrue(TEXT("FPV mounting position follows the body transform exactly"),
        Pawn->FirstPersonCamera->GetComponentLocation().Equals(ExpectedCameraPosition, 0.01F));
    TestTrue(TEXT("Stabilized FPV compensates body roll"),
        FMath::Abs(Pawn->FirstPersonCamera->GetComponentRotation().Roll) < 1.0F);

    Pawn->Body->SetWorldRotation(FRotator(20.0F, 65.0F, 80.0F));
    const FQuat GimbalTestBodyRotation = Pawn->Body->GetComponentQuat();
    const FRotator LimitedGimbalRotation =
        (GimbalTestBodyRotation.Inverse() *
            Pawn->FirstPersonCameraController->GetStabilizedTarget(GimbalTestBodyRotation)).Rotator();
    TestTrue(TEXT("Stabilized gimbal respects configured pitch travel"),
        LimitedGimbalRotation.Pitch >=
                Pawn->FirstPersonCameraController->MinimumGimbalPitchDegrees - 0.1F &&
            LimitedGimbalRotation.Pitch <=
                Pawn->FirstPersonCameraController->MaximumGimbalPitchDegrees + 0.1F);
    TestTrue(TEXT("Stabilized gimbal respects configured roll travel"),
        FMath::Abs(LimitedGimbalRotation.Roll) <=
            Pawn->FirstPersonCameraController->MaximumGimbalRollDegrees + 0.1F);
    TestTrue(TEXT("Stabilized gimbal respects configured yaw travel"),
        FMath::Abs(LimitedGimbalRotation.Yaw) <=
            Pawn->FirstPersonCameraController->MaximumGimbalYawDegrees + 0.1F);
    Pawn->FirstPersonCameraController->UpdateCameraPose(1.0F / 60.0F);

    const FQuat CameraBeforeAbruptRoll = Pawn->FirstPersonCamera->GetComponentQuat();
    Pawn->Body->SetWorldRotation(FRotator(0.0F, 65.0F, -30.0F));
    Pawn->FirstPersonCameraController->UpdateCameraPose(1.0F / 120.0F);
    const float CameraStepDegrees = FMath::RadiansToDegrees(
        CameraBeforeAbruptRoll.AngularDistance(Pawn->FirstPersonCamera->GetComponentQuat()));
    TestTrue(FString::Printf(
            TEXT("Stabilized camera step %.3f degrees is bounded during abrupt attitude change"),
            CameraStepDegrees),
        CameraStepDegrees <=
            Pawn->FirstPersonCameraController->MaximumGimbalAngularVelocityDegreesPerSecond /
                120.0F + 0.1F);

    Pawn->Body->SetWorldRotation(FRotator(89.9F, -120.0F, 0.0F));
    Pawn->FirstPersonCameraController->UpdateCameraPose(1.0F / 60.0F);
    TestTrue(TEXT("Extreme pitch keeps a finite camera orientation"),
        !Pawn->FirstPersonCamera->GetComponentQuat().ContainsNaN());
    const float HeadingAtExtremePitch =
        Pawn->FirstPersonCameraController->LastStableHeadingDegrees;
    Pawn->Body->SetWorldRotation(FRotator(90.0F, -120.0F, 0.0F));
    Pawn->FirstPersonCameraController->UpdateCameraPose(1.0F / 60.0F);
    TestTrue(TEXT("Heading remains continuous at vertical flight attitudes"),
        FMath::IsNearlyEqual(
            Pawn->FirstPersonCameraController->LastStableHeadingDegrees,
            HeadingAtExtremePitch,
            0.1F));

    Pawn->SetPadLeftY(0); Pawn->SetPadLeftX(0); Pawn->SetPadRightX(0); Pawn->SetPadTriggers(0);
    Pawn->UpdatePilotInput();
    TestFalse(TEXT("Neutral unlocks controls"), Pawn->bAwaitingNeutralInput);
    Pawn->SetPadLeftY(0.5F); Pawn->SetPadLeftX(-0.5F);
    Pawn->SetPadRightY(1.0F); Pawn->SetPadRightX(0.5F); Pawn->SetPadTriggers(-1.0F);
    Pawn->UpdatePilotInput();
    TestEqual(TEXT("FP forward on right stick"), Pawn->ForwardInputTarget, 1.0F);
    TestEqual(TEXT("FP strafe on right stick"), Pawn->LateralInputTarget, Pawn->ApplyDeadzone(0.5F));
    TestEqual(TEXT("FP yaw on left stick"), Pawn->YawInputTarget, Pawn->ApplyDeadzone(-0.5F));
    TestEqual(TEXT("FP altitude on left stick, triggers ignored"), Pawn->ThrottleInputTarget, Pawn->ApplyDeadzone(0.5F));
    Pawn->SetPadLeftY(0); Pawn->SetPadLeftX(0); Pawn->SetPadRightX(0); Pawn->SetPadRightY(0); Pawn->SetPadTriggers(0);
    Pawn->SetFirstPersonKeyboardThrottle(-1);
    Pawn->UpdatePilotInput();
    TestEqual(TEXT("FP keyboard descent"), Pawn->ThrottleInputTarget, -1.0F);
    Pawn->SetFirstPersonKeyboardThrottle(0);
    Pawn->ToggleViewMode();
    Pawn->UpdatePilotInput();
    TestEqual(TEXT("Switches to rigidly body-locked FPV"),
        Pawn->ViewMode, EDroneViewMode::BodyLockedFirstPerson);
    TestEqual(TEXT("Body-locked FPV selects the wide field of view"),
        Pawn->FirstPersonCamera->FieldOfView, Pawn->FirstPersonCameraController->BodyLockedFieldOfView);
    Pawn->FirstPersonCameraController->UpdateCameraPose(1.0F / 60.0F);
    const FQuat ExpectedBodyLockedRotation =
        Pawn->Body->GetComponentQuat() *
        Pawn->FirstPersonCameraController->MountRotation.Quaternion() *
        FRotator(Pawn->FirstPersonCameraController->BodyLockedUptiltDegrees, 0.0F, 0.0F).Quaternion();
    TestTrue(TEXT("Body-locked FPV matches the rigid body and calibrated mount"),
        Pawn->FirstPersonCamera->GetComponentQuat().AngularDistance(ExpectedBodyLockedRotation) < 1.0e-4F);
    // Reference: 516063db90b7a1311d969ecdb80ff5a140900418. Exact rigid body * mount * uptilt, no lag.
    for (int Hz : {30,60,120})
    for (const FRotator Pose : {FRotator(0,179,0),FRotator(45,-179,60),FRotator(-70,90,-100),FRotator(89.9F,0,170)})
    {
        Pawn->Body->SetWorldLocationAndRotation(FVector(1000,-800,300),Pose);
        Pawn->FirstPersonCameraController->UpdateCameraPose(1.0F/Hz);
        const FQuat Historical = Pawn->Body->GetComponentQuat() *
            Pawn->FirstPersonCameraController->MountRotation.Quaternion() *
            FRotator(Pawn->FirstPersonCameraController->BodyLockedUptiltDegrees,0,0).Quaternion();
        TestTrue(TEXT("Historical body-locked rapid combined rotation, without lag"),
            Pawn->FirstPersonCamera->GetComponentQuat().AngularDistance(Historical)<1.e-4F);
        TestTrue(TEXT("Historical mount remains rigid during rapid translation"),
            Pawn->FirstPersonCamera->GetComponentLocation().Equals(Pawn->Body->GetComponentTransform().TransformPosition(
                Pawn->FirstPersonCameraController->MountOffsetCentimeters),0.01F));
    }
    Pawn->ToggleViewMode();
    Pawn->UpdatePilotInput();
    TestTrue(TEXT("Returns to chase camera"), Pawn->Camera->IsActive() && !Pawn->FirstPersonCamera->IsActive());
    TestFalse(TEXT("Third-person mode restores drone visibility"), Pawn->Fuselage->bOwnerNoSee);
    TestFalse(TEXT("Third-person mode also restores runtime-added visuals"),
        RuntimeDroneVisual->bOwnerNoSee);
    TestEqual(TEXT("Third-person camera uses its configured field of view"),
        Pawn->Camera->FieldOfView, Pawn->ThirdPersonFieldOfView);
    TestEqual(TEXT("Physics tuning unchanged by view switch"), Pawn->MaximumHorizontalSpeedMetersPerSecond, 10.0F);
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightPilotProfilesTest, "Aether.Flight.PilotProfiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightPilotProfilesTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    AQuadrotorPawn* Pawn = World->SpawnActor<AQuadrotorPawn>();
    Pawn->FlightMode = EDroneFlightMode::LegacyAssisted;
    for (EDroneInputProfile Profile : {EDroneInputProfile::ProfessionalMode2,
        EDroneInputProfile::Legacy3D, EDroneInputProfile::LegacyFPV})
    {
        Pawn->InputProfile = Profile;
        Pawn->bAwaitingNeutralInput = false;
        Pawn->SetPadLeftX(0.25F); Pawn->SetPadLeftY(0.5F);
        Pawn->SetPadRightX(-0.75F); Pawn->SetPadRightY(1.0F);
        Pawn->SetPadTriggers(-1.0F);
        Pawn->UpdatePilotInput();
        const float Forward = Pawn->ForwardInputTarget;
        const float Right = Pawn->LateralInputTarget;
        const float Yaw = Pawn->YawInputTarget;
        const float Vertical = Pawn->ThrottleInputTarget;
        TestEqual(TEXT("Correct forward stick"), Forward,
            Pawn->ApplyDeadzone(Profile == EDroneInputProfile::Legacy3D ? 0.5F : 1.0F));
        TestEqual(TEXT("Correct vertical stick or triggers"), Vertical,
            Profile == EDroneInputProfile::Legacy3D ? -1.0F : Pawn->ApplyDeadzone(0.5F));
        for (int View = 0; View < 3; ++View)
        {
            Pawn->ToggleViewMode();
            Pawn->UpdatePilotInput();
            TestFalse(TEXT("Independent camera switch never inhibits input"), Pawn->bAwaitingNeutralInput);
            TestEqual(TEXT("Camera preserves pitch command"), Pawn->ForwardInputTarget, Forward);
            TestEqual(TEXT("Camera preserves roll command"), Pawn->LateralInputTarget, Right);
            TestEqual(TEXT("Camera preserves yaw command"), Pawn->YawInputTarget, Yaw);
            TestEqual(TEXT("Camera preserves vertical command"), Pawn->ThrottleInputTarget, Vertical);
        }
        Pawn->bInvertForward = Pawn->bInvertLateral = Pawn->bInvertYaw = Pawn->bInvertVertical = true;
        Pawn->UpdatePilotInput();
        TestEqual(TEXT("Invert forward"), Pawn->ForwardInputTarget, -Forward);
        TestEqual(TEXT("Invert lateral"), Pawn->LateralInputTarget, -Right);
        TestEqual(TEXT("Invert yaw"), Pawn->YawInputTarget, -Yaw);
        TestEqual(TEXT("Invert vertical"), Pawn->ThrottleInputTarget, -Vertical);
        Pawn->bInvertForward = Pawn->bInvertLateral = Pawn->bInvertYaw = Pawn->bInvertVertical = false;
    }
    Pawn->RequireNeutralInput();
    Pawn->UpdatePilotInput();
    TestTrue(TEXT("Held axes cannot clear transition gate"), Pawn->bAwaitingNeutralInput);
    TestEqual(TEXT("Transition clears old command"), Pawn->ForwardInputTarget, 0.0F);
    Pawn->PadLeftX = Pawn->PadLeftY = Pawn->PadRightX = Pawn->PadRightY = Pawn->PadTriggers = 0.0F;
    Pawn->UpdatePilotInput();
    TestFalse(TEXT("Neutral permits new profile"), Pawn->bAwaitingNeutralInput);
    TestEqual(TEXT("Centered spring stick means altitude hold"), Pawn->ThrottleInputTarget, 0.0F);
    Pawn->SetPadLeftY(1.0F);
    Pawn->BeginResetHold();
    Pawn->HandleControllerConnection(false);
    Pawn->UpdatePilotInput();
    TestTrue(TEXT("Disconnect reported"), Pawn->IsControllerDisconnected());
    TestEqual(TEXT("Disconnect clears vertical input"), Pawn->ThrottleInputTarget, 0.0F);
    TestFalse(TEXT("Disconnect cancels reset hold"), Pawn->bResetHeld);
    Pawn->HandleControllerConnection(true);
    Pawn->SetPadLeftY(1.0F);
    Pawn->UpdatePilotInput();
    TestTrue(TEXT("Reconnect with held input waits for neutral"), Pawn->bAwaitingNeutralInput);
    Pawn->SetPadLeftY(0.0F);
    Pawn->UpdatePilotInput();
    Pawn->ResetHoldSeconds = 1.5F;
    Pawn->BeginResetHold();
    Pawn->UpdateResetHold(0.5F);
    TestTrue(TEXT("Short hold shows progress without resetting"), Pawn->bResetHeld && Pawn->GetResetProgress() > 0.0F);
    Pawn->EndResetHold();
    TestEqual(TEXT("Release cancels reset"), Pawn->GetResetProgress(), 0.0F);
    Pawn->BeginResetHold();
    Pawn->UpdateResetHold(1.6F);
    TestFalse(TEXT("Long hold executes once"), Pawn->bResetHeld);
    TestTrue(TEXT("Reset requires neutral"), Pawn->bAwaitingNeutralInput);
    const auto Cinematic = Aether::GetAssistedTuning(EDroneFlightMode::Cinematic);
    const auto Sport = Aether::GetAssistedTuning(EDroneFlightMode::Sport);
    const auto Training = Aether::GetAssistedTuning(EDroneFlightMode::Training);
    TestTrue(TEXT("Distinct assisted speed envelopes"), Training.SpeedScale < Cinematic.SpeedScale && Cinematic.SpeedScale < Sport.SpeedScale);
    TestTrue(TEXT("Cinematic limits jerk independently of tilt"), Cinematic.AccelerationJerk < Sport.AccelerationJerk && Cinematic.TiltLimit < Sport.TiltLimit);
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightAssistedModesTest, "Aether.Flight.AssistedModes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightAssistedModesTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    World->bBegunPlay = true;
    AQuadrotorPawn* Pawn = World->SpawnActor<AQuadrotorPawn>(FVector(0, 0, 2000), FRotator::ZeroRotator);
    for (const auto Mode : {EDroneFlightMode::Cinematic, EDroneFlightMode::Sport, EDroneFlightMode::Training})
    for (const int32 Hz : {30, 60, 120})
    {
        Pawn->FlightMode = Mode;
        Pawn->ResetDrone();
        const auto Tuning = Aether::GetAssistedTuning(Mode);
        for (int Frame = 0; Frame < Hz * 30; ++Frame)
        {
            Pawn->SetForwardInput(Frame < Hz * 15 ? 1.0F : 0.0F);
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, 1.0F / Hz);
            if (Frame == Hz * 14)
            {
                TestTrue(FString::Printf(TEXT("Mode %d at %d Hz reaches speed"), static_cast<int>(Mode), Hz),
                    FMath::Abs(Pawn->GetVelocityMetersPerSecond().X -
                        Pawn->MaximumHorizontalSpeedMetersPerSecond * Tuning.SpeedScale) < 0.5F);
            }
        }
        TestTrue(FString::Printf(TEXT("Mode %d at %d Hz settles after release"), static_cast<int>(Mode), Hz),
            Pawn->GetVelocityMetersPerSecond().Size2D() < 0.2F);
        TestTrue(TEXT("Assisted mode retains altitude"), FMath::Abs(Pawn->GetActorLocation().Z - 2000.0F) < 50.0F);
    }
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
