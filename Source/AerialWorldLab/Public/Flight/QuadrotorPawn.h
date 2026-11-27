// Connects pilot intent, flight control and SI forces to the Chaos rigid body.
#pragma once

#include "CoreMinimal.h"
#include "Flight/DroneCameraControllerComponent.h"
#include "Flight/PilotControlsComponent.h"
#include "Flight/PilotInput.h"
#include "Flight/QuadrotorDynamics.h"
#include "GameFramework/Pawn.h"
#include "QuadrotorPawn.generated.h"

struct FQuadrotorControlFrame;

class UCameraComponent;
class UMaterialInterface;
class UDroneCameraControllerComponent;
class USpringArmComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UBoxComponent;

UCLASS(Config = Game)

class AERIALWORLDLAB_API AQuadrotorPawn : public APawn
{
    GENERATED_BODY()

  public:
    AQuadrotorPawn();

    virtual void OnConstruction(const FTransform &Transform) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent *PlayerInputComponent) override;

    bool IsManualMode() const
    {
        return FlightMode == EDroneFlightMode::Angle || FlightMode == EDroneFlightMode::Acro;
    }

    FString GetControlStatus() const;

    UPilotControlsComponent *GetPilotControls() const
    {
        return PilotControls;
    }

    FString GetInputProfileName() const;
    FString GetFlightModeName() const;
    bool UsesMode2Axes() const;

    bool IsControllerDisconnected() const
    {
        return bControllerDisconnected;
    }

    float GetResetProgress() const
    {
        if (!bResetHeld)
        {
            return 0.0F;
        }
        return FMath::Clamp(ResetHeldSeconds / FMath::Max(ResetHoldSeconds, 0.25F), 0.0F, 1.0F);
    }

    FVector GetVelocityMetersPerSecond() const;
    float GetMotorThrustNewtons(int32 MotorIndex) const;
    float GetCollectiveThrustNewtons() const;
    float GetMaximumCollectiveThrustNewtons() const;

    FVector2D GetMovementInput() const
    {
        return FVector2D(RawForwardInput, RawLateralInput);
    }

    bool IsPositionHolding() const
    {
        return bHoldingPosition;
    }

    bool IsFirstPerson() const
    {
        return ViewMode != EDroneViewMode::ThirdPerson;
    }

    bool IsWaitingForNeutralInput() const
    {
        return bAwaitingNeutralInput;
    }

    FString GetViewModeName() const
    {
        switch (ViewMode)
        {
        case EDroneViewMode::StabilizedFirstPerson:
            return TEXT("Stabilized FPV");
        case EDroneViewMode::BodyLockedFirstPerson:
            return TEXT("Body-Locked FPV");
        default:
            return TEXT("3D View");
        }
    }

  protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

  private:
    // Ordered simulation stages. Configuration remains in the existing reflected properties.
    void UpdatePilotCommands(float DeltaSeconds);
    FQuadrotorControlFrame ResolveControlFrame(const FPilotModeTuning &ModeTuning) const;
    float SmoothPilotAxis(float Current, float Target, float DeltaSeconds,
                          const FQuadrotorControlFrame &Frame,
                          const FPilotModeTuning &ModeTuning) const;
    void UpdateSmoothedPilotCommands(float DeltaSeconds, const FQuadrotorControlFrame &Frame,
                                     const FPilotModeTuning &ModeTuning);
    void UpdateChaseCameraZoom(float DeltaSeconds);
    FVector ApplyAerodynamicDrag(const FQuat &BodyRotation);
    FVector CalculateAssistedVelocity(float DeltaSeconds, const FQuadrotorControlFrame &Frame,
                                      FVector &HorizontalVelocity);
    FVector CalculateAssistedBodyRates(float DeltaSeconds, const FQuadrotorControlFrame &Frame,
                                       const FPilotModeTuning &ModeTuning,
                                       const FVector &DesiredVelocity,
                                       const FVector &HorizontalVelocity);
    void UpdateAssistedAltitude(float DeltaSeconds, const FQuadrotorControlFrame &Frame,
                                const FPilotModeTuning &ModeTuning, const FVector &DesiredVelocity);
    FVector UpdateManualFlightCommands(float DeltaSeconds, const FQuadrotorControlFrame &Frame,
                                       const FPilotModeTuning &ModeTuning);
    void ApplyMotorCommands(float DeltaSeconds, const FQuadrotorControlFrame &Frame,
                            FVector DesiredBodyRate);

    friend class UPilotControlsComponent;
    friend class FProfessionalControlsTest;
    friend class FControlSafetyTest;
    friend class FManualFlightTest;
    friend class FHighPerformanceTest;
    bool RequestFlightMode(EDroneFlightMode NewMode);
    bool IsGrounded() const;
    void ToggleArm();
    void RequestTakeoff();
    void RequestLanding();
    void AdjustPilotCameraZoom(float Amount);
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UPilotControlsComponent> PilotControls;
    EMotorState MotorState = EMotorState::Armed;
    bool bLanding = false;
    bool bTakeoffRequired = false;
    FVector CommandedBodyRate = FVector::ZeroVector;
    FVector CommandedVelocity = FVector::ZeroVector;
    bool bMotorSaturated = false;
    float PadLeftTrigger = 0;
    float PadRightTrigger = 0;

    void SetLeftTrigger(float V)
    {
        PadLeftTrigger = V;
    }

    void SetRightTrigger(float V)
    {
        PadRightTrigger = V;
    }
    friend class FFlightPilotProfilesTest;
    friend class FFlightAssistedModesTest;
    friend class FFlightChaosStabilityTest;
    void HandleControllerConnection(bool bConnected);
    FDelegateHandle ConnectionChangedHandle;
    bool bControllerDisconnected = false;
    void CycleInputProfile();
    void CycleFlightMode();
    void RequireNeutralInput();

    void BeginResetHold()
    {
        bResetHeld = true;
        ResetHeldSeconds = 0.0F;
    }

    void EndResetHold()
    {
        bResetHeld = false;
        ResetHeldSeconds = 0.0F;
    }

    void UpdateResetHold(float DeltaSeconds);

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Input")

    EDroneInputProfile InputProfile = EDroneInputProfile::ProfessionalMode2;
    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control")
    EDroneFlightMode FlightMode = EDroneFlightMode::LegacyAssisted;
    UPROPERTY(EditAnywhere, Config, Category = "Drone|Input")
    bool bInvertForward = false;
    UPROPERTY(EditAnywhere, Config, Category = "Drone|Input")
    bool bInvertLateral = false;
    UPROPERTY(EditAnywhere, Config, Category = "Drone|Input")
    bool bInvertYaw = false;
    UPROPERTY(EditAnywhere, Config, Category = "Drone|Input")
    bool bInvertVertical = false;
    UPROPERTY(EditAnywhere, Config, Category = "Drone|Input", meta = (ClampMin = "0.25"))
    float ResetHoldSeconds = 1.5F;
    bool bResetHeld = false;
    float ResetHeldSeconds = 0.0F;
    friend class FFlightViewModeTest;
    friend class FFlightImportedAssetsTest;
    void ToggleViewMode();
    void ApplyViewMode();
    void UpdatePilotInput();

    void SetPadLeftX(float Value)
    {
        PadLeftX = Value;
    }

    void SetPadLeftY(float Value)
    {
        PadLeftY = Value;
    }

    void SetPadRightX(float Value)
    {
        PadRightX = Value;
    }

    void SetPadRightY(float Value)
    {
        PadRightY = Value;
    }

    void SetPadTriggers(float Value)
    {
        PadTriggers = Value;
    }

    void SetKeyboardForward(float Value)
    {
        KeyboardForward = Value;
    }

    void SetKeyboardLateral(float Value)
    {
        KeyboardLateral = Value;
    }

    void SetKeyboardYaw(float Value)
    {
        KeyboardYaw = Value;
    }

    void SetKeyboardThrottle(float Value)
    {
        KeyboardThrottle = Value;
    }

    void SetFirstPersonKeyboardThrottle(float Value)
    {
        FirstPersonKeyboardThrottle = Value;
    }

    void SetLateralInput(float Value);
    void SetForwardInput(float Value);
    void SetYawInput(float Value);
    void SetThrottleInput(float Value);
    void SetCameraZoomInput(float Value);
    void ResetDrone();
    void ApplyTint(UPrimitiveComponent *Component, const FLinearColor &Color) const;
    float ApplyDeadzone(float Value) const;

    UPROPERTY(VisibleAnywhere, Category = "Drone")

    TObjectPtr<UBoxComponent> Body;

    UPROPERTY(VisibleAnywhere, Category = "Drone")

    TObjectPtr<UStaticMeshComponent> Fuselage;

    UPROPERTY(VisibleAnywhere, Category = "Drone")

    TArray<TObjectPtr<UStaticMeshComponent>> Rotors;

    UPROPERTY(VisibleAnywhere, Category = "Drone")

    TArray<TObjectPtr<UStaticMeshComponent>> PropellerBlades;

    UPROPERTY(VisibleAnywhere, Category = "Drone|Visuals")

    TArray<TObjectPtr<UStaticMeshComponent>> ImportedDroneParts;

    UPROPERTY(VisibleAnywhere, Category = "Drone")

    TObjectPtr<UStaticMeshComponent> NoseCamera;

    UPROPERTY(VisibleAnywhere, Category = "Drone")

    TObjectPtr<UStaticMeshComponent> CameraLens;

    UPROPERTY(VisibleAnywhere, Category = "Drone")

    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, Category = "Drone")

    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(VisibleAnywhere, Category = "Drone|Camera")

    TObjectPtr<UCameraComponent> FirstPersonCamera;

    UPROPERTY(VisibleAnywhere, Category = "Drone|Camera")

    TObjectPtr<UDroneCameraControllerComponent> FirstPersonCameraController;

    UPROPERTY(VisibleAnywhere, Category = "Drone|Camera")

    EDroneViewMode ViewMode = EDroneViewMode::ThirdPerson;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Camera",
              meta = (ClampMin = "60.0", ClampMax = "120.0"))
    float ThirdPersonFieldOfView = 75.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Camera", meta = (ClampMin = "0.1"))

    float ThirdPersonCameraPositionLagSpeed = 24.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Camera", meta = (ClampMin = "0.1"))

    float ThirdPersonCameraRotationLagSpeed = 20.0F;

    bool bUseMappedInput = false;
    bool bAwaitingNeutralInput = false;
    float PadLeftX = 0.0F;
    float PadLeftY = 0.0F;
    float PadRightX = 0.0F;
    float PadRightY = 0.0F;
    float PadTriggers = 0.0F;
    float KeyboardForward = 0.0F;
    float KeyboardLateral = 0.0F;
    float KeyboardYaw = 0.0F;
    float KeyboardThrottle = 0.0F;
    float FirstPersonKeyboardThrottle = 0.0F;

    UPROPERTY(Transient)

    TObjectPtr<UMaterialInterface> DroneMaterial;

    UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "0.1"))

    float MassKg = 1.0F;

    // World axes, meters/second. A positive X wind travels toward positive X.
    UPROPERTY(EditAnywhere, Config, Category = "Drone|Aerodynamics")
    FVector WindVelocityMetersPerSecond = FVector::ZeroVector;

    // Effective Cd * frontal area along each body axis. These need flight calibration.
    UPROPERTY(EditAnywhere, Config, Category = "Drone|Aerodynamics")
    FVector DragAreaSquareMeters = FVector(0.015, 0.015, 0.03);

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Aerodynamics", meta = (ClampMin = "0.0"))

    float AirDensityKgPerCubicMeter = 1.225F;

    UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "1.0"))

    float GravityMetersPerSecondSquared = 9.81F;

    UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "0.05"))

    float ArmLengthMeters = 0.23F;

    UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "0.001"))

    float InertiaXKgMetersSquared = 0.02F;

    UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "0.001"))

    float InertiaYKgMetersSquared = 0.02F;

    UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "0.001"))

    float InertiaZKgMetersSquared = 0.04F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Physics", meta = (ClampMin = "1.0"))

    float MaximumMotorThrustNewtons = 12.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Physics", meta = (ClampMin = "0.001"))

    float MotorTimeConstantSeconds = 0.01F;

    UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "0.001"))

    float YawMomentPerNewtonNewtonMeters = 0.018F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control",
              meta = (ClampMin = "1.0", ClampMax = "45.0"))
    float MaximumTiltDegrees = 45.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control",
              meta = (ClampMin = "1.0", ClampMax = "45.0"))
    float MaximumBrakingTiltDegrees = 44.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.0"))

    float RollAttitudeGainPerSecond = 9.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.1"))

    float MaximumRollRateRadiansPerSecond = 3.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.0"))

    float RollRateGainNewtonMeterSeconds = 0.22F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.0"))

    float MaximumYawRateRadiansPerSecond = 1.5F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.0"))

    float YawRateGainNewtonMeterSeconds = 0.12F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.0"))

    float YawHeadingGainPerSecond = 2.5F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.1"))

    float MaximumHorizontalSpeedMetersPerSecond = 20.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.1"))

    float HorizontalVelocityGainPerSecond = 5.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.1"))

    float BrakingVelocityGainPerSecond = 8.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.1"))

    float AccelerationJerkMetersPerSecondCubed = 100.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.1"))

    float BrakingJerkMetersPerSecondCubed = 160.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.1"))

    float VerticalSpeedMetersPerSecond = 4.5F;

    UPROPERTY(EditAnywhere, Category = "Drone|Control", meta = (ClampMin = "0.1"))

    float AltitudePositionGainPerSecondSquared = 2.5F;

    UPROPERTY(EditAnywhere, Category = "Drone|Control", meta = (ClampMin = "0.1"))

    float AltitudeVelocityGainPerSecond = 3.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control",
              meta = (ClampMin = "0.0", ClampMax = "0.5"))
    float StickDeadzone = 0.08F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.1"))

    float StickResponseSpeed = 24.0F;

    UPROPERTY(EditAnywhere, Category = "Drone|Control", meta = (ClampMin = "0.0"))

    float CameraZoomSpeed = 180.0F;

    UPROPERTY(EditAnywhere, Category = "Drone|Visuals", meta = (ClampMin = "60.0"))

    float MaximumVisualRotorRpm = 240.0F;

    // Flight tuning lives here; FlightControl.h contains stateless math only.
    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.0"))
    float PitchAttitudeGainPerSecond = 9.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.0"))

    float MaximumPitchRateRadiansPerSecond = 3.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.0"))

    float PitchRateGainNewtonMeterSeconds = 0.22F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.0"))

    float VelocityPredictionSeconds = 0.2F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.0"))

    float StickReleaseMultiplier = 3.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.0"))

    float StickExpo = 0.35F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.0"))

    float MaximumVerticalAcceleration = 4.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.0"))

    float MinimumAltitudeMeters = 0.35F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.0"))

    float MaximumAltitudeMeters = 50.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Control", meta = (ClampMin = "0.1"))

    float CollectiveThrustRampNewtonsPerSecond = 180.0F;

    UPROPERTY(EditAnywhere, Category = "Drone|Stabilizer", meta = (ClampMin = "0.0"))

    float PositionHoldGainPerSecond = 0.8F;

    UPROPERTY(EditAnywhere, Category = "Drone|Stabilizer", meta = (ClampMin = "0.0"))

    float PositionHoldMaximumSpeed = 0.5F;

    UPROPERTY(EditAnywhere, Category = "Drone|Stabilizer", meta = (ClampMin = "0.0"))

    float PositionHoldCaptureSpeed = 0.15F;

    UPROPERTY(EditAnywhere, Category = "Drone|Stabilizer", meta = (ClampMin = "0.0"))

    float PositionHoldCaptureTiltDegrees = 5.0F;

    // Center must persist before position hold may engage; braking still starts immediately.
    UPROPERTY(EditAnywhere, Category = "Drone|Stabilizer", meta = (ClampMin = "0.0"))
    float PositionHoldNeutralDelaySeconds = 0.15F;

    float RawForwardInput = 0.0F;
    float RawLateralInput = 0.0F;
    float TranslationNeutralSeconds = 0.0F;
    bool bHoldingPosition = false;
    FVector HoldPositionCentimeters = FVector::ZeroVector;
    FVector HorizontalAccelerationCommand = FVector::ZeroVector;
    float LateralInput = 0.0F;
    float ForwardInput = 0.0F;
    float YawInput = 0.0F;
    float LateralInputTarget = 0.0F;
    float ForwardInputTarget = 0.0F;
    float YawInputTarget = 0.0F;
    float ThrottleInput = 0.0F;
    float ThrottleInputTarget = 0.0F;
    float CameraZoomInput = 0.0F;
    FTransform InitialDroneTransform;
    float AltitudeTargetMeters = 0.0F;
    float YawTargetDegrees = 0.0F;
    float CollectiveCommandNewtons = 0.0F;
    Aether::MotorThrusts MotorThrustsNewtons{0.0F, 0.0F, 0.0F, 0.0F};
};
