#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "Flight/PilotInput.h"
#include "PilotControlsComponent.generated.h"

UENUM()
enum class ECollectiveMode : uint8 { Incremental, Direct };
UENUM()
enum class ETriggerRole : uint8 { Unused, Precision, Zoom, Gimbal, Auxiliary };
UENUM()
enum class EPilotAction : uint8
{
    Camera, Reset, Settings, Telemetry, Profile, NextMode, PreviousMode,
    IncreaseLimit, DecreaseLimit, Brake, Recenter, GimbalUp, GimbalDown,
    ZoomIn, ZoomOut, Takeoff, Land, Arm, Confirm, Cancel, Count
};
UENUM()
enum class EMotorState : uint8 { Disarmed, Armed, Failsafe };

USTRUCT()
struct FPrecisionAxis
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) float Center = 0.0F;
    UPROPERTY(EditAnywhere) float Minimum = -1.0F;
    UPROPERTY(EditAnywhere) float Maximum = 1.0F;
    UPROPERTY(EditAnywhere) float Inner = 0.05F;
    UPROPERTY(EditAnywhere) float Outer = 0.02F;
    UPROPERTY(EditAnywhere) float Sensitivity = 1.0F;
    UPROPERTY(EditAnywhere) float ExpoOffset = 0.0F;
    UPROPERTY(EditAnywhere) bool bInvert = false;
    UPROPERTY(EditAnywhere) int32 Source = 0;
    bool IsValid() const;
    float Process(float Raw, float Expo) const;
};

USTRUCT()
struct FPilotModeTuning
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) float Speed = 10.0F;
    UPROPERTY(EditAnywhere) float Tilt = 38.0F;
    UPROPERTY(EditAnywhere) float BrakeTilt = 44.0F;
    UPROPERTY(EditAnywhere) float Acceleration = 30.0F;
    UPROPERTY(EditAnywhere) float Braking = 30.0F;
    UPROPERTY(EditAnywhere) float Jerk = 100.0F;
    UPROPERTY(EditAnywhere) float BrakeJerk = 160.0F;
    UPROPERTY(EditAnywhere) float VelocityGain = 5.0F;
    UPROPERTY(EditAnywhere) float BrakeGain = 8.0F;
    UPROPERTY(EditAnywhere) float Prediction = 0.2F;
    UPROPERTY(EditAnywhere) float AttitudeGain = 9.0F;
    UPROPERTY(EditAnywhere) float RateDegrees = 172.0F;
    UPROPERTY(EditAnywhere) float YawDegrees = 86.0F;
    UPROPERTY(EditAnywhere) float Ascent = 1.5F;
    UPROPERTY(EditAnywhere) float Descent = 1.5F;
    UPROPERTY(EditAnywhere) float VerticalAcceleration = 4.0F;
    UPROPERTY(EditAnywhere) float Expo = 0.35F;
    UPROPERTY(EditAnywhere) float YawExpo = 0.35F;
    UPROPERTY(EditAnywhere) float Slew = 24.0F;
    UPROPERTY(EditAnywhere) float HoldGain = 0.8F;
    bool IsValid() const;
};

USTRUCT()
struct FPilotButton
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FKey Key;
    // Exact mask: 0 unmodified, 1 LB/L1, 2 RB/R1. Both modifiers suppress commands.
    UPROPERTY(EditAnywhere) int32 Modifiers = 0;
    UPROPERTY(EditAnywhere) EPilotAction Action = EPilotAction::Camera;
    FPilotButton() = default;
    FPilotButton(FKey InKey, int32 InModifiers, EPilotAction InAction)
        : Key(InKey), Modifiers(InModifiers), Action(InAction) {}
};

class AQuadrotorPawn;
class UInputComponent;

UCLASS(ClassGroup=(Drone), Config=Game)
class AERIALWORLDLAB_API UPilotControlsComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPilotControlsComponent();
    void Bind(UInputComponent* Input);
    void Press(FKey Key);
    void Release(FKey Key);
    void CancelHeld();
    void ToggleMenu();
    void RestoreDefaults();
    bool Validate(FString& Error) const;
    bool SaveSlot(int32 Slot);
    bool LoadSlot(int32 Slot);
    FString DescribeMenu() const;
    FString Diagnostics() const;
    void RefreshPausedInput();
    static FString ButtonLabel(FKey Key);
    FString BindingLabel(FKey Key) const;
    FString ActionBindings(EPilotAction Action) const;
    bool IsKeyHeld(FKey Key) const { return HeldKeys.Contains(Key); }
    bool IsMapPage() const { return Page==4; }
    void UpdateAuxiliary(bool LeftAvailable, bool RightAvailable);
    float AuxiliaryChannels[2]{};
    const FPilotModeTuning& Tuning(EDroneFlightMode Mode) const;
    float Shape(int32 Axis, float Raw, EDroneFlightMode Mode) const;
    int32 FindBinding(FKey Key, int32 Modifiers) const;
    bool IsMenuOpen() const { return bMenuOpen; }
    bool IsTelemetryVisible() const { return bTelemetry; }
    int32 ModifierMask() const { return (bFlightModifier ? 1 : 0) | (bCameraModifier ? 2 : 0); }
    void StepHolds(float Dt);
    float Raw[6]{};
    float Processed[4]{};
    float Throttle = 0.0F;
    FString Status;
    bool bPrecision = false;
    UPROPERTY(EditAnywhere, Config) TArray<FPrecisionAxis> Axes;
    UPROPERTY(EditAnywhere, Config) TArray<FPilotModeTuning> Modes;
    UPROPERTY(EditAnywhere, Config) TArray<FPilotButton> Buttons;
    UPROPERTY(EditAnywhere, Config) ECollectiveMode CollectiveMode = ECollectiveMode::Incremental;
    UPROPERTY(EditAnywhere, Config) float ThrottleIncrease = 0.35F;
    UPROPERTY(EditAnywhere, Config) float ThrottleDecrease = 0.5F;
    UPROPERTY(EditAnywhere, Config) float PrecisionScale = 0.25F;
    UPROPERTY(EditAnywhere, Config) float CameraSensitivity = 1.0F;
    UPROPERTY(EditAnywhere, Config) float ResetHoldSeconds = 1.5F;
    UPROPERTY(EditAnywhere, Config) float HoldSeconds = 1.5F;
    UPROPERTY(EditAnywhere, Config) ETriggerRole LeftTriggerRole = ETriggerRole::Unused;
    // Retain the original property name for saved profile compatibility (right trigger).
    UPROPERTY(EditAnywhere, Config) ETriggerRole TriggerRole = ETriggerRole::Precision;
    UPROPERTY(EditAnywhere, Config) EDroneInputProfile SavedProfile = EDroneInputProfile::ProfessionalMode2;
    UPROPERTY(EditAnywhere, Config) EDroneFlightMode SavedMode = EDroneFlightMode::LegacyAssisted;
private:
    friend class FProfessionalControlsTest;
    friend class FControlSafetyTest;
    AQuadrotorPawn* Drone() const;
    FString ProfileDirectory;
    FString SlotPath(int Slot) const;
    void Dispatch(EPilotAction Action);
    void MenuKey(FKey Key);
    void Adjust(int32 Direction);
    bool bMenuOpen = false, bTelemetry = true;
    bool bFlightModifier = false, bCameraModifier = false;
    bool bCapturing = false;
    int32 Page = 0, Row = 0, AxisIndex = 0, ButtonIndex = 0, SlotIndex = 1;
    TSet<FKey> HeldKeys;
    FKey HeldReset, HeldArm;
    float ArmSeconds = 0.0F;
};
