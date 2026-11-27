// Runs the imported city level; authored environment geometry is stored in the map.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AetherGameMode.generated.h"

class UCitySimulationSettings;

UCLASS(Config = Game)

class AERIALWORLDLAB_API AAetherGameMode : public AGameModeBase
{
    GENERATED_BODY()
  public:
    AAetherGameMode();
    virtual APawn *SpawnDefaultPawnAtTransform_Implementation(
        AController *NewPlayer, const FTransform &SpawnTransform) override;

  protected:
    virtual void BeginPlay() override;

  private:
    void SetupEnvironmentLighting();
    bool SpawnCityTraffic();
    bool LoadStreetCircuit(TArray<FVector> &Points) const;
    void EnsureSkyAtmosphere();
    void ConfigureSunlight(const UCitySimulationSettings &CitySettings);
    bool ConfigureSkylight(const UCitySimulationSettings &CitySettings);
    void ConfigureExposure(const UCitySimulationSettings &CitySettings);

    UPROPERTY(EditAnywhere, Config, Category = "City|Traffic")
    FString TrafficManifestRelativePath = TEXT("CityData/new_york_traffic.json");
    UPROPERTY(EditAnywhere, Config, Category = "City|Traffic",
              meta = (ClampMin = "0", ClampMax = "16"))
    int32 MaximumMovingVehicles = 4;
    friend class FFlightCityTest;
};
