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
    const FString Telemetry = FString::Printf(
        TEXT("AERIAL WORLD LAB | %s FLIGHT\n")
        TEXT("Position (m): X %6.2f  Y %6.2f  Z %6.2f\n")
        TEXT("Velocity (m/s): X %5.2f  Y %5.2f  Z %5.2f\n")
        TEXT("Motor thrust (N): %4.1f  %4.1f  %4.1f  %4.1f\n")
        TEXT("Collective (N): %5.1f / %5.1f\n")
        TEXT("Triangle/Tab: flight mode  |  Square/Backspace: reset  |  D-pad/PageUp/Down: camera"),
        *Drone->GetFlightModeName(),
        PositionMeters.X, PositionMeters.Y, PositionMeters.Z,
        Velocity.X, Velocity.Y, Velocity.Z,
        Drone->GetMotorThrustNewtons(0), Drone->GetMotorThrustNewtons(1),
        Drone->GetMotorThrustNewtons(2), Drone->GetMotorThrustNewtons(3),
        Drone->GetCollectiveThrustNewtons(), Drone->GetMaximumCollectiveThrustNewtons());
    DrawText(Telemetry, FLinearColor::White, 28.0F, 32.0F, nullptr, 1.0F, false);
}
