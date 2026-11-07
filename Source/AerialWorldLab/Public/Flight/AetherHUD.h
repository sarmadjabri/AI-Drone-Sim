#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AetherHUD.generated.h"

UCLASS()
class AERIALWORLDLAB_API AAetherHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
};
