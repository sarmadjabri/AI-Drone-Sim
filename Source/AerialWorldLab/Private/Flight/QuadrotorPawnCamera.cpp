// Camera selection and zoom; these controls do not alter flight laws.
#include "Flight/QuadrotorPawn.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

void AQuadrotorPawn::SetCameraZoomInput(float Value)
{
    CameraZoomInput = FMath::Clamp(Value, -1.0F, 1.0F);
}

void AQuadrotorPawn::ApplyViewMode()
{
    Camera->SetActive(!IsFirstPerson());
    FirstPersonCamera->SetActive(IsFirstPerson());
    Camera->SetFieldOfView(ThirdPersonFieldOfView);
    FirstPersonCameraController->SetViewMode(ViewMode);
}

void AQuadrotorPawn::ToggleViewMode()
{
    switch (ViewMode)
    {
    case EDroneViewMode::ThirdPerson:
        ViewMode = EDroneViewMode::StabilizedFirstPerson;
        break;
    case EDroneViewMode::StabilizedFirstPerson:
        ViewMode = EDroneViewMode::BodyLockedFirstPerson;
        break;
    default:
        ViewMode = EDroneViewMode::ThirdPerson;
        break;
    }
    ApplyViewMode();
    if (InputProfile == EDroneInputProfile::LegacyViewDependent)
    {
        RequireNeutralInput();
    }
}

void AQuadrotorPawn::AdjustPilotCameraZoom(float Amount)
{
    if (IsFirstPerson())
    {
        FirstPersonCameraController->AdjustFieldOfView(-Amount * 3);
    }
    else
    {
        CameraBoom->TargetArmLength =
            FMath::Clamp(CameraBoom->TargetArmLength - Amount * 30, 300.0F, 900.0F);
    }
}

void AQuadrotorPawn::UpdateChaseCameraZoom(float DeltaSeconds)
{
    float ThirdPersonZoomInput = 0.0F;
    if (!IsFirstPerson())
    {
        ThirdPersonZoomInput = CameraZoomInput;
    }
    CameraBoom->TargetArmLength = FMath::Clamp(
        CameraBoom->TargetArmLength - ThirdPersonZoomInput * CameraZoomSpeed * DeltaSeconds, 300.0F,
        900.0F);
}
