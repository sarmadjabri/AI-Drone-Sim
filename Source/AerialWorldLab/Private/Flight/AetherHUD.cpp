// Renders flight telemetry and controller diagnostics without changing physical state.
#include "Flight/AetherHUD.h"
#include "Flight/CitySimulationSettings.h"

#include "Engine/Canvas.h"
#include "Flight/QuadrotorPawn.h"
#include "GameFramework/PlayerController.h"

void AAetherHUD::DrawHUD()
{
    Super::DrawHUD();
    const APlayerController *Controller = GetOwningPlayerController();
    if (!Controller)
    {
        return;
    }
    const AQuadrotorPawn *Drone = Cast<AQuadrotorPawn>(Controller->GetPawn());
    if (!Drone || !Canvas)
    {
        return;
    }

    UPilotControlsComponent *Controls = Drone->GetPilotControls();
    Controls->RefreshPausedInput();
    if (Controls->IsMenuOpen())
    {
        NextTelemetryUpdateSeconds = 0.0;
        DrawRect(FLinearColor(0.01F, 0.02F, 0.03F, 0.95F), 0, 0, Canvas->ClipX, Canvas->ClipY);
        DrawText(Controls->DescribeMenu(), FLinearColor::White, 24, 24, nullptr, 1.0F, false);
        if (Controls->IsMapPage())
        {
            const float Scale = FMath::Min(Canvas->ClipX / 1280.0F, Canvas->ClipY / 720.0F);
            const float MapOriginX = (Canvas->ClipX - 1100 * Scale) * 0.5F;
            const float MapOriginY = 120 * Scale;
            DrawRect(FLinearColor(0.07F, 0.09F, 0.13F), MapOriginX, MapOriginY, 1100 * Scale,
                     470 * Scale);

            struct FMapButton
            {
                FKey Key;
                float X;
                float Y;
            };

            const FMapButton Buttons[] = {{EKeys::Gamepad_LeftTrigger, 20, 15},
                                          {EKeys::Gamepad_RightTrigger, 820, 15},
                                          {EKeys::Gamepad_LeftShoulder, 20, 80},
                                          {EKeys::Gamepad_RightShoulder, 820, 80},
                                          {EKeys::Gamepad_Special_Left, 325, 15},
                                          {EKeys::Gamepad_Special_Right, 575, 15},
                                          {EKeys::Gamepad_DPad_Up, 20, 155},
                                          {EKeys::Gamepad_DPad_Left, 20, 220},
                                          {EKeys::Gamepad_DPad_Right, 20, 285},
                                          {EKeys::Gamepad_DPad_Down, 20, 350},
                                          {EKeys::Gamepad_FaceButton_Top, 820, 155},
                                          {EKeys::Gamepad_FaceButton_Left, 820, 220},
                                          {EKeys::Gamepad_FaceButton_Right, 820, 285},
                                          {EKeys::Gamepad_FaceButton_Bottom, 820, 350},
                                          {EKeys::Gamepad_LeftThumbstick, 325, 350},
                                          {EKeys::Gamepad_RightThumbstick, 575, 350}};
            for (const auto &Button : Buttons)
            {
                const bool bIsButtonActive =
                    Controls->IsKeyHeld(Button.Key) ||
                    (Button.Key == EKeys::Gamepad_LeftTrigger &&
                     Controls->Raw[Aether::PhysicalAxis::LeftTrigger] > 0.05F) ||
                    (Button.Key == EKeys::Gamepad_RightTrigger &&
                     Controls->Raw[Aether::PhysicalAxis::RightTrigger] > 0.05F);
                FLinearColor ButtonColor(0.13F, 0.16F, 0.21F);
                if (bIsButtonActive)
                {
                    ButtonColor = FLinearColor(0.05F, 0.38F, 0.28F);
                }
                DrawRect(ButtonColor, MapOriginX + Button.X * Scale, MapOriginY + Button.Y * Scale,
                         250 * Scale, 55 * Scale);
                DrawText(Controls->ButtonLabel(Button.Key) + TEXT("\n") +
                             Controls->BindingLabel(Button.Key),
                         FLinearColor::White, MapOriginX + (Button.X + 8) * Scale,
                         MapOriginY + (Button.Y + 6) * Scale, nullptr, Scale, false);
            }
            // Stick crosshairs display physical values, before calibration/expo.
            for (int StickIndex = 0; StickIndex < 2; ++StickIndex)
            {
                const float X = MapOriginX + (420 + StickIndex * 250) * Scale;
                const float Y = MapOriginY + 230 * Scale;
                DrawLine(X - 55 * Scale, Y, X + 55 * Scale, Y, FLinearColor::Gray, 1);
                DrawLine(X, Y - 55 * Scale, X, Y + 55 * Scale, FLinearColor::Gray, 1);
                DrawRect(FLinearColor::Green, X + Controls->Raw[StickIndex * 2] * 50 * Scale - 4,
                         Y - Controls->Raw[StickIndex * 2 + 1] * 50 * Scale - 4, 8, 8);
            }
            DrawText(TEXT("Left stick"), FLinearColor::White, MapOriginX + 370 * Scale,
                     MapOriginY + 300 * Scale, nullptr, Scale, false);
            DrawText(TEXT("Right stick"), FLinearColor::White, MapOriginX + 620 * Scale,
                     MapOriginY + 300 * Scale, nullptr, Scale, false);
        }
        return;
    }
    const double NowSeconds = FPlatformTime::Seconds();
    if (CachedDrone.Get() != Drone || NowSeconds >= NextTelemetryUpdateSeconds)
    {
        CachedDrone = Drone;
        NextTelemetryUpdateSeconds = NowSeconds + 0.1;
        CachedTelemetry = Drone->GetControlStatus();
        if (Controls->IsTelemetryVisible())
        {
            CachedTelemetry += Controls->Diagnostics() + TEXT("\n");
        }
        CachedTelemetry +=
            TEXT("Camera: ") + Controls->ActionBindings(EPilotAction::Camera) +
            TEXT(" | Settings: ") + Controls->ActionBindings(EPilotAction::Settings) + TEXT("\n") +
            FString::Printf(TEXT("Reset (hold): %s %.0f%% | Takeoff: %s | Land: %s\n"),
                            *Controls->ActionBindings(EPilotAction::Reset),
                            Drone->GetResetProgress() * 100,
                            *Controls->ActionBindings(EPilotAction::Takeoff),
                            *Controls->ActionBindings(EPilotAction::Land));
    }
    if (Drone->IsFirstPerson())
    {
        const float X = Canvas->ClipX * 0.5F;
        const float Y = Canvas->ClipY * 0.5F;
        DrawLine(X - 7, Y, X + 7, Y, FLinearColor::White, 1.0F);
        DrawLine(X, Y - 7, X, Y + 7, FLinearColor::White, 1.0F);
    }
    DrawText(GetDefault<UCitySimulationSettings>()->EnvironmentCredits, FLinearColor::White, 20,
             Canvas->ClipY - 26, nullptr, 0.85F, false);
    DrawText(CachedTelemetry, FLinearColor::White, 28.0F, 32.0F, nullptr, 1.0F, false);
}
