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

    UPilotControlsComponent* Controls = Drone->GetPilotControls();
    Controls->RefreshPausedInput();
    if (Controls->IsMenuOpen())
    {
        DrawRect(FLinearColor(0.01F,0.02F,0.03F,0.95F),0,0,Canvas->ClipX,Canvas->ClipY);
        DrawText(Controls->DescribeMenu(),FLinearColor::White,24,24,nullptr,1.0F,false);
        if (Controls->IsMapPage())
        {
            const float Scale=FMath::Min(Canvas->ClipX/1280.0F,Canvas->ClipY/720.0F);
            const float X0=(Canvas->ClipX-1100*Scale)*0.5F, Y0=120*Scale;
            DrawRect(FLinearColor(0.07F,0.09F,0.13F),X0,Y0,1100*Scale,470*Scale);
            struct FMapButton { FKey Key; float X,Y; };
            const FMapButton Buttons[]={
                {EKeys::Gamepad_LeftTrigger,20,15},{EKeys::Gamepad_RightTrigger,820,15},
                {EKeys::Gamepad_LeftShoulder,20,80},{EKeys::Gamepad_RightShoulder,820,80},
                {EKeys::Gamepad_Special_Left,325,15},{EKeys::Gamepad_Special_Right,575,15},
                {EKeys::Gamepad_DPad_Up,20,155},{EKeys::Gamepad_DPad_Left,20,220},
                {EKeys::Gamepad_DPad_Right,20,285},{EKeys::Gamepad_DPad_Down,20,350},
                {EKeys::Gamepad_FaceButton_Top,820,155},{EKeys::Gamepad_FaceButton_Left,820,220},
                {EKeys::Gamepad_FaceButton_Right,820,285},{EKeys::Gamepad_FaceButton_Bottom,820,350},
                {EKeys::Gamepad_LeftThumbstick,325,350},{EKeys::Gamepad_RightThumbstick,575,350}};
            for (const auto& B:Buttons)
            {
                const bool Active=Controls->IsKeyHeld(B.Key) ||
                    (B.Key==EKeys::Gamepad_LeftTrigger && Controls->Raw[4]>0.05F) ||
                    (B.Key==EKeys::Gamepad_RightTrigger && Controls->Raw[5]>0.05F);
                DrawRect(Active ? FLinearColor(0.05F,0.38F,0.28F) : FLinearColor(0.13F,0.16F,0.21F),
                    X0+B.X*Scale,Y0+B.Y*Scale,250*Scale,55*Scale);
                DrawText(Controls->ButtonLabel(B.Key)+TEXT("\n")+Controls->BindingLabel(B.Key),
                    FLinearColor::White,X0+(B.X+8)*Scale,Y0+(B.Y+6)*Scale,nullptr,Scale,false);
            }
            // Stick crosshairs display physical values, before calibration/expo.
            for (int I=0;I<2;++I)
            {
                const float X=X0+(420+I*250)*Scale,Y=Y0+230*Scale;
                DrawLine(X-55*Scale,Y,X+55*Scale,Y,FLinearColor::Gray,1);
                DrawLine(X,Y-55*Scale,X,Y+55*Scale,FLinearColor::Gray,1);
                DrawRect(FLinearColor::Green,X+Controls->Raw[I*2]*50*Scale-4,
                    Y-Controls->Raw[I*2+1]*50*Scale-4,8,8);
            }
            DrawText(TEXT("Left stick") ,FLinearColor::White,X0+370*Scale,Y0+300*Scale,nullptr,Scale,false);
            DrawText(TEXT("Right stick"),FLinearColor::White,X0+620*Scale,Y0+300*Scale,nullptr,Scale,false);
        }
        return;
    }
    FString Telemetry = Drone->GetControlStatus();
    if (Controls->IsTelemetryVisible()) Telemetry += Controls->Diagnostics()+TEXT("\n");
    Telemetry+=TEXT("Camera: ")+Controls->ActionBindings(EPilotAction::Camera)+
        TEXT(" | Settings: ")+Controls->ActionBindings(EPilotAction::Settings)+TEXT("\n")+
        FString::Printf(TEXT("Reset (hold): %s %.0f%% | Takeoff: %s | Land: %s\n"),
            *Controls->ActionBindings(EPilotAction::Reset),Drone->GetResetProgress()*100,
            *Controls->ActionBindings(EPilotAction::Takeoff),*Controls->ActionBindings(EPilotAction::Land));
    if (Drone->IsFirstPerson())
    {
        const float X = Canvas->ClipX * 0.5F;
        const float Y = Canvas->ClipY * 0.5F;
        DrawLine(X - 7, Y, X + 7, Y, FLinearColor::White, 1.0F);
        DrawLine(X, Y - 7, X, Y + 7, FLinearColor::White, 1.0F);
    }
    DrawText(Telemetry, FLinearColor::White, 28.0F, 32.0F, nullptr, 1.0F, false);
}
