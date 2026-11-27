// Keeps body-locked FPV separate from stabilized/gimbal and chase-camera behavior.
#include "Flight/DroneCameraControllerComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"

namespace
{
    constexpr float CameraIntegrationStepSeconds = 1.0F / 120.0F;
    constexpr float MinimumHeadingProjectionSquared = 1.0e-6F;
} // namespace

UDroneCameraControllerComponent::UDroneCameraControllerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UDroneCameraControllerComponent::Configure(USceneComponent *InDroneBody,
                                                UCameraComponent *InFirstPersonCamera)
{
    DroneBody = InDroneBody;
    FirstPersonCamera = InFirstPersonCamera;
    if (FirstPersonCamera)
    {
        FirstPersonCamera->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);
    }
    ResetCameraPose();
}

void UDroneCameraControllerComponent::SetViewMode(EDroneViewMode NewMode)
{
    if (ViewMode == NewMode)
    {
        UpdateFirstPersonVisibility();
        return;
    }

    ViewMode = NewMode;
    bHasCameraPose = false;
    GimbalAngularVelocityRadiansPerSecond = FVector::ZeroVector;

    if (FirstPersonCamera && ViewMode != EDroneViewMode::ThirdPerson)
    {
        float FieldOfView = BodyLockedFieldOfView;
        if (ViewMode == EDroneViewMode::StabilizedFirstPerson)
        {
            FieldOfView = StabilizedFieldOfView;
        }
        FirstPersonCamera->SetFieldOfView(FieldOfView);
    }
    UpdateFirstPersonVisibility();
}

void UDroneCameraControllerComponent::ResetCameraPose()
{
    bHasCameraPose = false;
    GimbalAngularVelocityRadiansPerSecond = FVector::ZeroVector;
    if (DroneBody)
    {
        LastStableHeadingDegrees = DroneBody->GetComponentRotation().Yaw;
    }
}

void UDroneCameraControllerComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                    FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (ViewMode == EDroneViewMode::ThirdPerson)
    {
        return;
    }

    UpdateFirstPersonVisibility();
    UpdateCameraPose(DeltaTime);
}

void UDroneCameraControllerComponent::UpdateCameraPose(float DeltaTime)
{
    if (!DroneBody || !FirstPersonCamera || ViewMode == EDroneViewMode::ThirdPerson ||
        !FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0F)
    {
        return;
    }

    const FTransform BodyTransform = DroneBody->GetComponentTransform();
    const FQuat BodyRotation = BodyTransform.GetRotation().GetNormalized();
    const FVector CameraPosition = BodyTransform.TransformPosition(MountOffsetCentimeters);

    if (ViewMode == EDroneViewMode::BodyLockedFirstPerson)
    {
        const FQuat MountOrientation =
            MountRotation.Quaternion() * FRotator(BodyLockedUptiltDegrees, 0.0F, 0.0F).Quaternion();
        StabilizedCameraRotation = (BodyRotation * MountOrientation).GetNormalized();
        GimbalAngularVelocityRadiansPerSecond = FVector::ZeroVector;
        bHasCameraPose = true;
    }
    else
    {
        const FQuat TargetRotation = GetStabilizedTarget(BodyRotation);
        if (!bHasCameraPose)
        {
            StabilizedCameraRotation = TargetRotation;
            GimbalAngularVelocityRadiansPerSecond = FVector::ZeroVector;
            bHasCameraPose = true;
        }
        else
        {
            float RemainingTime = FMath::Min(DeltaTime, 0.25F);
            while (RemainingTime > 0.0F)
            {
                const float StepTime = FMath::Min(RemainingTime, CameraIntegrationStepSeconds);
                const FQuat RotationError =
                    (TargetRotation * StabilizedCameraRotation.Inverse()).GetNormalized();
                FVector ErrorAxis;
                float ErrorAngleRadians = 0.0F;
                RotationError.ToAxisAndAngle(ErrorAxis, ErrorAngleRadians);
                if (RotationError.W < 0.0F)
                {
                    ErrorAngleRadians = 2.0F * PI - ErrorAngleRadians;
                    ErrorAxis *= -1.0F;
                }

                if (ErrorAngleRadians <= UE_SMALL_NUMBER)
                {
                    GimbalAngularVelocityRadiansPerSecond = FVector::ZeroVector;
                    StabilizedCameraRotation = TargetRotation;
                    break;
                }

                const float MaximumSpeed =
                    FMath::DegreesToRadians(MaximumGimbalAngularVelocityDegreesPerSecond);
                const float MaximumAcceleration = FMath::DegreesToRadians(
                    MaximumGimbalAngularAccelerationDegreesPerSecondSquared);
                const float BrakingSpeed =
                    FMath::Sqrt(2.0F * MaximumAcceleration * ErrorAngleRadians);
                const float DesiredSpeed = FMath::Min3(
                    MaximumSpeed, ErrorAngleRadians * GimbalResponseSpeed, BrakingSpeed);
                const FVector DesiredAngularVelocity = ErrorAxis * DesiredSpeed;
                GimbalAngularVelocityRadiansPerSecond +=
                    (DesiredAngularVelocity - GimbalAngularVelocityRadiansPerSecond)
                        .GetClampedToMaxSize(MaximumAcceleration * StepTime);

                FVector RotationStep = GimbalAngularVelocityRadiansPerSecond * StepTime;
                if (FVector::DotProduct(RotationStep, ErrorAxis) >= ErrorAngleRadians)
                {
                    RotationStep = ErrorAxis * ErrorAngleRadians;
                    GimbalAngularVelocityRadiansPerSecond = FVector::ZeroVector;
                }
                if (!RotationStep.IsNearlyZero())
                {
                    StabilizedCameraRotation =
                        (FQuat(RotationStep.GetSafeNormal(), RotationStep.Size()) *
                         StabilizedCameraRotation)
                            .GetNormalized();
                }
                RemainingTime -= StepTime;
            }
        }
    }

    if (!StabilizedCameraRotation.IsNormalized() || StabilizedCameraRotation.ContainsNaN() ||
        CameraPosition.ContainsNaN())
    {
        UE_LOG(LogTemp, Error,
               TEXT("First-person camera pose became invalid; resetting camera smoothing"));
        ResetCameraPose();
        return;
    }

    FirstPersonCamera->SetWorldLocationAndRotation(CameraPosition, StabilizedCameraRotation, false,
                                                   nullptr, ETeleportType::TeleportPhysics);
}

FQuat UDroneCameraControllerComponent::GetStabilizedTarget(const FQuat &BodyRotation)
{
    FVector HorizontalForward = BodyRotation.RotateVector(FVector::ForwardVector);
    HorizontalForward.Z = 0.0F;
    if (HorizontalForward.SizeSquared() > MinimumHeadingProjectionSquared)
    {
        LastStableHeadingDegrees =
            FMath::RadiansToDegrees(FMath::Atan2(HorizontalForward.Y, HorizontalForward.X));
    }

    const FQuat DesiredWorldRotation =
        (FRotator(StabilizedPitchDegrees, LastStableHeadingDegrees, 0.0F).Quaternion() *
         MountRotation.Quaternion())
            .GetNormalized();
    return LimitGimbalTravel(BodyRotation, DesiredWorldRotation);
}

FQuat UDroneCameraControllerComponent::LimitGimbalTravel(const FQuat &BodyRotation,
                                                         const FQuat &DesiredWorldRotation) const
{
    FRotator RelativeGimbalRotation = (BodyRotation.Inverse() * DesiredWorldRotation).Rotator();
    RelativeGimbalRotation.Pitch =
        FMath::Clamp(FRotator::NormalizeAxis(RelativeGimbalRotation.Pitch),
                     MinimumGimbalPitchDegrees, MaximumGimbalPitchDegrees);
    RelativeGimbalRotation.Roll = FMath::Clamp(FRotator::NormalizeAxis(RelativeGimbalRotation.Roll),
                                               -MaximumGimbalRollDegrees, MaximumGimbalRollDegrees);
    RelativeGimbalRotation.Yaw = FMath::Clamp(FRotator::NormalizeAxis(RelativeGimbalRotation.Yaw),
                                              -MaximumGimbalYawDegrees, MaximumGimbalYawDegrees);
    return (BodyRotation * RelativeGimbalRotation.Quaternion()).GetNormalized();
}

void UDroneCameraControllerComponent::UpdateFirstPersonVisibility()
{
    AActor *Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    const bool bFirstPerson = ViewMode != EDroneViewMode::ThirdPerson;
    TInlineComponentArray<UPrimitiveComponent *> PrimitiveComponents(Owner);
    for (UPrimitiveComponent *Primitive : PrimitiveComponents)
    {
        if (Primitive && Primitive->bOwnerNoSee != bFirstPerson)
        {
            Primitive->SetOwnerNoSee(bFirstPerson);
        }
    }
}

// User-requested adjustments only; historical body-locked pose calculation is unchanged.
void UDroneCameraControllerComponent::AdjustGimbalPitch(float Degrees)
{
    if (ViewMode == EDroneViewMode::StabilizedFirstPerson)
    {
        StabilizedPitchDegrees = FMath::Clamp(StabilizedPitchDegrees + Degrees,
                                              MinimumGimbalPitchDegrees, MaximumGimbalPitchDegrees);
    }
}

void UDroneCameraControllerComponent::AdjustFieldOfView(float Degrees)
{
    if (ViewMode == EDroneViewMode::ThirdPerson || !FirstPersonCamera)
    {
        return;
    }
    if (ViewMode == EDroneViewMode::StabilizedFirstPerson)
    {
        StabilizedFieldOfView = FMath::Clamp(StabilizedFieldOfView + Degrees, 60.0F, 120.0F);
        FirstPersonCamera->SetFieldOfView(StabilizedFieldOfView);
    }
    else
    {
        BodyLockedFieldOfView = FMath::Clamp(BodyLockedFieldOfView + Degrees, 60.0F, 120.0F);
        FirstPersonCamera->SetFieldOfView(BodyLockedFieldOfView);
    }
}

void UDroneCameraControllerComponent::RecenterGimbal()
{
    if (ViewMode == EDroneViewMode::StabilizedFirstPerson)
    {
        StabilizedPitchDegrees = 0.0F;
    }
    ResetCameraPose();
}
