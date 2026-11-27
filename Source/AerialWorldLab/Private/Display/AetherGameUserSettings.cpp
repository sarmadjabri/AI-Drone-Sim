#include "Display/AetherGameUserSettings.h"
#include "Scalability.h"

namespace
{
    constexpr int32 BalancedGraphicsPresetVersion = 2;
}

void UAetherGameUserSettings::ApplyBalancedGraphics()
{
    // Keep geometry and textures recognizable while avoiding costly Epic lighting defaults.
    SetOverallScalabilityLevel(2);
    SetShadowQuality(1);
    SetGlobalIlluminationQuality(0);
    SetReflectionQuality(0);
    SetVisualEffectQuality(1);
    SetPostProcessingQuality(1);
    SetResolutionScaleValueEx(100.0F);
}

void UAetherGameUserSettings::LoadSettings(bool bForceReload)
{
    Super::LoadSettings(bForceReload);
    if (AetherGraphicsPresetVersion < BalancedGraphicsPresetVersion)
    {
        ApplyBalancedGraphics();
        // Engine startup skips ApplyNonResolutionSettings' scalability update until
        // initialization completes. Update the global groups before saving the migration.
        Scalability::SetQualityLevels(ScalabilityQuality);
        AetherGraphicsPresetVersion = BalancedGraphicsPresetVersion;
        SaveSettings();
    }
}
