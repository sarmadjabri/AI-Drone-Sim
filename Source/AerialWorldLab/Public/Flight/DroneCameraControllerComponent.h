#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneCameraControllerComponent.generated.h"

class UCameraComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class EDroneViewMode : uint8
{
    ThirdPerson UMETA(DisplayName = "3D View"),
    StabilizedFirstPerson UMETA(DisplayName = "Stabilized FPV"),
    BodyLockedFirstPerson UMETA(DisplayName = "Body-Locked FPV")
};

UCLASS(ClassGroup = (Drone), meta = (BlueprintSpawnableComponent), Config = Game)
class AERIALWORLDLAB_API UDroneCameraControllerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDroneCameraControllerComponent();

    void Configure(USceneComponent* DroneBody, UCameraComponent* FirstPersonCamera);
    void SetViewMode(EDroneViewMode NewMode);
    void ResetCameraPose();

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    friend class FFlightViewModeTest;

    void UpdateCameraPose(float DeltaTime);
    FQuat GetStabilizedTarget(const FQuat& BodyRotation);
    FQuat LimitGimbalTravel(const FQuat& BodyRotation, const FQuat& DesiredWorldRotation) const;
    void UpdateFirstPersonVisibility();

    UPROPERTY(Transient)
    TObjectPtr<USceneComponent> DroneBody;

    UPROPERTY(Transient)
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Camera")
    FVector MountOffsetCentimeters = FVector(12.0F, 0.0F, 8.0F);

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Camera")
    FRotator MountRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Camera", meta = (ClampMin = "-90.0", ClampMax = "90.0"))
    float StabilizedPitchDegrees = 0.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Camera", meta = (ClampMin = "60.0", ClampMax = "120.0"))
    float StabilizedFieldOfView = 95.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Camera", meta = (ClampMin = "60.0", ClampMax = "120.0"))
    float BodyLockedFieldOfView = 110.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Camera", meta = (ClampMin = "-45.0", ClampMax = "45.0"))
    float BodyLockedUptiltDegrees = 10.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Camera|Gimbal", meta = (ClampMin = "-180.0", ClampMax = "0.0"))
    float MinimumGimbalPitchDegrees = -90.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Camera|Gimbal", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float MaximumGimbalPitchDegrees = 30.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Camera|Gimbal", meta = (ClampMin = "0.0", ClampMax = "90.0"))
    float MaximumGimbalRollDegrees = 45.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Camera|Gimbal", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float MaximumGimbalYawDegrees = 180.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Camera|Gimbal", meta = (ClampMin = "1.0", ClampMax = "720.0"))
    float MaximumGimbalAngularVelocityDegreesPerSecond = 180.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Camera|Gimbal", meta = (ClampMin = "1.0", ClampMax = "3600.0"))
    float MaximumGimbalAngularAccelerationDegreesPerSecondSquared = 720.0F;

    UPROPERTY(EditAnywhere, Config, Category = "Drone|Camera|Gimbal", meta = (ClampMin = "0.1", ClampMax = "30.0"))
    float GimbalResponseSpeed = 8.0F;

    UPROPERTY(Transient)
    EDroneViewMode ViewMode = EDroneViewMode::ThirdPerson;

    FQuat StabilizedCameraRotation = FQuat::Identity;
    FVector GimbalAngularVelocityRadiansPerSecond = FVector::ZeroVector;
    float LastStableHeadingDegrees = 0.0F;
    bool bHasCameraPose = false;
};
