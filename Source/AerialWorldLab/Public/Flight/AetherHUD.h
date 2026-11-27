// Renders flight telemetry and controller diagnostics without changing physical state.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AetherHUD.generated.h"

class AQuadrotorPawn;

UCLASS()

class AERIALWORLDLAB_API AAetherHUD : public AHUD
{
    GENERATED_BODY()

  public:
    virtual void DrawHUD() override;

  private:
    // Draw every frame, but format expensive diagnostic strings at a readable 10 Hz.
    FString CachedTelemetry;
    TWeakObjectPtr<const AQuadrotorPawn> CachedDrone;
    double NextTelemetryUpdateSeconds = 0.0;
};
