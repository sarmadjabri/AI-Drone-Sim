#if WITH_DEV_AUTOMATION_TESTS
#include "Flight/PilotControlsComponent.h"
#include "Flight/QuadrotorPawn.h"
#include "Flight/FlightControl.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "GameFramework/InputSettings.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProfessionalControlsTest,"Aether.Flight.ProfessionalControls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProfessionalControlsTest::RunTest(const FString& Parameters)
{
    FPrecisionAxis A;
    TestEqual(TEXT("Center is silent"),A.Process(0,0.45F),0.0F);
    TestEqual(TEXT("Inner dead zone"),A.Process(0.04F,0.45F),0.0F);
    TestEqual(TEXT("Positive full authority"),A.Process(1,0.45F),1.0F);
    TestEqual(TEXT("Negative full authority"),A.Process(-1,0.45F),-1.0F);
    const float X=(0.1F-A.Inner)/(1-A.Inner-A.Outer);
    TestTrue(TEXT("Expo equation"),FMath::IsNearlyEqual(A.Process(0.1F,0.45F),0.55F*X+0.45F*X*X*X,1.e-6F));
    TestTrue(TEXT("Small input remains controllable"),A.Process(0.06F,0.45F)>0 && A.Process(0.06F,0.45F)<0.02F);
    A.bInvert=true; TestEqual(TEXT("Independent inversion"),A.Process(1,0.2F),-1.0F);
    A.bInvert=false; A.Center=0.03F; A.Minimum=-0.9F; A.Maximum=0.95F;
    TestEqual(TEXT("Calibrated center"),A.Process(0.03F,0.2F),0.0F);
    TestEqual(TEXT("Calibrated endpoint"),A.Process(0.95F,0.2F),1.0F);
    A.Maximum=A.Center; TestFalse(TEXT("Invalid range rejected"),A.IsValid());
    TestEqual(TEXT("Invalid range fails neutral"),A.Process(0.5F,0.2F),0.0F);

    UPilotControlsComponent* C=NewObject<UPilotControlsComponent>();
    FString Error; TestTrue(TEXT("Default settings valid"),C->Validate(Error));
    TestEqual(TEXT("Modifier selects its own action"), C->Buttons[C->FindBinding(EKeys::Gamepad_DPad_Up,1)].Action,EPilotAction::IncreaseLimit);
    TestEqual(TEXT("Camera modifier distinct"), C->Buttons[C->FindBinding(EKeys::Gamepad_DPad_Up,2)].Action,EPilotAction::GimbalUp);
    TestEqual(TEXT("Both modifiers suppress actions"),C->FindBinding(EKeys::Gamepad_DPad_Up,3),INDEX_NONE);
    const FPilotButton Duplicate=C->Buttons[0]; C->Buttons.Add(Duplicate); TestFalse(TEXT("Duplicate chords rejected"),C->Validate(Error)); C->Buttons.Pop();
    C->Axes[1].Source=C->Axes[0].Source; TestFalse(TEXT("Duplicate axes rejected"),C->Validate(Error)); C->RestoreDefaults();
    C->ProfileDirectory=FPaths::ProjectSavedDir()/TEXT("Automation/Controls")/FGuid::NewGuid().ToString();
    C->Axes[2].bInvert=true; C->Modes[6].YawDegrees=123; C->LeftTriggerRole=ETriggerRole::Auxiliary; C->Modes[1].Acceleration=2.5F; C->CollectiveMode=ECollectiveMode::Direct;
    TestTrue(TEXT("Save custom profile"),C->SaveSlot(1));
    C->RestoreDefaults(); TestTrue(TEXT("Load custom profile"),C->LoadSlot(1));
    TestTrue(TEXT("Inversion persisted"),C->Axes[2].bInvert);
    TestEqual(TEXT("Mode rates persisted"),C->Modes[6].YawDegrees,123.0F);
    TestEqual(TEXT("Independent left trigger role persisted"),C->LeftTriggerRole,ETriggerRole::Auxiliary);
    TestEqual(TEXT("Mode acceleration persisted"),C->Modes[1].Acceleration,2.5F);
    TestEqual(TEXT("Throttle mode persisted"),C->CollectiveMode,ECollectiveMode::Direct);
    FFileHelper::SaveStringToFile(TEXT("[/Script/AerialWorldLab.PilotControlsComponent]\nThrottleIncrease=-20\n"),*C->SlotPath(2));
    TestFalse(TEXT("Invalid import rejected"),C->LoadSlot(2));
    TestEqual(TEXT("Rejected import preserves live values"),C->Modes[6].YawDegrees,123.0F);
    IFileManager::Get().DeleteDirectory(*C->ProfileDirectory,false,true);

    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    AQuadrotorPawn* P=World->SpawnActor<AQuadrotorPawn>();
    P->FlightMode=EDroneFlightMode::Sport; P->bUseMappedInput=true;
    P->InputProfile=EDroneInputProfile::Custom;
    P->PilotControls->Axes[0].Source=0; P->PilotControls->Axes[2].Source=3;
    P->PadLeftX=0.5F; P->UpdatePilotInput();
    TestTrue(TEXT("Custom physical routing"),P->ForwardInputTarget>0 && P->YawInputTarget==0);
    const EDroneViewMode Camera=P->ViewMode;
    P->PadLeftX=0; P->UpdatePilotInput();
    P->CollectiveCommandNewtons=9.81F;
    TestTrue(TEXT("Neutral incremental manual transition"),P->RequestFlightMode(EDroneFlightMode::Acro));
    TestTrue(TEXT("Manual collective initialized without jump"),FMath::IsNearlyEqual(P->PilotControls->Throttle,9.81F/48.0F));
    TestEqual(TEXT("Flight mode does not change camera"),P->ViewMode,Camera);
    P->PilotControls->CollectiveMode=ECollectiveMode::Direct;
    P->PilotControls->Processed[3]=1.0F;
    TestFalse(TEXT("Direct unmatched throttle transition refused"),P->RequestFlightMode(EDroneFlightMode::Angle));
    P->PilotControls->Processed[3]=2*P->PilotControls->Throttle-1;
    TestTrue(TEXT("Direct matched transition accepted"),P->RequestFlightMode(EDroneFlightMode::Angle));
    P->MotorState=EMotorState::Disarmed;
    P->ToggleArm(); TestEqual(TEXT("Cannot arm off ground"),P->MotorState,EMotorState::Disarmed);
    P->MotorState=EMotorState::Armed;
    P->HandleControllerConnection(false);
    TestEqual(TEXT("Manual disconnect stops motors, no hidden autopilot"),P->MotorState,EMotorState::Failsafe);
    P->HandleControllerConnection(true); TestEqual(TEXT("Reconnect never arms"),P->MotorState,EMotorState::Failsafe);
    P->PilotControls->Press(EKeys::Gamepad_FaceButton_Left);
    P->PilotControls->Press(EKeys::Gamepad_LeftShoulder);
    TestFalse(TEXT("Modifier introduction cancels dangerous unmodified hold"),P->bResetHeld);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualFlightTest,"Aether.Flight.ManualFlight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FManualFlightTest::RunTest(const FString& Parameters)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->bBegunPlay=true;
    AQuadrotorPawn* P=World->SpawnActor<AQuadrotorPawn>(FVector(0,0,10000),FRotator::ZeroRotator);
    for (int Hz : {30,60,120})
    {
        P->FlightMode=EDroneFlightMode::Acro; P->ResetDrone(); P->MotorState=EMotorState::Armed;
        P->PilotControls->CollectiveMode=ECollectiveMode::Incremental;
        P->PilotControls->Throttle=0.3F; P->ThrottleInput=P->ThrottleInputTarget=0;
        P->SetLateralInput(0.4F); P->SetForwardInput(0.2F); P->SetYawInput(-0.3F);
        for (int I=0;I<Hz*2;++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,1.0F/Hz); }
        const FVector Rates=P->Body->GetComponentQuat().UnrotateVector(P->Body->GetPhysicsAngularVelocityInRadians());
        AddInfo(FString::Printf(TEXT("Acro %d Hz commanded %s measured %s"),Hz,*P->CommandedBodyRate.ToString(),*Rates.ToString()));
        TestTrue(TEXT("Acro follows simultaneous body rate commands"),(Rates-P->CommandedBodyRate).Size()<0.25F);
        TestFalse(TEXT("No Acro position hold"),P->bHoldingPosition);
        P->SetLateralInput(-0.4F); P->SetForwardInput(-0.2F); P->SetYawInput(0.3F);
        for (int I=0;I<Hz;++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,1.0F/Hz); }
        const FVector Reversed=P->Body->GetComponentQuat().UnrotateVector(P->Body->GetPhysicsAngularVelocityInRadians());
        TestTrue(TEXT("Acro follows rapid simultaneous reversal"),(Reversed-P->CommandedBodyRate).Size()<0.25F);
        TestTrue(TEXT("Spring center preserves latched collective"),FMath::IsNearlyEqual(P->PilotControls->Throttle,0.3F));
        P->ResetDrone(); P->MotorState=EMotorState::Armed; P->PilotControls->Throttle=0.3F;
        P->Body->SetWorldRotation(FRotator(0,0,25));
        for (int I=0;I<Hz*2;++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,1.0F/Hz); }
        TestTrue(TEXT("Centered Acro does not auto-level"),FMath::Abs(P->Body->GetComponentRotation().Roll)>20);
        P->FlightMode=EDroneFlightMode::Angle; P->ResetDrone(); P->MotorState=EMotorState::Armed;
        P->PilotControls->Throttle=0.3F; P->Body->SetWorldRotation(FRotator(20,0,25));
        for (int I=0;I<Hz*3;++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,1.0F/Hz); }
        TestTrue(TEXT("Angle returns toward level"),P->Body->GetUpVector().Z>0.995F);
        P->ResetDrone();
        for (int I=0;I<Hz;++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,1.0F/Hz); }
        TestEqual(TEXT("Disarmed manual has zero motor thrust"),P->GetCollectiveThrustNewtons(),0.0F);
        // Integrate the same incremental command at each render frequency.
        P->ResetDrone(); P->MotorState=EMotorState::Armed; P->ThrottleInput=P->ThrottleInputTarget=1;
        for (int I=0;I<Hz;++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,1.0F/Hz); }
        TestTrue(TEXT("Incremental throttle is time based"),FMath::IsNearlyEqual(P->PilotControls->Throttle,P->PilotControls->ThrottleIncrease,0.002F));
    }
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHighPerformanceTest,"Aether.Flight.HighPerformance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHighPerformanceTest::RunTest(const FString& Parameters)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->bBegunPlay=true;
    AQuadrotorPawn* P=World->SpawnActor<AQuadrotorPawn>(FVector(0,0,2000),FRotator::ZeroRotator);
    P->FlightMode=EDroneFlightMode::HighPerformance;
    P->ViewMode=EDroneViewMode::BodyLockedFirstPerson; P->ApplyViewMode();
    for (int Hz : {30,60,120})
    {
        P->ResetDrone();
        float TimeToSpeed=-1;
        for (int I=0;I<Hz*12;++I)
        {
            P->SetForwardInput(1); ++GFrameCounter; World->Tick(LEVELTICK_All,1.0F/Hz);
            if (TimeToSpeed<0 && P->GetVelocityMetersPerSecond().X>=19) TimeToSpeed=static_cast<float>(I)/Hz;
        }
        TestTrue(TEXT("High performance reaches 20 m/s"),FMath::Abs(P->GetVelocityMetersPerSecond().X-20)<0.6F);
        TestTrue(TEXT("High performance accelerates to 95% within 6 seconds"),TimeToSpeed>0 && TimeToSpeed<6);
        const FVector Start=P->GetActorLocation();
        float StopTime=-1;
        for (int I=0;I<Hz*12;++I)
        {
            P->SetForwardInput(0); ++GFrameCounter; World->Tick(LEVELTICK_All,1.0F/Hz);
            if (StopTime<0 && P->GetVelocityMetersPerSecond().Size2D()<0.2F) StopTime=static_cast<float>(I)/Hz;
        }
        const float Distance=FVector::Dist2D(Start,P->GetActorLocation())/100;
        AddInfo(FString::Printf(TEXT("HighPerformance %d Hz: 95%% speed %.2fs, stop %.2fs / %.2fm, altitude %.2fm"),
            Hz,TimeToSpeed,StopTime,Distance,P->GetActorLocation().Z/100));
        const FQuat HistoricalCamera=P->Body->GetComponentQuat()*FRotator(10,0,0).Quaternion();
        TestTrue(TEXT("Historical FPV rotation after acceleration and braking"),
            P->FirstPersonCamera->GetComponentQuat().AngularDistance(HistoricalCamera)<1.e-4F);
        TestTrue(TEXT("Historical FPV mount after acceleration and braking"),
            P->FirstPersonCamera->GetComponentLocation().Equals(P->Body->GetComponentTransform().TransformPosition(FVector(12,0,8)),0.01F));
        TestTrue(TEXT("High performance settles"),P->GetVelocityMetersPerSecond().Size2D()<0.2F && P->bHoldingPosition);
        TestTrue(TEXT("High performance altitude retained"),FMath::Abs(P->GetActorLocation().Z-2000)<70);
        TestTrue(TEXT("Physically bounded stopping distance"),Distance>5 && Distance<50);
        P->ResetDrone();
        for (int I=0;I<Hz*3;++I)
        { P->SetThrottleInput(1); ++GFrameCounter; World->Tick(LEVELTICK_All,1.0F/Hz); }
        TestTrue(TEXT("High-performance ascent approaches 4m/s"),P->GetVelocityMetersPerSecond().Z>3.4F);
        const float Target=P->AltitudeTargetMeters;
        for (int I=0;I<Hz*6;++I)
        { P->SetThrottleInput(0); ++GFrameCounter; World->Tick(LEVELTICK_All,1.0F/Hz); }
        TestTrue(TEXT("Vertical release captures altitude"),FMath::Abs(P->GetActorLocation().Z/100-P->AltitudeTargetMeters)<0.2F);
        TestTrue(TEXT("Vertical release has bounded target travel"),FMath::Abs(P->AltitudeTargetMeters-Target)<0.25F);
    }
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXboxControlsTest,"Aether.Flight.XboxControls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FXboxControlsTest::RunTest(const FString& Parameters)
{
    UPilotControlsComponent* C=NewObject<UPilotControlsComponent>();
    const auto Check=[&](FKey Key,int Modifier,EPilotAction Action)
    {
        const int Index=C->FindBinding(Key,Modifier);
        TestTrue(TEXT("Xbox chord exists"),Index!=INDEX_NONE);
        if (Index!=INDEX_NONE) TestEqual(TEXT("Xbox chord action"),C->Buttons[Index].Action,Action);
    };
    Check(EKeys::Gamepad_FaceButton_Bottom,0,EPilotAction::Confirm);
    Check(EKeys::Gamepad_FaceButton_Right,0,EPilotAction::Cancel);
    Check(EKeys::Gamepad_FaceButton_Left,0,EPilotAction::Reset);
    Check(EKeys::Gamepad_FaceButton_Top,0,EPilotAction::Camera);
    Check(EKeys::Gamepad_Special_Right,0,EPilotAction::Settings);
    Check(EKeys::Gamepad_Special_Left,0,EPilotAction::Telemetry);
    Check(EKeys::Gamepad_FaceButton_Bottom,1,EPilotAction::Takeoff);
    Check(EKeys::Gamepad_FaceButton_Right,1,EPilotAction::Land);
    Check(EKeys::Gamepad_RightThumbstick,1,EPilotAction::Arm);
    C->Press(EKeys::Gamepad_LeftShoulder);
    TestEqual(TEXT("LB activates flight modifier"),C->ModifierMask(),1);
    C->Press(EKeys::Gamepad_RightShoulder);
    TestEqual(TEXT("LB+RB suppresses unmodified camera"),C->FindBinding(EKeys::Gamepad_FaceButton_Top,C->ModifierMask()),INDEX_NONE);
    C->CancelHeld();
    TestEqual(TEXT("Disconnect clears Xbox modifiers"),C->ModifierMask(),0);
    TestEqual(TEXT("Xbox face label"),C->ButtonLabel(EKeys::Gamepad_FaceButton_Bottom),FString(TEXT("A / Cross")));
    // Check real configured axis mappings, not just already-normalized pilot values.
    for (const auto Name : {TEXT("PadLeftY"),TEXT("PadRightY")})
    {
        TArray<FInputAxisKeyMapping> Mappings;
        GetDefault<UInputSettings>()->GetAxisMappingByName(FName(Name),Mappings);
        TestEqual(TEXT("One physical vertical mapping"),Mappings.Num(),1);
        if (Mappings.Num()==1) TestEqual(TEXT("UE positive-up stays positive"),Mappings[0].Scale,1.0F);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControlSafetyTest,"Aether.Flight.ControlSafety",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FControlSafetyTest::RunTest(const FString& Parameters)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->bBegunPlay=true;
    AQuadrotorPawn* P=World->SpawnActor<AQuadrotorPawn>(FVector(0,0,2000),FRotator::ZeroRotator);
    auto* C=P->PilotControls.Get();
    P->bUseMappedInput=true; P->InputProfile=EDroneInputProfile::ProfessionalMode2;
    P->FlightMode=EDroneFlightMode::Sport;
    C->Axes[0].bInvert=true; P->KeyboardForward=1;
    P->UpdatePilotInput();
    TestEqual(TEXT("Controller inversion does not invert keyboard"),P->ForwardInputTarget,1.0F);
    P->KeyboardForward=0; C->RestoreDefaults();
    C->LeftTriggerRole=ETriggerRole::Auxiliary; C->TriggerRole=ETriggerRole::Precision;
    P->PadLeftTrigger=0.7F; P->PadRightTrigger=1; P->UpdatePilotInput();
    TestTrue(TEXT("Independent trigger precision"),C->bPrecision);
    TestEqual(TEXT("Independent auxiliary channel"),C->AuxiliaryChannels[0],0.7F);
    P->InputProfile=EDroneInputProfile::Custom; C->Axes[3].Source=5;
    P->UpdatePilotInput(); TestFalse(TEXT("Mapped throttle trigger cannot also change precision"),C->bPrecision);
    P->HandleControllerConnection(false);
    TestEqual(TEXT("Disconnect clears raw triggers immediately"),P->PadRightTrigger,0.0F);
    TestFalse(TEXT("Disconnect clears precision immediately"),C->bPrecision);
    P->HandleControllerConnection(true); P->InputProfile=EDroneInputProfile::ProfessionalMode2; C->RestoreDefaults();

    // Customized dangerous chords must stop when either their button or modifier is released.
    const int Reset=C->FindBinding(EKeys::Gamepad_FaceButton_Left,0); C->Buttons[Reset].Modifiers=2;
    C->Press(EKeys::Gamepad_RightShoulder); C->Press(EKeys::Gamepad_FaceButton_Left);
    TestTrue(TEXT("Custom RB+X reset starts"),P->bResetHeld);
    C->Release(EKeys::Gamepad_RightShoulder);
    TestFalse(TEXT("Modifier release cancels custom reset"),P->bResetHeld);
    C->CancelHeld(); C->RestoreDefaults();
    C->Press(EKeys::Gamepad_FaceButton_Left); P->UpdateResetHold(0.25F);
    C->Press(EKeys::Gamepad_FaceButton_Left);
    TestTrue(TEXT("Duplicate press does not restart hold timer"),P->GetResetProgress()>0);
    C->CancelHeld();

    // Assisted transitions preserve held commands and physical state; manual entry rejects held sticks.
    P->bAwaitingNeutralInput=false; P->PadRightY=0.5F; P->UpdatePilotInput();
    const float Command=P->ForwardInputTarget;
    const FVector Velocity(300,50,20); P->Body->SetPhysicsLinearVelocity(Velocity);
    TestTrue(TEXT("Assisted mode switch while moving"),P->RequestFlightMode(EDroneFlightMode::Cinematic));
    TestEqual(TEXT("Assisted switch preserves held input"),P->ForwardInputTarget,Command);
    TestTrue(TEXT("Assisted switch preserves momentum"),P->Body->GetPhysicsLinearVelocity().Equals(Velocity));
    P->RequireNeutralInput(); P->UpdatePilotInput();
    TestFalse(TEXT("Gated target cannot disguise held physical stick on manual entry"),P->RequestFlightMode(EDroneFlightMode::Acro));
    P->PadRightY=0; P->UpdatePilotInput();

    // Direct collective handoff starts at the matched absolute input, including below midpoint.
    for (int Hz : {30,60,120})
    {
        P->FlightMode=EDroneFlightMode::Angle; P->ResetDrone(); P->MotorState=EMotorState::Armed;
        C->CollectiveMode=ECollectiveMode::Direct;
        C->Throttle=0.1F; P->CollectiveCommandNewtons=0.1F*P->GetMaximumCollectiveThrustNewtons();
        C->Axes[3].Inner=C->Axes[3].Outer=C->Axes[3].ExpoOffset=0;
        P->PadLeftY=-0.8F; P->RequireNeutralInput(); P->UpdatePilotInput();
        TestFalse(TEXT("Matching direct throttle clears gate"),P->bAwaitingNeutralInput);
        ++GFrameCounter; World->Tick(LEVELTICK_All,1.0F/Hz);
        TestTrue(TEXT("Direct throttle never slews from a 50 percent midpoint"),FMath::IsNearlyEqual(C->Throttle,0.1F,1.e-5F));
        // Pure command shaping must retain small inputs and full authority in every mode.
        for (int Mode=1;Mode<7;++Mode)
        {
            const auto Flight=static_cast<EDroneFlightMode>(Mode);
            for (int Axis=0;Axis<4;++Axis)
            {
                TestEqual(TEXT("All modes full-stick authority"),C->Shape(Axis,1,Flight),1.0F);
                TestTrue(TEXT("All modes small-stick precision"),C->Shape(Axis,0.08F,Flight)>0 && C->Shape(Axis,0.08F,Flight)<0.1F);
            }
        }
    }
    // Exercise actual paused menu navigation, remap capture, and reserved modifier precedence.
    C->CancelHeld(); C->bMenuOpen=true; C->Page=3; C->ButtonIndex=0;
    C->MenuKey(EKeys::Enter); C->Press(EKeys::Gamepad_LeftShoulder); C->Press(EKeys::K);
    TestEqual(TEXT("Interactive capture saves new key"),C->Buttons[0].Key,EKeys::K);
    TestEqual(TEXT("Interactive capture saves chord"),C->Buttons[0].Modifiers,1);
    C->MenuKey(EKeys::Tab); TestTrue(TEXT("Controller map reachable with keyboard"),C->IsMapPage());
    P->PadRightX=0.6F; C->RefreshPausedInput();
    TestEqual(TEXT("Paused calibration sees live raw axis"),C->Raw[2],0.6F);
    TestTrue(TEXT("Paused calibration sees live processed axis"),C->Processed[1]>0);
    TestEqual(TEXT("Paused calibration cannot issue flight command"),P->LateralInputTarget,0.0F);
    C->CancelHeld(); C->bMenuOpen=false;

    // Complete the actual assisted startup/landing workflow against collision geometry.
    AActor* Ground=World->SpawnActor<AActor>();
    UBoxComponent* Floor=NewObject<UBoxComponent>(Ground);
    Ground->SetRootComponent(Floor); Floor->SetBoxExtent(FVector(100,100,5));
    Floor->SetCollisionProfileName(TEXT("BlockAll")); Floor->RegisterComponent();
    Floor->SetWorldLocation(FVector(0,0,-5));
    P->FlightMode=EDroneFlightMode::Sport; C->RestoreDefaults();
    P->Body->SetWorldLocationAndRotation(FVector(0,0,5),FRotator::ZeroRotator);
    P->Body->SetPhysicsLinearVelocity(FVector::ZeroVector); P->Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
    P->PadLeftY=P->PadRightX=0; P->MotorState=EMotorState::Disarmed;
    P->RequireNeutralInput(); P->UpdatePilotInput();
    P->ToggleArm(); TestEqual(TEXT("Grounded neutral deliberate arming succeeds"),P->MotorState,EMotorState::Armed);
    TestTrue(TEXT("Arming alone does not take off"),P->bTakeoffRequired);
    TestTrue(TEXT("HUD explains armed takeoff gate"),P->GetControlStatus().Contains(TEXT("WAITING FOR TAKEOFF")));
    P->RequestTakeoff(); TestFalse(TEXT("Explicit takeoff unlocks motors"),P->bTakeoffRequired);
    for (int I=0;I<300;++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,1.0F/60); }
    TestTrue(TEXT("Assisted takeoff reaches requested height"),P->GetActorLocation().Z>120 && P->GetActorLocation().Z<190);
    P->RequestLanding();
    for (int I=0;I<600;++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,1.0F/60); }
    TestEqual(TEXT("Controlled landing disarms near ground"),P->MotorState,EMotorState::Disarmed);
    TestTrue(TEXT("HUD explains disarmed inputs"),P->GetControlStatus().Contains(TEXT("hold")));
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return true;
}
#endif
