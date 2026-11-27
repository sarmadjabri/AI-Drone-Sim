#include "Display/AetherGameViewportClient.h"

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"
#include "GenericPlatform/GenericWindow.h"
#if PLATFORM_LINUX
#include "SDL.h"
#endif

EWindowMode::Type UAetherGameViewportClient::GetFullscreenToggleTarget(
    EWindowMode::Type CurrentMode)
{
    if (CurrentMode == EWindowMode::Windowed)
    {
        // Borderless fills the display without changing its refresh rate or display mode.
        return EWindowMode::WindowedFullscreen;
    }
    return EWindowMode::Windowed;
}

bool UAetherGameViewportClient::InputKey(const FInputKeyEventArgs &EventArgs)
{
    if (IsRunningGame() && EventArgs.Key == EKeys::F11)
    {
        if (EventArgs.Event == IE_Pressed)
        {
            ToggleFullscreen();
        }
        // Consume repeats/releases as well, so the built-in handler cannot toggle twice.
        return true;
    }
    return Super::InputKey(EventArgs);
}

void UAetherGameViewportClient::ToggleFullscreen()
{
    if (!Viewport || !GEngine)
    {
        return;
    }
    UGameUserSettings *Settings = GEngine->GetGameUserSettings();
    if (!Settings)
    {
        return;
    }
    const EWindowMode::Type CurrentMode = Viewport->GetWindowMode();
    const EWindowMode::Type TargetMode = GetFullscreenToggleTarget(CurrentMode);
    if (CurrentMode == EWindowMode::Windowed)
    {
        PreviousWindowedResolution = Viewport->GetSizeXY();
        // Request native dimensions explicitly; retaining the small window size can
        // make some Linux window managers snap a borderless window back to that size.
        FIntPoint DesktopResolution = Settings->GetDesktopResolution();
        const TSharedPtr<SWindow> Window = GetWindow();
        if (Window.IsValid())
        {
            const FSlateRect MonitorBounds = Window->GetFullScreenInfo();
            DesktopResolution = FIntPoint(FMath::RoundToInt(MonitorBounds.GetSize().X),
                                          FMath::RoundToInt(MonitorBounds.GetSize().Y));
            // Unreal initializes maximum window size from the desktop work area.
            // Remove the taskbar-sized restriction before requesting a full monitor.
            FWindowSizeLimits Limits = Window->GetSizeLimits();
            Limits.SetMaxWidth(float(DesktopResolution.X));
            Limits.SetMaxHeight(float(DesktopResolution.Y));
            Window->SetSizeLimits(Limits);
#if PLATFORM_LINUX
            const TSharedPtr<FGenericWindow> NativeWindow = Window->GetNativeWindow();
            if (NativeWindow.IsValid())
            {
                // Updating Slate limits alone does not refresh SDL's X11 size hints.
                SDL_Window *SdlWindow =
                    static_cast<SDL_Window *>(NativeWindow->GetOSWindowHandle());
                SDL_SetWindowMaximumSize(SdlWindow, DesktopResolution.X, DesktopResolution.Y);
            }
#endif
        }
        if (DesktopResolution.X > 0 && DesktopResolution.Y > 0)
        {
            Settings->SetScreenResolution(DesktopResolution);
        }
    }
    else if (PreviousWindowedResolution.X > 0 && PreviousWindowedResolution.Y > 0)
    {
        Settings->SetScreenResolution(PreviousWindowedResolution);
    }
    Settings->SetFullscreenMode(TargetMode);
    // Do not reapply scalability or let launch flags override this explicit user action.
    Settings->ApplyResolutionSettings(false);
    Settings->ConfirmVideoMode();
    Settings->SaveSettings();
    UE_LOG(LogTemp, Display, TEXT("F11 window mode: %d -> %d"), int32(CurrentMode),
           int32(TargetMode));
}
