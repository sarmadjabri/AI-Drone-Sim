#include "Flight/AetherHUD.h"

#include "Engine/Canvas.h"
#include "Flight/QuadrotorPawn.h"
#include "GameFramework/PlayerController.h"

void AAetherHUD::DrawHUD()
{
    Super::DrawHUD();
    const AQuadrotorPawn* Drone = Cast<AQuadrotorPawn>(
        GetOwningPlayerController() ? GetOwningPlayerController()->GetPawn() : nullptr);
    if (!Drone || !Canvas)
    {
        return;
    }

    const FVector PositionMeters = Drone->GetActorLocation() / 100.0F;
    const FVector Velocity = Drone->GetVelocityMetersPerSecond();
    const FVector2D Input = Drone->GetMovementInput();
    const FString Telemetry = FString::Printf(
        TEXT("AERIAL WORLD LAB | %s\n")
        TEXT("Position (m): X %6.2f  Y %6.2f  Z %6.2f\n")
        TEXT("Velocity (m/s): X %5.2f  Y %5.2f  Z %5.2f\n")
        TEXT("Stick forward/right: %+.2f / %+.2f  |  Position hold: %s\n")
        TEXT("Motor thrust (N): %4.1f  %4.1f  %4.1f  %4.1f\n")
        TEXT("Collective (N): %5.1f / %5.1f\n")
        TEXT("%s\n")
        TEXT("Triangle/V: switch view  |  Square/Backspace: reset  |  F11: fullscreen\n")
        TEXT("%s"),
        *Drone->GetViewModeName(),
        PositionMeters.X, PositionMeters.Y, PositionMeters.Z,
        Velocity.X, Velocity.Y, Velocity.Z,
        Input.X, Input.Y, Drone->IsPositionHolding() ? TEXT("ON") : TEXT("OFF"),
        Drone->GetMotorThrustNewtons(0), Drone->GetMotorThrustNewtons(1),
        Drone->GetMotorThrustNewtons(2), Drone->GetMotorThrustNewtons(3),
        Drone->GetCollectiveThrustNewtons(), Drone->GetMaximumCollectiveThrustNewtons(),
        Drone->IsFirstPerson()
            ? TEXT("Left stick: altitude/yaw | Right stick: move | WASD: move | Q/E: yaw | Space/Ctrl: altitude")
            : TEXT("Left stick/WASD: move | Right stick/QE: yaw | L2/R2 or F/R: altitude | D-pad/PageUp/Down: zoom"),
        Drone->IsWaitingForNeutralInput() ? TEXT("Center sticks and release movement keys/triggers to resume") : TEXT(""));
    if (Drone->IsFirstPerson())
    {
        const float X = Canvas->ClipX * 0.5F;
        const float Y = Canvas->ClipY * 0.5F;
        DrawLine(X - 7, Y, X + 7, Y, FLinearColor::White, 1.0F);
        DrawLine(X, Y - 7, X, Y + 7, FLinearColor::White, 1.0F);
    }
    DrawText(Telemetry, FLinearColor::White, 28.0F, 32.0F, nullptr, 1.0F, false);
}
