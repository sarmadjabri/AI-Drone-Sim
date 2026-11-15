#include "Flight/PilotControlsComponent.h"
#include "Flight/QuadrotorPawn.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"

bool FPrecisionAxis::IsValid() const
{
    return FMath::IsFinite(Center) && FMath::IsFinite(Minimum) && FMath::IsFinite(Maximum) &&
        Minimum >= -1.0F && Maximum <= 1.0F && Center - Minimum >= 0.1F && Maximum - Center >= 0.1F &&
        FMath::IsFinite(Inner) && Inner >= 0 && Inner <= 0.5F && FMath::IsFinite(Outer) && Outer >= 0 && Outer <= 0.2F &&
        FMath::IsFinite(Sensitivity) && Sensitivity >= 0.1F && Sensitivity <= 2.0F &&
        FMath::IsFinite(ExpoOffset) && ExpoOffset >= -1 && ExpoOffset <= 1 && Source >= 0 && Source < 6;
}
float FPrecisionAxis::Process(float RawValue, float Expo) const
{
    if (!IsValid() || !FMath::IsFinite(RawValue) || !FMath::IsFinite(Expo)) return 0.0F;
    const float Centered = RawValue - Center;
    const float Normalized = Centered / (Centered >= 0 ? Maximum - Center : Center - Minimum);
    const float X = FMath::Clamp((FMath::Abs(Normalized) - Inner) / (1.0F - Inner - Outer), 0.0F, 1.0F);
    const float E = FMath::Clamp(Expo + ExpoOffset, 0.0F, 1.0F);
    return FMath::Clamp(((1.0F - E) * X + E * X * X * X) * Sensitivity, 0.0F, 1.0F) *
        FMath::Sign(Normalized) * (bInvert ? -1.0F : 1.0F);
}
bool FPilotModeTuning::IsValid() const
{
    const float Values[] = {Speed,Tilt,BrakeTilt,Acceleration,Braking,Jerk,BrakeJerk,VelocityGain,BrakeGain,Prediction,
        AttitudeGain,RateDegrees,YawDegrees,Ascent,Descent,VerticalAcceleration,Expo,YawExpo,Slew,HoldGain};
    for (float V : Values) if (!FMath::IsFinite(V) || V < 0) return false;
    return Speed >= 0.5F && Speed <= 30 && Tilt >= 5 && Tilt <= 60 && BrakeTilt >= 5 && BrakeTilt <= 60 &&
        Acceleration >= 0.5F && Acceleration <= 30 && Braking >= 0.5F && Braking <= 30 &&
        Jerk >= 1 && Jerk <= 300 && BrakeJerk >= 1 && BrakeJerk <= 400 && VelocityGain <= 10 && BrakeGain <= 12 &&
        Prediction <= 0.5F && AttitudeGain >= 1 && AttitudeGain <= 12 && RateDegrees >= 30 && RateDegrees <= 360 &&
        YawDegrees >= 15 && YawDegrees <= 180 && Ascent >= 0.1F && Ascent <= 6 && Descent >= 0.1F && Descent <= 4 &&
        VerticalAcceleration >= 1 && VerticalAcceleration <= 8 && Expo <= 1 && YawExpo <= 1 && Slew <= 100 && HoldGain <= 2;
}
UPilotControlsComponent::UPilotControlsComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    RestoreDefaults();
}
void UPilotControlsComponent::RestoreDefaults()
{
    Axes.SetNum(4);
    const int Sources[] = {3,2,0,1};
    for (int I=0; I<4; ++I) { Axes[I] = FPrecisionAxis(); Axes[I].Source = Sources[I]; }
    Modes.Init(FPilotModeTuning(), 7);
    Modes[1].Speed=4.5F; Modes[1].Tilt=20; Modes[1].BrakeTilt=25; Modes[1].Jerk=12; Modes[1].BrakeJerk=20;
    Modes[1].Acceleration=3; Modes[1].Braking=4;
    Modes[1].Expo=0.45F; Modes[1].YawExpo=0.4F; Modes[1].YawDegrees=60; Modes[1].Slew=8;
    Modes[2].Expo=0.25F; Modes[2].YawExpo=0.2F; Modes[2].YawDegrees=110;
    Modes[3].Speed=2.5F; Modes[3].Tilt=12; Modes[3].BrakeTilt=18; Modes[3].Jerk=8; Modes[3].BrakeJerk=16;
    Modes[3].Acceleration=2; Modes[3].Braking=3;
    Modes[3].Expo=0.4F; Modes[3].YawExpo=0.35F; Modes[3].YawDegrees=45; Modes[3].HoldGain=1.0F;
    Modes[4].Speed=20; Modes[4].Tilt=50; Modes[4].BrakeTilt=55; Modes[4].Jerk=180; Modes[4].BrakeJerk=260;
    Modes[4].RateDegrees=220; Modes[4].YawDegrees=150; Modes[4].Ascent=4; Modes[4].Descent=3;
    Modes[4].Expo=0.15F; Modes[4].YawExpo=0.15F; Modes[4].Slew=40; Modes[4].Prediction=0.2F;
    Modes[5].Tilt=40; Modes[5].Expo=0.25F; Modes[5].YawExpo=0.2F; Modes[5].YawDegrees=120;
    Modes[6].RateDegrees=240; Modes[6].YawDegrees=150; Modes[6].Expo=0.2F; Modes[6].YawExpo=0.15F; Modes[6].Slew=0;
    CollectiveMode=ECollectiveMode::Incremental; ThrottleIncrease=0.35F; ThrottleDecrease=0.5F;
    ResetHoldSeconds=1.5F; LeftTriggerRole=ETriggerRole::Unused;
    PrecisionScale=0.25F; CameraSensitivity=1; HoldSeconds=1.5F; TriggerRole=ETriggerRole::Precision;
    Buttons = {
        {EKeys::Gamepad_FaceButton_Bottom,0,EPilotAction::Confirm},
        {EKeys::Gamepad_FaceButton_Right,0,EPilotAction::Cancel},
        {EKeys::Gamepad_FaceButton_Top,0,EPilotAction::Camera}, {EKeys::V,0,EPilotAction::Camera},
        {EKeys::Gamepad_FaceButton_Left,0,EPilotAction::Reset}, {EKeys::BackSpace,0,EPilotAction::Reset},
        {EKeys::Gamepad_Special_Right,0,EPilotAction::Settings}, {EKeys::Escape,0,EPilotAction::Settings},
        {EKeys::Gamepad_Special_Left,0,EPilotAction::Telemetry}, {EKeys::F8,0,EPilotAction::Telemetry},
        {EKeys::F6,0,EPilotAction::Profile}, {EKeys::F7,0,EPilotAction::NextMode},
        {EKeys::Gamepad_DPad_Right,1,EPilotAction::NextMode}, {EKeys::Gamepad_DPad_Left,1,EPilotAction::PreviousMode},
        {EKeys::Gamepad_DPad_Up,1,EPilotAction::IncreaseLimit}, {EKeys::Gamepad_DPad_Down,1,EPilotAction::DecreaseLimit},
        {EKeys::Gamepad_LeftThumbstick,0,EPilotAction::Brake}, {EKeys::Gamepad_RightThumbstick,0,EPilotAction::Recenter},
        {EKeys::Gamepad_RightThumbstick,2,EPilotAction::Recenter},
        {EKeys::Gamepad_DPad_Up,2,EPilotAction::GimbalUp}, {EKeys::Gamepad_DPad_Down,2,EPilotAction::GimbalDown},
        {EKeys::Gamepad_DPad_Left,2,EPilotAction::ZoomOut}, {EKeys::Gamepad_DPad_Right,2,EPilotAction::ZoomIn},
        {EKeys::Gamepad_DPad_Up,0,EPilotAction::ZoomIn}, {EKeys::Gamepad_DPad_Down,0,EPilotAction::ZoomOut},
        {EKeys::PageUp,0,EPilotAction::ZoomIn}, {EKeys::PageDown,0,EPilotAction::ZoomOut},
        {EKeys::Gamepad_FaceButton_Bottom,1,EPilotAction::Takeoff}, {EKeys::Gamepad_FaceButton_Right,1,EPilotAction::Land},
        {EKeys::Gamepad_RightThumbstick,1,EPilotAction::Arm}, {EKeys::M,0,EPilotAction::Arm},
        {EKeys::T,0,EPilotAction::Takeoff}, {EKeys::L,0,EPilotAction::Land}
    };
}
AQuadrotorPawn* UPilotControlsComponent::Drone() const { return Cast<AQuadrotorPawn>(GetOwner()); }
const FPilotModeTuning& UPilotControlsComponent::Tuning(EDroneFlightMode Mode) const
{
    const int I=static_cast<int>(Mode);
    return Modes[Modes.IsValidIndex(I) ? I : 0];
}
float UPilotControlsComponent::Shape(int32 Axis, float Value, EDroneFlightMode Mode) const
{
    return Axes.IsValidIndex(Axis) ? Axes[Axis].Process(Value, Axis==2 ? Tuning(Mode).YawExpo : Tuning(Mode).Expo) : 0;
}
bool UPilotControlsComponent::Validate(FString& Error) const
{
    if (Axes.Num()!=4 || Modes.Num()!=7 || Buttons.Num()<1 || Buttons.Num()>64) { Error=TEXT("Invalid array size"); return false; }
    for (const auto& A: Axes) if (!A.IsValid()) { Error=TEXT("Invalid axis calibration"); return false; }
    for (int I=0; I<4; ++I) for (int J=I+1; J<4; ++J)
        if (Axes[I].Source==Axes[J].Source) { Error=TEXT("Duplicate custom axis source"); return false; }
    for (const auto& M: Modes) if (!M.IsValid()) { Error=TEXT("Mode tuning out of bounds"); return false; }
    if (!FMath::IsFinite(ThrottleIncrease) || ThrottleIncrease<0.05F || ThrottleIncrease>1 ||
        !FMath::IsFinite(ThrottleDecrease) || ThrottleDecrease<0.05F || ThrottleDecrease>1 ||
        !FMath::IsFinite(PrecisionScale) || PrecisionScale<0.1F || PrecisionScale>1 ||
        !FMath::IsFinite(CameraSensitivity) || CameraSensitivity<0.1F || CameraSensitivity>3 ||
        !FMath::IsFinite(HoldSeconds) || HoldSeconds<0.5F || HoldSeconds>5 ||
        !FMath::IsFinite(ResetHoldSeconds) || ResetHoldSeconds<0.5F || ResetHoldSeconds>5 ||
        static_cast<int>(TriggerRole)>4 || static_cast<int>(LeftTriggerRole)>4 || static_cast<int>(CollectiveMode)>1 || static_cast<int>(SavedProfile)>4 || static_cast<int>(SavedMode)>6)
    { Error=TEXT("Invalid throttle/profile/safety setting"); return false; }
    for (int I=0; I<Buttons.Num(); ++I)
    {
        const auto& B=Buttons[I];
        const bool FlightAxisKey=B.Key==EKeys::W || B.Key==EKeys::A || B.Key==EKeys::S || B.Key==EKeys::D ||
            B.Key==EKeys::Q || B.Key==EKeys::E || B.Key==EKeys::R || B.Key==EKeys::F ||
            B.Key==EKeys::SpaceBar || B.Key==EKeys::LeftControl;
        if (FlightAxisKey || (B.Key==EKeys::Escape && B.Action!=EPilotAction::Settings) || !B.Key.IsValid() || B.Key.IsAnalog() || B.Key==EKeys::AnyKey || B.Key==EKeys::Gamepad_LeftShoulder ||
            B.Key==EKeys::Gamepad_RightShoulder || B.Modifiers<0 || B.Modifiers>2 || B.Action>=EPilotAction::Count)
        { Error=TEXT("Invalid or reserved button"); return false; }
        for (int J=0; J<I; ++J) if (B.Key==Buttons[J].Key && B.Modifiers==Buttons[J].Modifiers)
        { Error=TEXT("Duplicate button chord"); return false; }
    }
    return true;
}
int32 UPilotControlsComponent::FindBinding(FKey Key, int32 Modifiers) const
{
    for (int I=0; I<Buttons.Num(); ++I) if (Buttons[I].Key==Key && Buttons[I].Modifiers==Modifiers) return I;
    return INDEX_NONE;
}
void UPilotControlsComponent::Bind(UInputComponent* Input)
{
    FString Error;
    if (!Validate(Error)) { RestoreDefaults(); Status=TEXT("Invalid config restored to defaults: ")+Error; }
    auto& Pressed=Input->BindKey(EKeys::AnyKey,IE_Pressed,this,&UPilotControlsComponent::Press);
    Pressed.bExecuteWhenPaused=true; Pressed.bConsumeInput=false;
    auto& Released=Input->BindKey(EKeys::AnyKey,IE_Released,this,&UPilotControlsComponent::Release);
    Released.bExecuteWhenPaused=true; Released.bConsumeInput=false;
}
void UPilotControlsComponent::CancelHeld()
{
    HeldReset=FKey(); HeldArm=FKey(); ArmSeconds=0; HeldKeys.Reset();
    bPrecision=false; AuxiliaryChannels[0]=AuxiliaryChannels[1]=0;
    bFlightModifier=bCameraModifier=false;
    if (Drone()) Drone()->EndResetHold();
}
void UPilotControlsComponent::Press(FKey Key)
{
    if (HeldKeys.Contains(Key)) return; // OS repeats must not restart holds or retrigger actions.
    HeldKeys.Add(Key);
    if (Key==EKeys::Gamepad_LeftShoulder || Key==EKeys::Gamepad_RightShoulder)
    {
        // A newly introduced modifier cancels any dangerous unmodified hold.
        HeldReset=FKey(); HeldArm=FKey(); ArmSeconds=0;
        if (Drone()) Drone()->EndResetHold();
        if (Key==EKeys::Gamepad_LeftShoulder) bFlightModifier=true; else bCameraModifier=true;
        return;
    }
    if (bMenuOpen) { MenuKey(Key); return; }
    if (Key==EKeys::Escape) { ToggleMenu(); return; }
    const int I=FindBinding(Key,ModifierMask());
    if (I==INDEX_NONE) return;
    const auto Action=Buttons[I].Action;
    if (Action==EPilotAction::Reset) { HeldReset=Key; if (Drone()) Drone()->BeginResetHold(); }
    else if (Action==EPilotAction::Arm) { HeldArm=Key; ArmSeconds=0; }
    else Dispatch(Action);
}
void UPilotControlsComponent::Release(FKey Key)
{
    HeldKeys.Remove(Key);
    if (Key==HeldReset) { HeldReset=FKey(); if (Drone()) Drone()->EndResetHold(); }
    if (Key==HeldArm) { HeldArm=FKey(); ArmSeconds=0; }
    if (Key==EKeys::Gamepad_LeftShoulder || Key==EKeys::Gamepad_RightShoulder)
    {
        // Removing either modifier invalidates a pending chord, including custom reset chords.
        HeldReset=FKey(); HeldArm=FKey(); ArmSeconds=0;
        if (Drone()) Drone()->EndResetHold();
        if (Key==EKeys::Gamepad_LeftShoulder) bFlightModifier=false; else bCameraModifier=false;
    }
}
void UPilotControlsComponent::StepHolds(float Dt)
{
    if (HeldArm.IsValid() && !bMenuOpen)
    {
        ArmSeconds+=FMath::Max(0.0F,Dt);
        if (ArmSeconds>=HoldSeconds) { HeldArm=FKey(); ArmSeconds=0; if (Drone()) Drone()->ToggleArm(); }
    }
}
void UPilotControlsComponent::Dispatch(EPilotAction Action)
{
    AQuadrotorPawn* P=Drone(); if (!P) return;
    switch (Action)
    {
    case EPilotAction::Confirm: Status=TEXT("Ready; open Menu/Options for settings"); break;
    case EPilotAction::Cancel:
        P->bLanding=false; P->AltitudeTargetMeters=P->GetActorLocation().Z/100.0F;
        P->RequireNeutralInput(); Status=TEXT("Operation cancelled"); break;
    case EPilotAction::Camera: P->ToggleViewMode(); break;
    case EPilotAction::Settings: ToggleMenu(); break;
    case EPilotAction::Telemetry: bTelemetry=!bTelemetry; break;
    case EPilotAction::Profile: P->CycleInputProfile(); break;
    case EPilotAction::NextMode: P->RequestFlightMode(static_cast<EDroneFlightMode>((static_cast<int>(P->FlightMode)+1)%7)); break;
    case EPilotAction::PreviousMode: P->RequestFlightMode(static_cast<EDroneFlightMode>((static_cast<int>(P->FlightMode)+6)%7)); break;
    case EPilotAction::IncreaseLimit: case EPilotAction::DecreaseLimit:
    {
        if (P->FlightMode==EDroneFlightMode::LegacyAssisted) { Status=TEXT("Select Cinematic/Sport/Training/High Performance to adjust limits"); return; }
        auto& M=Modes[static_cast<int>(P->FlightMode)];
        const float Sign=Action==EPilotAction::IncreaseLimit ? 1.0F : -1.0F;
        if (P->IsManualMode()) M.RateDegrees=FMath::Clamp(M.RateDegrees+Sign*10,30.0F,360.0F);
        else M.Speed=FMath::Clamp(M.Speed+Sign,0.5F,30.0F);
        break;
    }
    case EPilotAction::Brake:
        if (!P->IsManualMode()) { P->RequireNeutralInput(); Status=TEXT("Braking; hold captures when settled"); }
        else Status=TEXT("Brake/position hold unavailable in manual modes");
        break;
    case EPilotAction::Recenter: P->FirstPersonCameraController->RecenterGimbal(); break;
    case EPilotAction::GimbalUp: case EPilotAction::GimbalDown:
        if (P->ViewMode!=EDroneViewMode::StabilizedFirstPerson) Status=TEXT("Gimbal pitch is available only in stabilized FPV");
        else P->FirstPersonCameraController->AdjustGimbalPitch((Action==EPilotAction::GimbalUp ? 5 : -5)*CameraSensitivity);
        break;
    case EPilotAction::ZoomIn: case EPilotAction::ZoomOut:
        P->AdjustPilotCameraZoom((Action==EPilotAction::ZoomIn ? 1 : -1)*CameraSensitivity); break;
    case EPilotAction::Takeoff: P->RequestTakeoff(); break;
    case EPilotAction::Land: P->RequestLanding(); break;
    default: break;
    }
}
void UPilotControlsComponent::ToggleMenu()
{
    AQuadrotorPawn* P=Drone(); if (!P) return;
    APlayerController* PC=Cast<APlayerController>(P->GetController());
    if (!PC) return;
    if (bMenuOpen) { PC->SetPause(false); bMenuOpen=false; P->RequireNeutralInput(); }
    else { bMenuOpen=PC->SetPause(true); CancelHeld(); }
    bCapturing=false;
}
namespace
{
    struct FModeField { const TCHAR* Label; float FPilotModeTuning::* Value; float Step, Minimum, Maximum; };
    const FModeField ModeFields[] = {
        {TEXT("Speed m/s"),&FPilotModeTuning::Speed,0.5F,0.5F,30},
        {TEXT("Tilt degrees"),&FPilotModeTuning::Tilt,1,5,60},
        {TEXT("Brake tilt degrees"),&FPilotModeTuning::BrakeTilt,1,5,60},
        {TEXT("Acceleration jerk m/s^3"),&FPilotModeTuning::Jerk,2,1,300},
        {TEXT("Braking jerk m/s^3"),&FPilotModeTuning::BrakeJerk,2,1,400},
        {TEXT("Velocity gain /s"),&FPilotModeTuning::VelocityGain,0.1F,0,10},
        {TEXT("Braking gain /s"),&FPilotModeTuning::BrakeGain,0.1F,0,12},
        {TEXT("Prediction seconds"),&FPilotModeTuning::Prediction,0.01F,0,0.5F},
        {TEXT("Attitude gain /s"),&FPilotModeTuning::AttitudeGain,0.5F,1,12},
        {TEXT("Roll/pitch rate deg/s"),&FPilotModeTuning::RateDegrees,5,30,360},
        {TEXT("Yaw rate deg/s"),&FPilotModeTuning::YawDegrees,5,15,180},
        {TEXT("Ascent m/s"),&FPilotModeTuning::Ascent,0.1F,0.1F,6},
        {TEXT("Descent m/s"),&FPilotModeTuning::Descent,0.1F,0.1F,4},
        {TEXT("Vertical acceleration m/s^2"),&FPilotModeTuning::VerticalAcceleration,0.2F,1,8},
        {TEXT("Pitch/roll expo"),&FPilotModeTuning::Expo,0.05F,0,1},
        {TEXT("Yaw expo"),&FPilotModeTuning::YawExpo,0.05F,0,1},
        {TEXT("Input slew normalized/s (0=off)"),&FPilotModeTuning::Slew,1,0,100},
        {TEXT("Acceleration cap m/s^2"),&FPilotModeTuning::Acceleration,0.5F,0.5F,30},
        {TEXT("Braking cap m/s^2"),&FPilotModeTuning::Braking,0.5F,0.5F,30},
        {TEXT("Position hold gain /s"),&FPilotModeTuning::HoldGain,0.1F,0,2}
    };

    FString ActionName(EPilotAction Action)
    {
        return StaticEnum<EPilotAction>()->GetNameStringByValue(static_cast<int64>(Action));
    }
}
FString UPilotControlsComponent::SlotPath(int Slot) const
{
    return FConfigCacheIni::NormalizeConfigIniPath(FPaths::ConvertRelativePathToFull(
        (ProfileDirectory.IsEmpty() ? FPaths::ProjectSavedDir()/TEXT("Controls") : ProfileDirectory)/FString::Printf(TEXT("Custom%d.ini"),Slot)));
}
bool UPilotControlsComponent::SaveSlot(int32 Slot)
{
    if (Slot<1 || Slot>2) return false;
    if (Drone()) { SavedProfile=Drone()->InputProfile; SavedMode=Drone()->FlightMode; ResetHoldSeconds=Drone()->ResetHoldSeconds; }
    FString Error; if (!Validate(Error)) { Status=Error; return false; }
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(SlotPath(Slot)),true);
    const FString Path=SlotPath(Slot);
    SaveConfig(CPF_Config,*Path,GConfig,false);
    const bool Saved=IFileManager::Get().FileSize(*Path)>0;
    Status=Saved ? TEXT("Saved ")+Path : TEXT("Could not save profile");
    return Saved;
}
bool UPilotControlsComponent::LoadSlot(int32 Slot)
{
    if (Slot<1 || Slot>2 || !IFileManager::Get().FileExists(*SlotPath(Slot))) { Status=TEXT("Profile file missing"); return false; }
    AQuadrotorPawn* P=Drone();
    if (P && (P->MotorState!=EMotorState::Disarmed || !P->IsGrounded()))
    { Status=TEXT("Land and disarm before loading a profile"); return false; }
    UPilotControlsComponent* Candidate=NewObject<UPilotControlsComponent>();
    GConfig->UnloadFile(SlotPath(Slot));
    Candidate->LoadConfig(GetClass(),*SlotPath(Slot));
    FString Error; if (!Candidate->Validate(Error)) { Status=Error; return false; }
    if (P && Candidate->SavedMode>=EDroneFlightMode::Angle && !P->IsGrounded())
    { Status=TEXT("Land before loading a manual-flight profile"); return false; }
    Axes=Candidate->Axes; Modes=Candidate->Modes; Buttons=Candidate->Buttons;
    CollectiveMode=Candidate->CollectiveMode; ThrottleIncrease=Candidate->ThrottleIncrease; ThrottleDecrease=Candidate->ThrottleDecrease;
    PrecisionScale=Candidate->PrecisionScale; CameraSensitivity=Candidate->CameraSensitivity;
    HoldSeconds=Candidate->HoldSeconds; ResetHoldSeconds=Candidate->ResetHoldSeconds; TriggerRole=Candidate->TriggerRole; LeftTriggerRole=Candidate->LeftTriggerRole;
    SavedMode=Candidate->SavedMode; SavedProfile=Candidate->SavedProfile;
    CancelHeld();
    if (P)
    {
        P->ResetHoldSeconds=ResetHoldSeconds;
        P->InputProfile=SavedProfile;
        // Loading is a grounded/disarmed configuration transaction. Neutral/low-throttle
        // confirmation is enforced before arming; never report success after a refused mode change.
        P->FlightMode=SavedMode;
        P->AltitudeTargetMeters=P->GetActorLocation().Z/100.0F;
        P->YawTargetDegrees=P->GetActorRotation().Yaw;
        Throttle=0;
        P->RequireNeutralInput();
    }
    Status=TEXT("Loaded ")+SlotPath(Slot); return true;
}
void UPilotControlsComponent::MenuKey(FKey Key)
{
    if (bCapturing)
    {
        if (Key==EKeys::Escape) { bCapturing=false; return; }
        FPilotButton Old=Buttons[ButtonIndex];
        Buttons[ButtonIndex].Key=Key; Buttons[ButtonIndex].Modifiers=ModifierMask();
        FString Error;
        if (!Validate(Error)) { Buttons[ButtonIndex]=Old; Status=Error; }
        else { Status=TEXT("Binding changed"); bCapturing=false; }
        return;
    }
    if (Key==EKeys::Escape || Key==EKeys::Gamepad_FaceButton_Right || Key==EKeys::Gamepad_Special_Right) { ToggleMenu(); return; }
    if (Key==EKeys::Tab || Key==EKeys::Gamepad_FaceButton_Top) { Page=(Page+1)%5; Row=0; return; }
    const int Counts[]={15,10,20,2,1};
    if (Key==EKeys::Up || Key==EKeys::Gamepad_DPad_Up) Row=(Row+Counts[Page]-1)%Counts[Page];
    else if (Key==EKeys::Down || Key==EKeys::Gamepad_DPad_Down) Row=(Row+1)%Counts[Page];
    else if (Key==EKeys::Left || Key==EKeys::Gamepad_DPad_Left) Adjust(-1);
    else if (Key==EKeys::Right || Key==EKeys::Gamepad_DPad_Right) Adjust(1);
    else if (Key==EKeys::Enter || Key==EKeys::Gamepad_FaceButton_Bottom)
    {
        if (Page==3) { bCapturing=true; Status=TEXT("Hold LB/L1 or RB/R1, then press the new button; Esc cancels"); }
        else if (Page==0 && Row==11) SaveSlot(SlotIndex);
        else if (Page==0 && Row==12) LoadSlot(SlotIndex);
        else if (Page==0 && Row==13)
        {
            if (Drone() && Drone()->IsManualMode() && Drone()->MotorState!=EMotorState::Disarmed)
                Status=TEXT("Disarm before restoring defaults");
            else { RestoreDefaults(); Drone()->ResetHoldSeconds=ResetHoldSeconds; Status=TEXT("Defaults restored"); }
        }
        else Adjust(1);
    }
}
void UPilotControlsComponent::Adjust(int32 D)
{
    AQuadrotorPawn* P=Drone(); if (!P) return;
    if ((Page==1 || (Page==0 && Row>=2 && Row<=4)) && P->IsManualMode() && P->MotorState!=EMotorState::Disarmed)
    { Status=TEXT("Disarm before changing manual axis/throttle interpretation"); return; }
    if (Page==0)
    {
        switch (Row)
        {
        case 0: P->CycleInputProfile(); break;
        case 1: P->RequestFlightMode(static_cast<EDroneFlightMode>((static_cast<int>(P->FlightMode)+D+7)%7)); break;
        case 2: CollectiveMode=CollectiveMode==ECollectiveMode::Incremental ? ECollectiveMode::Direct : ECollectiveMode::Incremental; Throttle=0; break;
        case 3: ThrottleIncrease=FMath::Clamp(ThrottleIncrease+D*0.05F,0.05F,1.0F); break;
        case 4: ThrottleDecrease=FMath::Clamp(ThrottleDecrease+D*0.05F,0.05F,1.0F); break;
        case 5: PrecisionScale=FMath::Clamp(PrecisionScale+D*0.05F,0.1F,1.0F); break;
        case 6: TriggerRole=static_cast<ETriggerRole>((static_cast<int>(TriggerRole)+D+5)%5); break;
        case 14: LeftTriggerRole=static_cast<ETriggerRole>((static_cast<int>(LeftTriggerRole)+D+5)%5); break;
        case 7: CameraSensitivity=FMath::Clamp(CameraSensitivity+D*0.1F,0.1F,3.0F); break;
        case 8: HoldSeconds=FMath::Clamp(HoldSeconds+D*0.25F,0.5F,5.0F); break;
        case 9: P->ResetHoldSeconds=FMath::Clamp(P->ResetHoldSeconds+D*0.25F,0.5F,5.0F); break;
        case 10: SlotIndex=3-SlotIndex; break;
        default: break;
        }
    }
    else if (Page==1)
    {
        if (Row==0) { AxisIndex=(AxisIndex+D+4)%4; return; }
        auto& A=Axes[AxisIndex]; const FPrecisionAxis Old=A;
        switch (Row)
        {
        case 1:
        {
            const int Source=(A.Source+D+6)%6;
            for (int I=0; I<4; ++I) if (I!=AxisIndex && Axes[I].Source==Source) Axes[I].Source=A.Source;
            A.Source=Source; P->InputProfile=EDroneInputProfile::Custom; break;
        }
        case 2: A.bInvert=!A.bInvert; break;
        case 3: A.Inner=FMath::Clamp(A.Inner+D*0.01F,0.0F,0.5F); break;
        case 4: A.Outer=FMath::Clamp(A.Outer+D*0.01F,0.0F,0.2F); break;
        case 5: A.Center+=D*0.01F; break;
        case 6: A.Minimum=FMath::Clamp(A.Minimum+D*0.01F,-1.0F,0.0F); break;
        case 7: A.Maximum=FMath::Clamp(A.Maximum+D*0.01F,0.0F,1.0F); break;
        case 8: A.Sensitivity=FMath::Clamp(A.Sensitivity+D*0.05F,0.1F,2.0F); break;
        case 9: A.ExpoOffset=FMath::Clamp(A.ExpoOffset+D*0.05F,-1.0F,1.0F); break;
        }
        if (!A.IsValid()) { A=Old; Status=TEXT("Calibration range must span its center"); }
    }
    else if (Page==2)
    {
        if (P->FlightMode==EDroneFlightMode::LegacyAssisted) { Status=TEXT("Original legacy tuning is read-only; select a configurable mode"); return; }
        auto& M=Modes[static_cast<int>(P->FlightMode)];
        const auto& F=ModeFields[Row]; M.*(F.Value)=FMath::Clamp(M.*(F.Value)+D*F.Step,F.Minimum,F.Maximum);
        if (P->FlightMode==EDroneFlightMode::LegacyAssisted) Status=TEXT("Legacy mode preserves original pawn tuning; select another mode to use these values");
    }
    else if (Page==3)
    {
        if (Row==0) ButtonIndex=(ButtonIndex+D+Buttons.Num())%Buttons.Num();
        else { Buttons[ButtonIndex].Modifiers=(Buttons[ButtonIndex].Modifiers+D+3)%3;
            FString Error; if (!Validate(Error)) { Buttons[ButtonIndex].Modifiers=(Buttons[ButtonIndex].Modifiers-D+3)%3; Status=Error; } }
    }
}
FString UPilotControlsComponent::ActionBindings(EPilotAction Action) const
{
    FString Out;
    for (const auto& B:Buttons)
    {
        if (B.Action!=Action) continue;
        if (!Out.IsEmpty()) Out+=TEXT(" or ");
        if (B.Modifiers==1) Out+=TEXT("LB/L1 + ");
        if (B.Modifiers==2) Out+=TEXT("RB/R1 + ");
        Out+=ButtonLabel(B.Key);
    }
    return Out.IsEmpty() ? TEXT("unassigned (open settings)") : Out;
}
FString UPilotControlsComponent::BindingLabel(FKey Key) const
{
    if (Key==EKeys::Gamepad_LeftShoulder) return TEXT("Flight modifier");
    if (Key==EKeys::Gamepad_RightShoulder) return TEXT("Camera modifier");
    if (Key==EKeys::Gamepad_LeftTrigger || Key==EKeys::Gamepad_RightTrigger)
        return StaticEnum<ETriggerRole>()->GetNameStringByValue(static_cast<int64>(
            Key==EKeys::Gamepad_LeftTrigger ? LeftTriggerRole : TriggerRole));
    const int Index=FindBinding(Key,ModifierMask());
    return Index==INDEX_NONE ? TEXT("Unassigned") : ActionName(Buttons[Index].Action);
}
void UPilotControlsComponent::UpdateAuxiliary(bool LeftAvailable, bool RightAvailable)
{
    bPrecision=false;
    const bool Available[]={LeftAvailable,RightAvailable};
    const ETriggerRole Roles[]={LeftTriggerRole,TriggerRole};
    for (int I=0; I<2; ++I)
    {
        const float Value=Available[I] ? FMath::Clamp(Raw[4+I],0.0F,1.0F) : 0;
        AuxiliaryChannels[I]=Roles[I]==ETriggerRole::Auxiliary ? Value : 0;
        bPrecision |= Roles[I]==ETriggerRole::Precision && Value>0.5F;
    }
}
FString UPilotControlsComponent::ButtonLabel(FKey Key)
{
    // Unreal normalizes Xbox and DualSense to the same positional gamepad keys.
    // Paired labels also work when a driver does not expose the controller brand.
    if (Key==EKeys::Gamepad_FaceButton_Bottom) return TEXT("A / Cross");
    if (Key==EKeys::Gamepad_FaceButton_Right) return TEXT("B / Circle");
    if (Key==EKeys::Gamepad_FaceButton_Left) return TEXT("X / Square");
    if (Key==EKeys::Gamepad_FaceButton_Top) return TEXT("Y / Triangle");
    if (Key==EKeys::Gamepad_LeftShoulder) return TEXT("LB / L1");
    if (Key==EKeys::Gamepad_RightShoulder) return TEXT("RB / R1");
    if (Key==EKeys::Gamepad_LeftTrigger) return TEXT("LT / L2");
    if (Key==EKeys::Gamepad_RightTrigger) return TEXT("RT / R2");
    if (Key==EKeys::Gamepad_LeftThumbstick) return TEXT("LS click / L3");
    if (Key==EKeys::Gamepad_RightThumbstick) return TEXT("RS click / R3");
    if (Key==EKeys::Gamepad_Special_Left) return TEXT("View (Back) / Create");
    if (Key==EKeys::Gamepad_Special_Right) return TEXT("Menu (Start) / Options");
    return Key.GetDisplayName().ToString();
}
void UPilotControlsComponent::RefreshPausedInput()
{
    // Actor flight ticks stop while paused; axis bindings continue so calibration stays live.
    if (bMenuOpen && Drone()) Drone()->UpdatePilotInput();
}
FString UPilotControlsComponent::Diagnostics() const
{
    float Live[6]; for (int I=0; I<6; ++I) Live[I]=Raw[I];
    float Shaped[4]; for (int I=0; I<4; ++I) Shaped[I]=Processed[I];
    if (const auto* P=Drone())
    {
        Live[0]=P->PadLeftX; Live[1]=P->PadLeftY; Live[2]=P->PadRightX;
        Live[3]=P->PadRightY; Live[4]=P->PadLeftTrigger; Live[5]=P->PadRightTrigger;

    }
    FString Keys;
    for (const FKey& Key : HeldKeys) Keys+=ButtonLabel(Key)+TEXT(" ");
    return FString::Printf(TEXT("Raw LX/LY/RX/RY: %+.3f %+.3f %+.3f %+.3f | LT/RT %.3f %.3f\nProcessed pitch/roll/yaw/vertical: %+.3f %+.3f %+.3f %+.3f\nInner dead zones: %.2f %.2f %.2f %.2f | neutral: %d %d %d %d\nModifiers LB/L1:%d RB/R1:%d | precision:%d | arm hold %.1f/%.1fs\nAuxiliary 1/2: %.2f %.2f\nButtons: %s\n%s"),
        Live[0],Live[1],Live[2],Live[3],Live[4],Live[5],Shaped[0],Shaped[1],Shaped[2],Shaped[3],
        Axes[0].Inner,Axes[1].Inner,Axes[2].Inner,Axes[3].Inner,
        FMath::IsNearlyZero(Shaped[0]),FMath::IsNearlyZero(Shaped[1]),FMath::IsNearlyZero(Shaped[2]),FMath::IsNearlyZero(Shaped[3]),
        bFlightModifier,bCameraModifier,bPrecision,ArmSeconds,HoldSeconds,AuxiliaryChannels[0],AuxiliaryChannels[1],*Keys,*Status);
}
FString UPilotControlsComponent::DescribeMenu() const
{
    const AQuadrotorPawn* P=Drone(); if (!P) return TEXT("");
    FString Out=TEXT("CONTROLS — PAUSED: inputs shown here do not move the aircraft\nTab/Y/Triangle: page | arrows/D-pad: select/change | Enter/A/Cross: confirm | Esc/B/Circle: back\n");
    const auto Add=[&](int I,const FString& Text) { Out+=(Row==I ? TEXT("> ") : TEXT("  "))+Text+TEXT("\n"); };
    if (Page==0)
    {
        Add(0,TEXT("Profile: ")+P->GetInputProfileName()); Add(1,TEXT("Mode: ")+P->GetFlightModeName());
        Add(2,FString(TEXT("Manual throttle: "))+(CollectiveMode==ECollectiveMode::Incremental ? TEXT("Incremental (center holds)") : TEXT("Direct (center = 50%)")));
        Add(3,FString::Printf(TEXT("Throttle increase /s: %.2f"),ThrottleIncrease));
        Add(4,FString::Printf(TEXT("Throttle decrease /s: %.2f"),ThrottleDecrease));
        Add(5,FString::Printf(TEXT("Precision scale: %.2f"),PrecisionScale)); Add(6,TEXT("RT/R2 role: ")+StaticEnum<ETriggerRole>()->GetNameStringByValue(static_cast<int64>(TriggerRole)));
        Add(7,FString::Printf(TEXT("Camera sensitivity: %.2f"),CameraSensitivity)); Add(8,FString::Printf(TEXT("Arm hold seconds: %.2f"),HoldSeconds));
        Add(9,FString::Printf(TEXT("Reset hold seconds: %.2f"),P->ResetHoldSeconds)); Add(10,FString::Printf(TEXT("Custom slot: %d"),SlotIndex));
        Add(11,TEXT("Save profile")); Add(12,TEXT("Load profile")); Add(13,TEXT("Restore defaults"));
        Add(14,TEXT("LT/L2 role: ")+StaticEnum<ETriggerRole>()->GetNameStringByValue(static_cast<int64>(LeftTriggerRole)));
    }
    else if (Page==1)
    {
        const auto& A=Axes[AxisIndex]; const TCHAR* Names[]={TEXT("Pitch/forward"),TEXT("Roll/lateral"),TEXT("Yaw"),TEXT("Vertical")};
        const TCHAR* Sources[]={TEXT("LX"),TEXT("LY"),TEXT("RX"),TEXT("RY"),TEXT("LT (signed)"),TEXT("RT (signed)")};
        Add(0,FString(TEXT("Axis: "))+Names[AxisIndex]); Add(1,FString(TEXT("Custom source: "))+Sources[A.Source]);
        Add(2,FString::Printf(TEXT("Invert: %d"),A.bInvert)); Add(3,FString::Printf(TEXT("Inner dead zone: %.2f"),A.Inner));
        Add(4,FString::Printf(TEXT("Outer dead zone: %.2f"),A.Outer)); Add(5,FString::Printf(TEXT("Center: %.2f"),A.Center));
        Add(6,FString::Printf(TEXT("Minimum: %.2f"),A.Minimum)); Add(7,FString::Printf(TEXT("Maximum: %.2f"),A.Maximum));
        Add(8,FString::Printf(TEXT("Sensitivity: %.2f"),A.Sensitivity)); Add(9,FString::Printf(TEXT("Expo offset: %.2f"),A.ExpoOffset));
    }
    else if (Page==2)
    {
        Out+=P->GetFlightModeName()+TEXT("\n"); const auto& M=Tuning(P->FlightMode);
        for (int I=0; I<20; ++I) Add(I,FString::Printf(TEXT("%s: %.2f"),ModeFields[I].Label,M.*(ModeFields[I].Value)));
    }
    else if (Page==3)
    {
        const auto& B=Buttons[ButtonIndex]; Add(0,ActionName(B.Action)+TEXT(" : ")+ButtonLabel(B.Key));
        Add(1,FString::Printf(TEXT("Modifier: %d (0 none, 1 LB/L1, 2 RB/R1)"),B.Modifiers));
        Out+=bCapturing ? TEXT("Listening for button...\n") : TEXT("A/Cross/Enter: select and press a new button\n");
    }
    if (Page==4) return Out+TEXT("Controller map: hold LB/L1 or RB/R1 to inspect modified actions. Active controls glow.\n");
    return Out+TEXT("\n")+Diagnostics();
}
