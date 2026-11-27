#if WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Flight/FlightControl.h"
#include "Flight/PilotControlsComponent.h"
#include "Flight/QuadrotorPawn.h"
#include "GameFramework/InputSettings.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProfessionalControlsTest, "Aether.Flight.ProfessionalControls",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FProfessionalControlsTest::RunTest(const FString &Parameters)
{
    FPrecisionAxis AxisCalibration;
    TestEqual(TEXT("Center is silent"), AxisCalibration.Process(0, 0.45F), 0.0F);
    TestEqual(TEXT("Inner dead zone"), AxisCalibration.Process(0.04F, 0.45F), 0.0F);
    TestEqual(TEXT("Positive full authority"), AxisCalibration.Process(1, 0.45F), 1.0F);
    TestEqual(TEXT("Negative full authority"), AxisCalibration.Process(-1, 0.45F), -1.0F);
    const float X =
        (0.1F - AxisCalibration.Inner) / (1 - AxisCalibration.Inner - AxisCalibration.Outer);
    TestTrue(TEXT("Expo equation"), FMath::IsNearlyEqual(AxisCalibration.Process(0.1F, 0.45F),
                                                         0.55F * X + 0.45F * X * X * X, 1.e-6F));
    TestTrue(TEXT("Small input remains controllable"),
             AxisCalibration.Process(0.06F, 0.45F) > 0 &&
                 AxisCalibration.Process(0.06F, 0.45F) < 0.02F);
    AxisCalibration.bInvert = true;
    TestEqual(TEXT("Independent inversion"), AxisCalibration.Process(1, 0.2F), -1.0F);
    AxisCalibration.bInvert = false;
    AxisCalibration.Center = 0.03F;
    AxisCalibration.Minimum = -0.9F;
    AxisCalibration.Maximum = 0.95F;
    TestEqual(TEXT("Calibrated center"), AxisCalibration.Process(0.03F, 0.2F), 0.0F);
    TestEqual(TEXT("Calibrated endpoint"), AxisCalibration.Process(0.95F, 0.2F), 1.0F);
    AxisCalibration.Maximum = AxisCalibration.Center;
    TestFalse(TEXT("Invalid range rejected"), AxisCalibration.IsValid());
    TestEqual(TEXT("Invalid range fails neutral"), AxisCalibration.Process(0.5F, 0.2F), 0.0F);

    UPilotControlsComponent *Controls = NewObject<UPilotControlsComponent>();
    FString Error;
    TestTrue(TEXT("Default settings valid"), Controls->Validate(Error));
    TestEqual(TEXT("Modifier selects its own action"),
              Controls->Buttons[Controls->FindBinding(EKeys::Gamepad_DPad_Up, 1)].Action,
              EPilotAction::IncreaseLimit);
    TestEqual(TEXT("Camera modifier distinct"),
              Controls->Buttons[Controls->FindBinding(EKeys::Gamepad_DPad_Up, 2)].Action,
              EPilotAction::GimbalUp);
    TestEqual(TEXT("Both modifiers suppress actions"),
              Controls->FindBinding(EKeys::Gamepad_DPad_Up, 3), INDEX_NONE);
    const FPilotButton Duplicate = Controls->Buttons[0];
    Controls->Buttons.Add(Duplicate);
    TestFalse(TEXT("Duplicate chords rejected"), Controls->Validate(Error));
    Controls->Buttons.Pop();
    Controls->Axes[1].Source = Controls->Axes[0].Source;
    TestFalse(TEXT("Duplicate axes rejected"), Controls->Validate(Error));
    Controls->RestoreDefaults();
    Controls->ProfileDirectory =
        FPaths::ProjectSavedDir() / TEXT("Automation/Controls") / FGuid::NewGuid().ToString();
    Controls->Axes[2].bInvert = true;
    Controls->Modes[6].YawDegrees = 123;
    Controls->LeftTriggerRole = ETriggerRole::Auxiliary;
    Controls->Modes[1].Acceleration = 2.5F;
    Controls->CollectiveMode = ECollectiveMode::Direct;
    TestTrue(TEXT("Save custom profile"), Controls->SaveSlot(1));
    Controls->RestoreDefaults();
    TestTrue(TEXT("Load custom profile"), Controls->LoadSlot(1));
    TestTrue(TEXT("Inversion persisted"), Controls->Axes[2].bInvert);
    TestEqual(TEXT("Mode rates persisted"), Controls->Modes[6].YawDegrees, 123.0F);
    TestEqual(TEXT("Independent left trigger role persisted"), Controls->LeftTriggerRole,
              ETriggerRole::Auxiliary);
    TestEqual(TEXT("Mode acceleration persisted"), Controls->Modes[1].Acceleration, 2.5F);
    TestEqual(TEXT("Throttle mode persisted"), Controls->CollectiveMode, ECollectiveMode::Direct);
    FFileHelper::SaveStringToFile(
        TEXT("[/Script/AerialWorldLab.PilotControlsComponent]\nThrottleIncrease=-20\n"),
        *Controls->SlotPath(2));
    TestFalse(TEXT("Invalid import rejected"), Controls->LoadSlot(2));
    TestEqual(TEXT("Rejected import preserves live values"), Controls->Modes[6].YawDegrees, 123.0F);
    IFileManager::Get().DeleteDirectory(*Controls->ProfileDirectory, false, true);

    UWorld *World = UWorld::CreateWorld(EWorldType::Game, false);
    AQuadrotorPawn *DronePawn = World->SpawnActor<AQuadrotorPawn>();
    DronePawn->FlightMode = EDroneFlightMode::Sport;
    DronePawn->bUseMappedInput = true;
    DronePawn->InputProfile = EDroneInputProfile::Custom;
    DronePawn->PilotControls->Axes[0].Source = 0;
    DronePawn->PilotControls->Axes[2].Source = 3;
    DronePawn->PadLeftX = 0.5F;
    DronePawn->UpdatePilotInput();
    TestTrue(TEXT("Custom physical routing"),
             DronePawn->ForwardInputTarget > 0 && DronePawn->YawInputTarget == 0);
    const EDroneViewMode Camera = DronePawn->ViewMode;
    DronePawn->PadLeftX = 0;
    DronePawn->UpdatePilotInput();
    DronePawn->CollectiveCommandNewtons = 9.81F;
    TestTrue(TEXT("Neutral incremental manual transition"),
             DronePawn->RequestFlightMode(EDroneFlightMode::Acro));
    TestTrue(TEXT("Manual collective initialized without jump"),
             FMath::IsNearlyEqual(DronePawn->PilotControls->Throttle, 9.81F / 48.0F));
    TestEqual(TEXT("Flight mode does not change camera"), DronePawn->ViewMode, Camera);
    DronePawn->PilotControls->CollectiveMode = ECollectiveMode::Direct;
    DronePawn->PilotControls->Processed[3] = 1.0F;
    TestFalse(TEXT("Direct unmatched throttle transition refused"),
              DronePawn->RequestFlightMode(EDroneFlightMode::Angle));
    DronePawn->PilotControls->Processed[3] = 2 * DronePawn->PilotControls->Throttle - 1;
    TestTrue(TEXT("Direct matched transition accepted"),
             DronePawn->RequestFlightMode(EDroneFlightMode::Angle));
    DronePawn->MotorState = EMotorState::Disarmed;
    DronePawn->ToggleArm();
    TestEqual(TEXT("Cannot arm off ground"), DronePawn->MotorState, EMotorState::Disarmed);
    DronePawn->MotorState = EMotorState::Armed;
    DronePawn->HandleControllerConnection(false);
    TestEqual(TEXT("Manual disconnect stops motors, no hidden autopilot"), DronePawn->MotorState,
              EMotorState::Failsafe);
    DronePawn->HandleControllerConnection(true);
    TestEqual(TEXT("Reconnect never arms"), DronePawn->MotorState, EMotorState::Failsafe);
    DronePawn->PilotControls->Press(EKeys::Gamepad_FaceButton_Left);
    DronePawn->PilotControls->Press(EKeys::Gamepad_LeftShoulder);
    TestFalse(TEXT("Modifier introduction cancels dangerous unmodified hold"),
              DronePawn->bResetHeld);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualFlightTest, "Aether.Flight.ManualFlight",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FManualFlightTest::RunTest(const FString &Parameters)
{
    UWorld *World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    World->bBegunPlay = true;
    AQuadrotorPawn *DronePawn =
        World->SpawnActor<AQuadrotorPawn>(FVector(0, 0, 10000), FRotator::ZeroRotator);
    for (int Hz : {30, 60, 120})
    {
        DronePawn->FlightMode = EDroneFlightMode::Acro;
        DronePawn->ResetDrone();
        DronePawn->MotorState = EMotorState::Armed;
        DronePawn->PilotControls->CollectiveMode = ECollectiveMode::Incremental;
        DronePawn->PilotControls->Throttle = 0.3F;
        DronePawn->ThrottleInput = 0;
        DronePawn->ThrottleInputTarget = 0;
        DronePawn->SetLateralInput(0.4F);
        DronePawn->SetForwardInput(0.2F);
        DronePawn->SetYawInput(-0.3F);
        for (int SampleIndex = 0; SampleIndex < Hz * 2; ++SampleIndex)
        {
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, 1.0F / Hz);
        }
        const FVector Rates = DronePawn->Body->GetComponentQuat().UnrotateVector(
            DronePawn->Body->GetPhysicsAngularVelocityInRadians());
        AddInfo(FString::Printf(TEXT("Acro %d Hz commanded %s measured %s"), Hz,
                                *DronePawn->CommandedBodyRate.ToString(), *Rates.ToString()));
        TestTrue(TEXT("Acro follows simultaneous body rate commands"),
                 (Rates - DronePawn->CommandedBodyRate).Size() < 0.25F);
        TestFalse(TEXT("No Acro position hold"), DronePawn->bHoldingPosition);
        DronePawn->SetLateralInput(-0.4F);
        DronePawn->SetForwardInput(-0.2F);
        DronePawn->SetYawInput(0.3F);
        for (int SampleIndex = 0; SampleIndex < Hz; ++SampleIndex)
        {
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, 1.0F / Hz);
        }
        const FVector Reversed = DronePawn->Body->GetComponentQuat().UnrotateVector(
            DronePawn->Body->GetPhysicsAngularVelocityInRadians());
        TestTrue(TEXT("Acro follows rapid simultaneous reversal"),
                 (Reversed - DronePawn->CommandedBodyRate).Size() < 0.25F);
        TestTrue(TEXT("Spring center preserves latched collective"),
                 FMath::IsNearlyEqual(DronePawn->PilotControls->Throttle, 0.3F));
        DronePawn->ResetDrone();
        DronePawn->MotorState = EMotorState::Armed;
        DronePawn->PilotControls->Throttle = 0.3F;
        DronePawn->Body->SetWorldRotation(FRotator(0, 0, 25));
        for (int SampleIndex = 0; SampleIndex < Hz * 2; ++SampleIndex)
        {
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, 1.0F / Hz);
        }
        TestTrue(TEXT("Centered Acro does not auto-level"),
                 FMath::Abs(DronePawn->Body->GetComponentRotation().Roll) > 20);
        DronePawn->FlightMode = EDroneFlightMode::Angle;
        DronePawn->ResetDrone();
        DronePawn->MotorState = EMotorState::Armed;
        DronePawn->PilotControls->Throttle = 0.3F;
        DronePawn->Body->SetWorldRotation(FRotator(20, 0, 25));
        for (int SampleIndex = 0; SampleIndex < Hz * 3; ++SampleIndex)
        {
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, 1.0F / Hz);
        }
        TestTrue(TEXT("Angle returns toward level"), DronePawn->Body->GetUpVector().Z > 0.995F);
        DronePawn->ResetDrone();
        for (int SampleIndex = 0; SampleIndex < Hz; ++SampleIndex)
        {
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, 1.0F / Hz);
        }
        TestEqual(TEXT("Disarmed manual has zero motor thrust"),
                  DronePawn->GetCollectiveThrustNewtons(), 0.0F);
        // Integrate the same incremental command at each render frequency.
        DronePawn->ResetDrone();
        DronePawn->MotorState = EMotorState::Armed;
        DronePawn->ThrottleInput = 1;
        DronePawn->ThrottleInputTarget = 1;
        for (int SampleIndex = 0; SampleIndex < Hz; ++SampleIndex)
        {
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, 1.0F / Hz);
        }
        TestTrue(TEXT("Incremental throttle is time based"),
                 FMath::IsNearlyEqual(DronePawn->PilotControls->Throttle,
                                      DronePawn->PilotControls->ThrottleIncrease, 0.002F));
    }
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHighPerformanceTest, "Aether.Flight.HighPerformance",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FHighPerformanceTest::RunTest(const FString &Parameters)
{
    UWorld *World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    World->bBegunPlay = true;
    AQuadrotorPawn *DronePawn =
        World->SpawnActor<AQuadrotorPawn>(FVector(0, 0, 2000), FRotator::ZeroRotator);
    DronePawn->FlightMode = EDroneFlightMode::HighPerformance;
    DronePawn->ViewMode = EDroneViewMode::BodyLockedFirstPerson;
    DronePawn->ApplyViewMode();
    for (int Hz : {30, 60, 120})
    {
        DronePawn->ResetDrone();
        float TimeToSpeed = -1;
        for (int SampleIndex = 0; SampleIndex < Hz * 12; ++SampleIndex)
        {
            DronePawn->SetForwardInput(1);
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, 1.0F / Hz);
            if (TimeToSpeed < 0 &&
                DronePawn->GetVelocityMetersPerSecond().X >=
                    DronePawn->PilotControls->Tuning(DronePawn->FlightMode).Speed * 0.95F)
            {
                TimeToSpeed = static_cast<float>(SampleIndex) / Hz;
            }
        }
        TestTrue(TEXT("High performance reaches its faster configured speed"),
                 FMath::Abs(DronePawn->GetVelocityMetersPerSecond().X -
                            DronePawn->PilotControls->Tuning(DronePawn->FlightMode).Speed) < 0.6F);
        TestTrue(TEXT("High performance accelerates to 95% within 6 seconds"),
                 TimeToSpeed > 0 && TimeToSpeed < 6);
        const FVector Start = DronePawn->GetActorLocation();
        float StopTime = -1;
        for (int SampleIndex = 0; SampleIndex < Hz * 12; ++SampleIndex)
        {
            DronePawn->SetForwardInput(0);
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, 1.0F / Hz);
            if (StopTime < 0 && DronePawn->GetVelocityMetersPerSecond().Size2D() < 0.2F)
            {
                StopTime = static_cast<float>(SampleIndex) / Hz;
            }
        }
        const float Distance = FVector::Dist2D(Start, DronePawn->GetActorLocation()) / 100;
        AddInfo(FString::Printf(
            TEXT("HighPerformance %d Hz: 95%% speed %.2fs, stop %.2fs / %.2fm, altitude %.2fm"), Hz,
            TimeToSpeed, StopTime, Distance, DronePawn->GetActorLocation().Z / 100));
        const FQuat HistoricalCamera =
            DronePawn->Body->GetComponentQuat() * FRotator(10, 0, 0).Quaternion();
        TestTrue(TEXT("Historical FPV rotation after acceleration and braking"),
                 DronePawn->FirstPersonCamera->GetComponentQuat().AngularDistance(
                     HistoricalCamera) < 1.e-4F);
        TestTrue(TEXT("Historical FPV mount after acceleration and braking"),
                 DronePawn->FirstPersonCamera->GetComponentLocation().Equals(
                     DronePawn->Body->GetComponentTransform().TransformPosition(FVector(12, 0, 8)),
                     0.01F));
        TestTrue(TEXT("High performance settles"),
                 DronePawn->GetVelocityMetersPerSecond().Size2D() < 0.2F &&
                     DronePawn->bHoldingPosition);
        TestTrue(TEXT("High performance altitude retained"),
                 FMath::Abs(DronePawn->GetActorLocation().Z - 2000) < 70);
        const FPilotModeTuning &ModeTuning =
            DronePawn->PilotControls->Tuning(DronePawn->FlightMode);
        const float BrakeAcceleration = FMath::Min(
            ModeTuning.Braking, DronePawn->GravityMetersPerSecondSquared *
                                    FMath::Tan(FMath::DegreesToRadians(ModeTuning.BrakeTilt)));
        // Allow one second of attitude/motor transient beyond v^2/(2a). The
        // stopping envelope must scale with speed instead of using the old 20 m/s bound.
        const float StoppingEnvelope =
            ModeTuning.Speed * ModeTuning.Speed / (2 * BrakeAcceleration) + ModeTuning.Speed;
        TestTrue(TEXT("Physically bounded stopping distance"),
                 Distance > 5 && Distance < StoppingEnvelope);
        DronePawn->ResetDrone();
        for (int SampleIndex = 0; SampleIndex < Hz * 3; ++SampleIndex)
        {
            DronePawn->SetThrottleInput(1);
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, 1.0F / Hz);
        }
        TestTrue(TEXT("High-performance ascent approaches 4m/s"),
                 DronePawn->GetVelocityMetersPerSecond().Z > 3.4F);
        const float Target = DronePawn->AltitudeTargetMeters;
        for (int SampleIndex = 0; SampleIndex < Hz * 6; ++SampleIndex)
        {
            DronePawn->SetThrottleInput(0);
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, 1.0F / Hz);
        }
        TestTrue(TEXT("Vertical release captures altitude"),
                 FMath::Abs(DronePawn->GetActorLocation().Z / 100 -
                            DronePawn->AltitudeTargetMeters) < 0.2F);
        TestTrue(TEXT("Vertical release has bounded target travel"),
                 FMath::Abs(DronePawn->AltitudeTargetMeters - Target) < 0.25F);
    }
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXboxControlsTest, "Aether.Flight.XboxControls",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FXboxControlsTest::RunTest(const FString &Parameters)
{
    UPilotControlsComponent *Controls = NewObject<UPilotControlsComponent>();
    const auto Check = [&](FKey Key, int Modifier, EPilotAction Action)
    {
        const int Index = Controls->FindBinding(Key, Modifier);
        TestTrue(TEXT("Xbox chord exists"), Index != INDEX_NONE);
        if (Index != INDEX_NONE)
        {
            TestEqual(TEXT("Xbox chord action"), Controls->Buttons[Index].Action, Action);
        }
    };
    Check(EKeys::Gamepad_FaceButton_Bottom, 0, EPilotAction::Confirm);
    Check(EKeys::Gamepad_FaceButton_Right, 0, EPilotAction::Cancel);
    Check(EKeys::Gamepad_FaceButton_Left, 0, EPilotAction::Reset);
    Check(EKeys::Gamepad_FaceButton_Top, 0, EPilotAction::Camera);
    Check(EKeys::Gamepad_Special_Right, 0, EPilotAction::Settings);
    Check(EKeys::Gamepad_Special_Left, 0, EPilotAction::Telemetry);
    Check(EKeys::Gamepad_FaceButton_Bottom, 1, EPilotAction::Takeoff);
    Check(EKeys::Gamepad_FaceButton_Right, 1, EPilotAction::Land);
    Check(EKeys::Gamepad_RightThumbstick, 1, EPilotAction::Arm);
    Controls->Press(EKeys::Gamepad_LeftShoulder);
    TestEqual(TEXT("LB activates flight modifier"), Controls->ModifierMask(), 1);
    Controls->Press(EKeys::Gamepad_RightShoulder);
    TestEqual(TEXT("LB+RB suppresses unmodified camera"),
              Controls->FindBinding(EKeys::Gamepad_FaceButton_Top, Controls->ModifierMask()),
              INDEX_NONE);
    Controls->CancelHeld();
    TestEqual(TEXT("Disconnect clears Xbox modifiers"), Controls->ModifierMask(), 0);
    TestEqual(TEXT("Xbox face label"), Controls->ButtonLabel(EKeys::Gamepad_FaceButton_Bottom),
              FString(TEXT("A / Cross")));
    // Check real configured axis mappings, not just already-normalized pilot values.
    for (const auto Name : {TEXT("PadLeftY"), TEXT("PadRightY")})
    {
        TArray<FInputAxisKeyMapping> Mappings;
        GetDefault<UInputSettings>()->GetAxisMappingByName(FName(Name), Mappings);
        TestEqual(TEXT("One physical vertical mapping"), Mappings.Num(), 1);
        if (Mappings.Num() == 1)
        {
            TestEqual(TEXT("UE positive-up stays positive"), Mappings[0].Scale, 1.0F);
        }
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControlSafetyTest, "Aether.Flight.ControlSafety",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FControlSafetyTest::RunTest(const FString &Parameters)
{
    UWorld *World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    World->bBegunPlay = true;
    AQuadrotorPawn *DronePawn =
        World->SpawnActor<AQuadrotorPawn>(FVector(0, 0, 2000), FRotator::ZeroRotator);
    auto *Controls = DronePawn->PilotControls.Get();
    DronePawn->bUseMappedInput = true;
    DronePawn->InputProfile = EDroneInputProfile::ProfessionalMode2;
    DronePawn->FlightMode = EDroneFlightMode::Sport;
    Controls->Axes[0].bInvert = true;
    DronePawn->KeyboardForward = 1;
    DronePawn->UpdatePilotInput();
    TestEqual(TEXT("Controller inversion does not invert keyboard"), DronePawn->ForwardInputTarget,
              1.0F);
    DronePawn->KeyboardForward = 0;
    Controls->RestoreDefaults();
    Controls->LeftTriggerRole = ETriggerRole::Auxiliary;
    Controls->TriggerRole = ETriggerRole::Precision;
    DronePawn->PadLeftTrigger = 0.7F;
    DronePawn->PadRightTrigger = 1;
    DronePawn->UpdatePilotInput();
    TestTrue(TEXT("Independent trigger precision"), Controls->bPrecision);
    TestEqual(TEXT("Independent auxiliary channel"), Controls->AuxiliaryChannels[0], 0.7F);
    DronePawn->InputProfile = EDroneInputProfile::Custom;
    Controls->Axes[3].Source = 5;
    DronePawn->UpdatePilotInput();
    TestFalse(TEXT("Mapped throttle trigger cannot also change precision"), Controls->bPrecision);
    DronePawn->HandleControllerConnection(false);
    TestEqual(TEXT("Disconnect clears raw triggers immediately"), DronePawn->PadRightTrigger, 0.0F);
    TestFalse(TEXT("Disconnect clears precision immediately"), Controls->bPrecision);
    DronePawn->HandleControllerConnection(true);
    DronePawn->InputProfile = EDroneInputProfile::ProfessionalMode2;
    Controls->RestoreDefaults();

    // Customized dangerous chords must stop when either their button or modifier is released.
    const int Reset = Controls->FindBinding(EKeys::Gamepad_FaceButton_Left, 0);
    Controls->Buttons[Reset].Modifiers = 2;
    Controls->Press(EKeys::Gamepad_RightShoulder);
    Controls->Press(EKeys::Gamepad_FaceButton_Left);
    TestTrue(TEXT("Custom RB+X reset starts"), DronePawn->bResetHeld);
    Controls->Release(EKeys::Gamepad_RightShoulder);
    TestFalse(TEXT("Modifier release cancels custom reset"), DronePawn->bResetHeld);
    Controls->CancelHeld();
    Controls->RestoreDefaults();
    Controls->Press(EKeys::Gamepad_FaceButton_Left);
    DronePawn->UpdateResetHold(0.25F);
    Controls->Press(EKeys::Gamepad_FaceButton_Left);
    TestTrue(TEXT("Duplicate press does not restart hold timer"),
             DronePawn->GetResetProgress() > 0);
    Controls->CancelHeld();

    // Assisted transitions preserve held commands and physical state; manual entry rejects held
    // sticks.
    DronePawn->bAwaitingNeutralInput = false;
    DronePawn->PadRightY = 0.5F;
    DronePawn->UpdatePilotInput();
    const float Command = DronePawn->ForwardInputTarget;
    const FVector Velocity(300, 50, 20);
    DronePawn->Body->SetPhysicsLinearVelocity(Velocity);
    TestTrue(TEXT("Assisted mode switch while moving"),
             DronePawn->RequestFlightMode(EDroneFlightMode::Cinematic));
    TestEqual(TEXT("Assisted switch preserves held input"), DronePawn->ForwardInputTarget, Command);
    TestTrue(TEXT("Assisted switch preserves momentum"),
             DronePawn->Body->GetPhysicsLinearVelocity().Equals(Velocity));
    DronePawn->RequireNeutralInput();
    DronePawn->UpdatePilotInput();
    TestFalse(TEXT("Gated target cannot disguise held physical stick on manual entry"),
              DronePawn->RequestFlightMode(EDroneFlightMode::Acro));
    DronePawn->PadRightY = 0;
    DronePawn->UpdatePilotInput();

    // Direct collective handoff starts at the matched absolute input, including below midpoint.
    for (int Hz : {30, 60, 120})
    {
        DronePawn->FlightMode = EDroneFlightMode::Angle;
        DronePawn->ResetDrone();
        DronePawn->MotorState = EMotorState::Armed;
        Controls->CollectiveMode = ECollectiveMode::Direct;
        Controls->Throttle = 0.1F;
        DronePawn->CollectiveCommandNewtons = 0.1F * DronePawn->GetMaximumCollectiveThrustNewtons();
        Controls->Axes[3].Inner = Controls->Axes[3].Outer = Controls->Axes[3].ExpoOffset = 0;
        DronePawn->PadLeftY = -0.8F;
        DronePawn->RequireNeutralInput();
        DronePawn->UpdatePilotInput();
        TestFalse(TEXT("Matching direct throttle clears gate"), DronePawn->bAwaitingNeutralInput);
        ++GFrameCounter;
        World->Tick(LEVELTICK_All, 1.0F / Hz);
        TestTrue(TEXT("Direct throttle never slews from a 50 percent midpoint"),
                 FMath::IsNearlyEqual(Controls->Throttle, 0.1F, 1.e-5F));
        // Pure command shaping must retain small inputs and full authority in every mode.
        for (int Mode = 1; Mode < 7; ++Mode)
        {
            const auto Flight = static_cast<EDroneFlightMode>(Mode);
            for (int Axis = 0; Axis < 4; ++Axis)
            {
                TestEqual(TEXT("All modes full-stick authority"), Controls->Shape(Axis, 1, Flight),
                          1.0F);
                TestTrue(TEXT("All modes small-stick precision"),
                         Controls->Shape(Axis, 0.08F, Flight) > 0 &&
                             Controls->Shape(Axis, 0.08F, Flight) < 0.1F);
            }
        }
    }
    // Exercise actual paused menu navigation, remap capture, and reserved modifier precedence.
    Controls->CancelHeld();
    Controls->bMenuOpen = true;
    Controls->Page = 3;
    Controls->ButtonIndex = 0;
    Controls->MenuKey(EKeys::Enter);
    Controls->Press(EKeys::Gamepad_LeftShoulder);
    Controls->Press(EKeys::K);
    TestEqual(TEXT("Interactive capture saves new key"), Controls->Buttons[0].Key, EKeys::K);
    TestEqual(TEXT("Interactive capture saves chord"), Controls->Buttons[0].Modifiers, 1);
    Controls->MenuKey(EKeys::Tab);
    TestTrue(TEXT("Controller map reachable with keyboard"), Controls->IsMapPage());
    DronePawn->PadRightX = 0.6F;
    Controls->RefreshPausedInput();
    TestEqual(TEXT("Paused calibration sees live raw axis"), Controls->Raw[2], 0.6F);
    TestTrue(TEXT("Paused calibration sees live processed axis"), Controls->Processed[1] > 0);
    TestEqual(TEXT("Paused calibration cannot issue flight command"), DronePawn->LateralInputTarget,
              0.0F);
    Controls->CancelHeld();
    Controls->bMenuOpen = false;

    // Complete the actual assisted startup/landing workflow against collision geometry.
    AActor *Ground = World->SpawnActor<AActor>();
    UBoxComponent *Floor = NewObject<UBoxComponent>(Ground);
    Ground->SetRootComponent(Floor);
    Floor->SetBoxExtent(FVector(100, 100, 5));
    Floor->SetCollisionProfileName(TEXT("BlockAll"));
    Floor->RegisterComponent();
    Floor->SetWorldLocation(FVector(0, 0, -5));
    DronePawn->FlightMode = EDroneFlightMode::Sport;
    Controls->RestoreDefaults();
    DronePawn->Body->SetWorldLocationAndRotation(FVector(0, 0, 5), FRotator::ZeroRotator);
    DronePawn->Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
    DronePawn->Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    DronePawn->PadLeftY = 0;
    DronePawn->PadRightX = 0;
    DronePawn->MotorState = EMotorState::Disarmed;
    DronePawn->RequireNeutralInput();
    DronePawn->UpdatePilotInput();
    DronePawn->ToggleArm();
    TestEqual(TEXT("Grounded neutral deliberate arming succeeds"), DronePawn->MotorState,
              EMotorState::Armed);
    TestTrue(TEXT("Arming alone does not take off"), DronePawn->bTakeoffRequired);
    TestTrue(TEXT("HUD explains armed takeoff gate"),
             DronePawn->GetControlStatus().Contains(TEXT("WAITING FOR TAKEOFF")));
    DronePawn->RequestTakeoff();
    TestFalse(TEXT("Explicit takeoff unlocks motors"), DronePawn->bTakeoffRequired);
    for (int SampleIndex = 0; SampleIndex < 300; ++SampleIndex)
    {
        ++GFrameCounter;
        World->Tick(LEVELTICK_All, 1.0F / 60);
    }
    TestTrue(TEXT("Assisted takeoff reaches requested height"),
             DronePawn->GetActorLocation().Z > 120 && DronePawn->GetActorLocation().Z < 190);
    DronePawn->RequestLanding();
    for (int SampleIndex = 0; SampleIndex < 600; ++SampleIndex)
    {
        ++GFrameCounter;
        World->Tick(LEVELTICK_All, 1.0F / 60);
    }
    TestEqual(TEXT("Controlled landing disarms near ground"), DronePawn->MotorState,
              EMotorState::Disarmed);
    TestTrue(TEXT("HUD explains disarmed inputs"),
             DronePawn->GetControlStatus().Contains(TEXT("hold")));
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
