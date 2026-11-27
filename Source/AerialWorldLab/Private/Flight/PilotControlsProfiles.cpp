// Validated profile save/load transactions; serialized property names remain stable.
#include "Flight/PilotControlsComponent.h"
#include "Components/InputComponent.h"
#include "Flight/QuadrotorPawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

FString UPilotControlsComponent::SlotPath(int Slot) const
{
    FString Directory = ProfileDirectory;
    if (Directory.IsEmpty())
    {
        Directory = FPaths::ProjectSavedDir() / TEXT("Controls");
    }
    const FString Filename = FString::Printf(TEXT("Custom%d.ini"), Slot);
    return FConfigCacheIni::NormalizeConfigIniPath(
        FPaths::ConvertRelativePathToFull(Directory / Filename));
}

bool UPilotControlsComponent::SaveSlot(int32 Slot)
{
    if (Slot < 1 || Slot > 2)
    {
        return false;
    }
    if (Drone())
    {
        SavedProfile = Drone()->InputProfile;
        SavedMode = Drone()->FlightMode;
        ResetHoldSeconds = Drone()->ResetHoldSeconds;
    }
    FString Error;
    if (!Validate(Error))
    {
        Status = Error;
        return false;
    }
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(SlotPath(Slot)), true);
    const FString Path = SlotPath(Slot);
    SaveConfig(CPF_Config, *Path, GConfig, false);
    const bool Saved = IFileManager::Get().FileSize(*Path) > 0;
    if (Saved)
    {
        Status = TEXT("Saved ") + Path;
    }
    else
    {
        Status = TEXT("Could not save profile");
    }
    return Saved;
}

bool UPilotControlsComponent::LoadSlot(int32 Slot)
{
    if (Slot < 1 || Slot > 2 || !IFileManager::Get().FileExists(*SlotPath(Slot)))
    {
        Status = TEXT("Profile file missing");
        return false;
    }
    AQuadrotorPawn *DronePawn = Drone();
    if (DronePawn && (DronePawn->MotorState != EMotorState::Disarmed || !DronePawn->IsGrounded()))
    {
        Status = TEXT("Land and disarm before loading a profile");
        return false;
    }
    UPilotControlsComponent *Candidate = NewObject<UPilotControlsComponent>();
    GConfig->UnloadFile(SlotPath(Slot));
    Candidate->LoadConfig(GetClass(), *SlotPath(Slot));
    FString Error;
    if (!Candidate->Validate(Error))
    {
        Status = Error;
        return false;
    }
    if (DronePawn && Candidate->SavedMode >= EDroneFlightMode::Angle && !DronePawn->IsGrounded())
    {
        Status = TEXT("Land before loading a manual-flight profile");
        return false;
    }
    Axes = Candidate->Axes;
    Modes = Candidate->Modes;
    Buttons = Candidate->Buttons;
    CollectiveMode = Candidate->CollectiveMode;
    ThrottleIncrease = Candidate->ThrottleIncrease;
    ThrottleDecrease = Candidate->ThrottleDecrease;
    PrecisionScale = Candidate->PrecisionScale;
    CameraSensitivity = Candidate->CameraSensitivity;
    HoldSeconds = Candidate->HoldSeconds;
    ResetHoldSeconds = Candidate->ResetHoldSeconds;
    TriggerRole = Candidate->TriggerRole;
    LeftTriggerRole = Candidate->LeftTriggerRole;
    SavedMode = Candidate->SavedMode;
    SavedProfile = Candidate->SavedProfile;
    CancelHeld();
    if (DronePawn)
    {
        DronePawn->ResetHoldSeconds = ResetHoldSeconds;
        DronePawn->InputProfile = SavedProfile;
        // Loading is a grounded/disarmed configuration transaction. Neutral/low-throttle
        // confirmation is enforced before arming; never report success after a refused mode change.
        DronePawn->FlightMode = SavedMode;
        DronePawn->AltitudeTargetMeters = DronePawn->GetActorLocation().Z / 100.0F;
        DronePawn->YawTargetDegrees = DronePawn->GetActorRotation().Yaw;
        Throttle = 0;
        DronePawn->RequireNeutralInput();
    }
    Status = TEXT("Loaded ") + SlotPath(Slot);
    return true;
}
