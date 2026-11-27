// Controller labels and diagnostic text, separated from input processing.
#include "Flight/PilotControlsComponent.h"
#include "Components/InputComponent.h"
#include "Flight/QuadrotorPawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

FString UPilotControlsComponent::GetActionName(EPilotAction Action)
{
    return StaticEnum<EPilotAction>()->GetNameStringByValue(static_cast<int64>(Action));
}

FString UPilotControlsComponent::ActionBindings(EPilotAction Action) const
{
    FString Description;
    for (const auto &B : Buttons)
    {
        if (B.Action != Action)
        {
            continue;
        }
        if (!Description.IsEmpty())
        {
            Description += TEXT(" or ");
        }
        if (B.Modifiers == 1)
        {
            Description += TEXT("LB/L1 + ");
        }
        if (B.Modifiers == 2)
        {
            Description += TEXT("RB/R1 + ");
        }
        Description += ButtonLabel(B.Key);
    }
    if (Description.IsEmpty())
    {
        return TEXT("unassigned (open settings)");
    }
    return Description;
}

FString UPilotControlsComponent::BindingLabel(FKey Key) const
{
    if (Key == EKeys::Gamepad_LeftShoulder)
    {
        return TEXT("Flight modifier");
    }
    if (Key == EKeys::Gamepad_RightShoulder)
    {
        return TEXT("Camera modifier");
    }
    if (Key == EKeys::Gamepad_LeftTrigger || Key == EKeys::Gamepad_RightTrigger)
    {
        ETriggerRole Role = TriggerRole;
        if (Key == EKeys::Gamepad_LeftTrigger)
        {
            Role = LeftTriggerRole;
        }
        return StaticEnum<ETriggerRole>()->GetNameStringByValue(static_cast<int64>(Role));
    }
    const int Index = FindBinding(Key, ModifierMask());
    if (Index == INDEX_NONE)
    {
        return TEXT("Unassigned");
    }
    return GetActionName(Buttons[Index].Action);
}

FString UPilotControlsComponent::ButtonLabel(FKey Key)
{
    // Unreal normalizes Xbox and DualSense to the same positional gamepad keys.
    // Paired labels also work when a driver does not expose the controller brand.
    if (Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        return TEXT("A / Cross");
    }
    if (Key == EKeys::Gamepad_FaceButton_Right)
    {
        return TEXT("B / Circle");
    }
    if (Key == EKeys::Gamepad_FaceButton_Left)
    {
        return TEXT("X / Square");
    }
    if (Key == EKeys::Gamepad_FaceButton_Top)
    {
        return TEXT("Y / Triangle");
    }
    if (Key == EKeys::Gamepad_LeftShoulder)
    {
        return TEXT("LB / L1");
    }
    if (Key == EKeys::Gamepad_RightShoulder)
    {
        return TEXT("RB / R1");
    }
    if (Key == EKeys::Gamepad_LeftTrigger)
    {
        return TEXT("LT / L2");
    }
    if (Key == EKeys::Gamepad_RightTrigger)
    {
        return TEXT("RT / R2");
    }
    if (Key == EKeys::Gamepad_LeftThumbstick)
    {
        return TEXT("LS click / L3");
    }
    if (Key == EKeys::Gamepad_RightThumbstick)
    {
        return TEXT("RS click / R3");
    }
    if (Key == EKeys::Gamepad_Special_Left)
    {
        return TEXT("View (Back) / Create");
    }
    if (Key == EKeys::Gamepad_Special_Right)
    {
        return TEXT("Menu (Start) / Options");
    }
    return Key.GetDisplayName().ToString();
}

FString UPilotControlsComponent::Diagnostics() const
{
    float Live[6];
    for (int Index = 0; Index < 6; ++Index)
    {
        Live[Index] = Raw[Index];
    }
    float Shaped[4];
    for (int Index = 0; Index < 4; ++Index)
    {
        Shaped[Index] = Processed[Index];
    }
    if (const auto *DronePawn = Drone())
    {
        Live[0] = DronePawn->PadLeftX;
        Live[1] = DronePawn->PadLeftY;
        Live[2] = DronePawn->PadRightX;
        Live[3] = DronePawn->PadRightY;
        Live[4] = DronePawn->PadLeftTrigger;
        Live[5] = DronePawn->PadRightTrigger;
    }
    FString Keys;
    for (const FKey &Key : HeldKeys)
    {
        Keys += ButtonLabel(Key) + TEXT(" ");
    }
    return FString::Printf(
        TEXT("Raw LX/LY/RX/RY: %+.3f %+.3f %+.3f %+.3f | LT/RT %.3f %.3f\nProcessed "
             "pitch/roll/yaw/vertical: %+.3f %+.3f %+.3f %+.3f\nInner dead zones: %.2f %.2f %.2f "
             "%.2f | neutral: %d %d %d %d\nModifiers LB/L1:%d RB/R1:%d | precision:%d | arm hold "
             "%.1f/%.1fs\nAuxiliary 1/2: %.2f %.2f\nButtons: %s\n%s"),
        Live[0], Live[1], Live[2], Live[3], Live[4], Live[5], Shaped[0], Shaped[1], Shaped[2],
        Shaped[3], Axes[0].Inner, Axes[1].Inner, Axes[2].Inner, Axes[3].Inner,
        FMath::IsNearlyZero(Shaped[0]), FMath::IsNearlyZero(Shaped[1]),
        FMath::IsNearlyZero(Shaped[2]), FMath::IsNearlyZero(Shaped[3]), bFlightModifier,
        bCameraModifier, bPrecision, ArmSeconds, HoldSeconds, AuxiliaryChannels[0],
        AuxiliaryChannels[1], *Keys, *Status);
}
