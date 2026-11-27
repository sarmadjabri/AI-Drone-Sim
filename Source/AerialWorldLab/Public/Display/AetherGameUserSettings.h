#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "AetherGameUserSettings.generated.h"

UCLASS(Config = GameUserSettings)

class AERIALWORLDLAB_API UAetherGameUserSettings : public UGameUserSettings
{
    GENERATED_BODY()
  public:
    virtual void LoadSettings(bool bForceReload = false) override;
    void ApplyBalancedGraphics();

  private:
    // Apply the simulator's initial preset once; retain subsequent user changes.
    UPROPERTY(Config)
    int32 AetherGraphicsPresetVersion = 0;
};
