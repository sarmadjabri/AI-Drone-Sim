// Shared city policy, independent of the imported map's geometry or tile count.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CitySimulationSettings.generated.h"

UCLASS(Config = Game, DefaultConfig)

class AERIALWORLDLAB_API UCitySimulationSettings : public UObject
{
    GENERATED_BODY()

  public:
    // Bounded street probes ignore roofs and tree crowns above the expected grade.
    UPROPERTY(EditAnywhere, Config, Category = "City|Collision")
    float StreetProbeAboveCentimeters = 150.0F;
    UPROPERTY(EditAnywhere, Config, Category = "City|Collision")
    float StreetProbeBelowCentimeters = 300.0F;
    UPROPERTY(EditAnywhere, Config, Category = "City|Flight")
    float DroneLaunchClearanceCentimeters = 220.0F;

    // Coordinate acceptance is a dataset policy, not a fixed 1 km city limit.
    UPROPERTY(EditAnywhere, Config, Category = "City|Traffic")
    double MaximumRoadCoordinateCentimeters = 10000000.0;

    // Directional illuminance is in lux. Skylight intensity is a multiplier.
    UPROPERTY(EditAnywhere, Config, Category = "City|Daylight")
    float SunIlluminanceLux = 50000.0F;
    UPROPERTY(EditAnywhere, Config, Category = "City|Daylight")
    FRotator SunRotationDegrees = FRotator(-55.0F, -35.0F, 0.0F);
    UPROPERTY(EditAnywhere, Config, Category = "City|Daylight")
    FLinearColor SunColor = FLinearColor(1.0F, 0.98F, 0.95F);
    UPROPERTY(EditAnywhere, Config, Category = "City|Daylight")
    float SkylightIntensity = 1.2F;

    // Fixed exposure avoids brightness pumping when flying between facades.
    UPROPERTY(EditAnywhere, Config, Category = "City|Camera")
    float CameraISO = 100.0F;
    UPROPERTY(EditAnywhere, Config, Category = "City|Camera")
    float CameraShutterSpeedReciprocalSeconds = 100.0F;
    UPROPERTY(EditAnywhere, Config, Category = "City|Camera")
    float CameraFStop = 8.0F;

    UPROPERTY(EditAnywhere, Config, Category = "City|Attribution")
    FString EnvironmentCredits =
        TEXT("NY-style environment: NVIDIA Brownstone | Car: DGG/Eric Chadwick, CC BY 4.0");
};
