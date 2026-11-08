#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AetherGameMode.generated.h"

class AStaticMeshActor;
class UStaticMesh;

UCLASS()
class AERIALWORLDLAB_API AAetherGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AAetherGameMode();

    virtual APawn* SpawnDefaultPawnAtTransform_Implementation(
        AController* NewPlayer,
        const FTransform& SpawnTransform) override;

protected:
    virtual void BeginPlay() override;

private:
    friend class FFlightImportedAssetsTest;
    void SetupEnvironmentLighting();
    AStaticMeshActor* SpawnTestBox(
        const FVector& Location,
        const FVector& Scale,
        const FLinearColor& Color);
    void SpawnBuilding(
        const FVector& Location,
        const FVector& Scale,
        const FLinearColor& FacadeColor,
        int32 Floors,
        int32 WindowColumns);

    UPROPERTY()
    TObjectPtr<UStaticMesh> ApartmentFacadePanelMesh;
};
