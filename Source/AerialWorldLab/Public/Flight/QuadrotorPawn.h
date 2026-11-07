#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Flight/QuadrotorDynamics.h"
#include "QuadrotorPawn.generated.h"

class UCameraComponent;
class UMaterialInterface;
class USpringArmComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UBoxComponent;

UCLASS()
class AERIALWORLDLAB_API AQuadrotorPawn : public APawn
{
    GENERATED_BODY()

public:
    AQuadrotorPawn();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    FVector GetVelocityMetersPerSecond() const;
    float GetMotorThrustNewtons(int32 MotorIndex) const;
    float GetCollectiveThrustNewtons() const;
    float GetMaximumCollectiveThrustNewtons() const;
    FString GetFlightModeName() const;
    int32 GetFlightModeIndex() const;

protected:
    virtual void BeginPlay() override;

private:
    void SetRollInput(float Value);
    void SetPitchInput(float Value);
    void SetYawInput(float Value);
    void SetThrottleInput(float Value);
    void SetCameraZoomInput(float Value);
    void CycleFlightMode();
    void ResetDrone();
    void ApplyTint(UPrimitiveComponent* Component, const FLinearColor& Color) const;
    float ApplyDeadzone(float Value) const;

    UPROPERTY(VisibleAnywhere, Category = "Drone")
    TObjectPtr<UBoxComponent> Body;

    UPROPERTY(VisibleAnywhere, Category = "Drone")
    TObjectPtr<UStaticMeshComponent> Fuselage;

    UPROPERTY(VisibleAnywhere, Category = "Drone")
    TArray<TObjectPtr<UStaticMeshComponent>> Rotors;

    UPROPERTY(VisibleAnywhere, Category = "Drone")
    TArray<TObjectPtr<UStaticMeshComponent>> PropellerBlades;

    UPROPERTY(VisibleAnywhere, Category = "Drone")
    TObjectPtr<UStaticMeshComponent> NoseCamera;

    UPROPERTY(VisibleAnywhere, Category = "Drone")
    TObjectPtr<UStaticMeshComponent> CameraLens;

    UPROPERTY(VisibleAnywhere, Category = "Drone")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, Category = "Drone")
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> DroneMaterial;

    UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "0.1"))
    float MassKg = 1.0F;

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

    UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "1.0"))
    float MaximumMotorThrustNewtons = 12.0F;

    UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "0.001"))
    float MotorTimeConstantSeconds = 0.035F;

    UPROPERTY(EditAnywhere, Category = "Drone|Physics", meta = (ClampMin = "0.001"))
    float YawMomentPerNewtonNewtonMeters = 0.018F;

    UPROPERTY(EditAnywhere, Category = "Drone|Control", meta = (ClampMin = "1.0", ClampMax = "45.0"))
    float MaximumTiltDegrees = 25.0F;

    UPROPERTY(EditAnywhere, Category = "Drone|Control", meta = (ClampMin = "0.0"))
    float AngleGainNewtonMetersPerRadian = 0.8F;

    UPROPERTY(EditAnywhere, Category = "Drone|Control", meta = (ClampMin = "0.0"))
    float RateDampingNewtonMeterSeconds = 0.22F;

    UPROPERTY(EditAnywhere, Category = "Drone|Control", meta = (ClampMin = "0.0"))
    float MaximumYawRateRadiansPerSecond = 1.8F;

    UPROPERTY(EditAnywhere, Category = "Drone|Control", meta = (ClampMin = "0.0"))
    float YawRateGainNewtonMeterSeconds = 0.12F;

    UPROPERTY(EditAnywhere, Category = "Drone|Control", meta = (ClampMin = "0.0"))
    float ThrottleRampNewtonsPerSecond = 18.0F;

    UPROPERTY(EditAnywhere, Category = "Drone|Control", meta = (ClampMin = "0.0", ClampMax = "0.5"))
    float StickDeadzone = 0.08F;

    UPROPERTY(EditAnywhere, Category = "Drone|Control", meta = (ClampMin = "0.1"))
    float StickResponseSpeed = 12.0F;

    UPROPERTY(EditAnywhere, Category = "Drone|Control", meta = (ClampMin = "0.0"))
    float CameraZoomSpeed = 180.0F;

    UPROPERTY(EditAnywhere, Category = "Drone|Visuals", meta = (ClampMin = "60.0"))
    float MaximumVisualRotorRpm = 240.0F;

    float RollInput = 0.0F;
    float PitchInput = 0.0F;
    float YawInput = 0.0F;
    float RollInputTarget = 0.0F;
    float PitchInputTarget = 0.0F;
    float YawInputTarget = 0.0F;
    float ThrottleInput = 0.0F;
    float ThrottleInputTarget = 0.0F;
    float CameraZoomInput = 0.0F;
    int32 FlightModeIndex = 1;
    FTransform InitialDroneTransform;
    float CollectiveCommandNewtons = 0.0F;
    Aether::MotorThrusts MotorThrustsNewtons{ 0.0F, 0.0F, 0.0F, 0.0F };
};
