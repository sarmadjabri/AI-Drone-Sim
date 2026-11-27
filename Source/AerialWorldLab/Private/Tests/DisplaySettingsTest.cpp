#include "Display/AetherGameUserSettings.h"
#include "Display/AetherGameViewportClient.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAetherDisplaySettingsTest, "Aether.Flight.DisplaySettings",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FAetherDisplaySettingsTest::RunTest(const FString &Parameters)
{
    TestEqual(TEXT("F11 enters borderless fullscreen"),
              UAetherGameViewportClient::GetFullscreenToggleTarget(EWindowMode::Windowed),
              EWindowMode::WindowedFullscreen);
    TestEqual(TEXT("F11 exits borderless fullscreen"),
              UAetherGameViewportClient::GetFullscreenToggleTarget(EWindowMode::WindowedFullscreen),
              EWindowMode::Windowed);
    TestEqual(TEXT("F11 exits exclusive fullscreen"),
              UAetherGameViewportClient::GetFullscreenToggleTarget(EWindowMode::Fullscreen),
              EWindowMode::Windowed);

    // Presets must change rendering only, preserving display size and window mode.
    UAetherGameUserSettings *Settings = NewObject<UAetherGameUserSettings>();
    Settings->SetScreenResolution(FIntPoint(1280, 720));
    Settings->SetFullscreenMode(EWindowMode::Windowed);
    Settings->ApplyBalancedGraphics();
    TestEqual(TEXT("Balanced preset preserves resolution"), Settings->GetScreenResolution(),
              FIntPoint(1280, 720));
    TestEqual(TEXT("Balanced preset preserves window mode"), Settings->GetFullscreenMode(),
              EWindowMode::Windowed);
    TestEqual(TEXT("Balanced preset keeps medium texture quality"), Settings->GetTextureQuality(),
              2);
    TestEqual(TEXT("Balanced preset avoids expensive GI"), Settings->GetGlobalIlluminationQuality(),
              0);
    TestEqual(TEXT("Balanced preset limits shadow cost"), Settings->GetShadowQuality(), 1);
    return true;
}
#endif
