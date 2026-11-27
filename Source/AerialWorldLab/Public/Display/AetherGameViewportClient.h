#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "AetherGameViewportClient.generated.h"

// Handle the display shortcut at the viewport, including while flight is paused.
UCLASS()

class AERIALWORLDLAB_API UAetherGameViewportClient : public UGameViewportClient
{
    GENERATED_BODY()
  public:
    virtual bool InputKey(const FInputKeyEventArgs &EventArgs) override;
    static EWindowMode::Type GetFullscreenToggleTarget(EWindowMode::Type CurrentMode);

  private:
    void ToggleFullscreen();
    FIntPoint PreviousWindowedResolution = FIntPoint::ZeroValue;
};
