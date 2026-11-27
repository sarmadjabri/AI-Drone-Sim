// Validates controller calibration, routes actions and persists editable pilot profiles.
#pragma once
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Flight/PilotInput.h"
#include "Flight/FlightModeTuning.h"
#include "InputCoreTypes.h"
#include "PilotControlsComponent.generated.h"

UENUM()
enum class ECollectiveMode : uint8
{
    Incremental,
    Direct
};
UENUM()
enum class ETriggerRole : uint8
{
    Unused,
    Precision,
    Zoom,
    Gimbal,
    Auxiliary
};
UENUM()
enum class EPilotAction : uint8
{
    Camera,
    Reset,
    Settings,
    Telemetry,
    Profile,
    NextMode,
    PreviousMode,
    IncreaseLimit,
    DecreaseLimit,
    Brake,
    Recenter,
    GimbalUp,
    GimbalDown,
    ZoomIn,
    ZoomOut,
    Takeoff,
    Land,
    Arm,
    Confirm,
    Cancel,
    Count
};
UENUM()
enum class EMotorState : uint8
{
    Disarmed,
    Armed,
    Failsafe
};

USTRUCT()

struct FPrecisionAxis
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere)
    float Center = 0.0F;
    UPROPERTY(EditAnywhere)
    float Minimum = -1.0F;
    UPROPERTY(EditAnywhere)
    float Maximum = 1.0F;
    UPROPERTY(EditAnywhere)
    float Inner = 0.05F;
    UPROPERTY(EditAnywhere)
    float Outer = 0.02F;
    UPROPERTY(EditAnywhere)
    float Sensitivity = 1.0F;
    UPROPERTY(EditAnywhere)
    float ExpoOffset = 0.0F;
    UPROPERTY(EditAnywhere)
    bool bInvert = false;
    UPROPERTY(EditAnywhere)
    int32 Source = 0;
    bool IsValid() const;
    float Process(float Raw, float Expo) const;
};

USTRUCT()

struct FPilotButton
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere)
    FKey Key;
    // Exact mask: 0 unmodified, 1 LB/L1, 2 RB/R1. Both modifiers suppress commands.
    UPROPERTY(EditAnywhere)
    int32 Modifiers = 0;
    UPROPERTY(EditAnywhere)
    EPilotAction Action = EPilotAction::Camera;
    FPilotButton() = default;

    FPilotButton(FKey InKey, int32 InModifiers, EPilotAction InAction)
        : Key(InKey), Modifiers(InModifiers), Action(InAction)
    {
    }
};

class AQuadrotorPawn;
class UInputComponent;

UCLASS(ClassGroup = (Drone), Config = Game)

class AERIALWORLDLAB_API UPilotControlsComponent : public UActorComponent
{
    GENERATED_BODY()
  public:
    UPilotControlsComponent();
    void Bind(UInputComponent *Input);
    void Press(FKey Key);
    void Release(FKey Key);
    void CancelHeld();
    void ToggleMenu();
    void RestoreDefaults();
    bool Validate(FString &Error) const;
    bool SaveSlot(int32 Slot);
    bool LoadSlot(int32 Slot);
    FString DescribeMenu() const;
    FString Diagnostics() const;
    void RefreshPausedInput();
    static FString ButtonLabel(FKey Key);
    FString BindingLabel(FKey Key) const;
    FString ActionBindings(EPilotAction Action) const;

    bool IsKeyHeld(FKey Key) const
    {
        return HeldKeys.Contains(Key);
    }

    bool IsMapPage() const;

    void UpdateAuxiliary(bool LeftAvailable, bool RightAvailable);
    float AuxiliaryChannels[2]{};
    const FPilotModeTuning &Tuning(EDroneFlightMode Mode) const;
    float Shape(int32 Axis, float Raw, EDroneFlightMode Mode) const;
    int32 FindBinding(FKey Key, int32 Modifiers) const;

    bool IsMenuOpen() const
    {
        return bMenuOpen;
    }

    bool IsTelemetryVisible() const
    {
        return bTelemetry;
    }

    // Independent bits allow both modifiers to be active at the same time.
    int32 ModifierMask() const
    {
        int32 Mask = 0;
        if (bFlightModifier)
        {
            Mask |= 1;
        }
        if (bCameraModifier)
        {
            Mask |= 2;
        }
        return Mask;
    }

    void StepHolds(float DeltaSeconds);
    float Raw[Aether::PhysicalAxis::Count]{};
    float Processed[Aether::CommandAxis::Count]{};
    float Throttle = 0.0F;
    FString Status;
    bool bPrecision = false;
    UPROPERTY(EditAnywhere, Config)
    TArray<FPrecisionAxis> Axes;
    UPROPERTY(EditAnywhere, Config)
    TArray<FPilotModeTuning> Modes;
    UPROPERTY(EditAnywhere, Config)
    TArray<FPilotButton> Buttons;
    UPROPERTY(EditAnywhere, Config)
    ECollectiveMode CollectiveMode = ECollectiveMode::Incremental;
    UPROPERTY(EditAnywhere, Config)
    float ThrottleIncrease = 0.35F;
    UPROPERTY(EditAnywhere, Config)
    float ThrottleDecrease = 0.5F;
    UPROPERTY(EditAnywhere, Config)
    float PrecisionScale = 0.25F;
    UPROPERTY(EditAnywhere, Config)
    float CameraSensitivity = 1.0F;
    UPROPERTY(EditAnywhere, Config)
    float ResetHoldSeconds = 1.5F;
    UPROPERTY(EditAnywhere, Config)
    float HoldSeconds = 1.5F;
    UPROPERTY(EditAnywhere, Config)
    ETriggerRole LeftTriggerRole = ETriggerRole::Unused;
    // Retain the original property name for saved profile compatibility (right trigger).
    UPROPERTY(EditAnywhere, Config)
    ETriggerRole TriggerRole = ETriggerRole::Precision;
    UPROPERTY(EditAnywhere, Config)
    EDroneInputProfile SavedProfile = EDroneInputProfile::ProfessionalMode2;
    UPROPERTY(EditAnywhere, Config)
    EDroneFlightMode SavedMode = EDroneFlightMode::LegacyAssisted;

  private:
    friend class FProfessionalControlsTest;
    friend class FControlSafetyTest;
    AQuadrotorPawn *Drone() const;
    static FString GetActionName(EPilotAction Action);
    FString ProfileDirectory;
    FString SlotPath(int Slot) const;
    void Dispatch(EPilotAction Action);
    void MenuKey(FKey Key);
    void Adjust(int32 Direction);
    void AdjustGeneralSettings(int32 AdjustmentDirection, AQuadrotorPawn *DronePawn);
    void AdjustAxisCalibration(int32 AdjustmentDirection, AQuadrotorPawn *DronePawn);
    void AdjustFlightModeSettings(int32 AdjustmentDirection, AQuadrotorPawn *DronePawn);
    void AdjustButtonSettings(int32 AdjustmentDirection, AQuadrotorPawn *DronePawn);

    bool bMenuOpen = false;
    bool bTelemetry = true;
    bool bFlightModifier = false;
    bool bCameraModifier = false;
    bool bCapturing = false;
    int32 Page = 0;
    int32 Row = 0;
    int32 AxisIndex = 0;
    int32 ButtonIndex = 0;
    int32 SlotIndex = 1;
    TSet<FKey> HeldKeys;
    FKey HeldReset;
    FKey HeldArm;
    float ArmSeconds = 0.0F;
};
