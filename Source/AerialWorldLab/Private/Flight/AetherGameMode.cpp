#include "Flight/AetherGameMode.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flight/AetherHUD.h"
#include "Flight/CityVehicle.h"
#include "Flight/CitySimulationSettings.h"
#include "Flight/QuadrotorPawn.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    constexpr float LaunchGroundSearchDepthCentimeters = 10000.0F;
    constexpr int32 MinimumStreetCircuitPointCount = 4;
    constexpr int32 MaximumStreetCircuitPointCount = 10000;
    constexpr int32 MaximumSupportedTrafficVehicles = 16;
} // namespace

AAetherGameMode::AAetherGameMode()
{
    DefaultPawnClass = AQuadrotorPawn::StaticClass();
    HUDClass = AAetherHUD::StaticClass();
}

APawn *AAetherGameMode::SpawnDefaultPawnAtTransform_Implementation(AController *NewPlayer,
                                                                   const FTransform &SpawnTransform)
{
    FTransform DroneSpawn = SpawnTransform;
    FVector Location = SpawnTransform.GetLocation();
    // The authored street grade determines the launch height in centimeters.
    // Search below the authored start, so overhead tree canopies are not mistaken for the street.
    // Static city meshes are loaded with the map before the player is spawned.
    FHitResult StreetGroundHit;
    FCollisionQueryParams StreetQuery(SCENE_QUERY_STAT(DroneCityLaunch), true);
    if (GetWorld()->LineTraceSingleByObjectType(
            StreetGroundHit, Location, Location - FVector(0, 0, LaunchGroundSearchDepthCentimeters),
            FCollisionObjectQueryParams(ECC_WorldStatic), StreetQuery))
    {
        Location.Z = StreetGroundHit.ImpactPoint.Z +
                     GetDefault<UCitySimulationSettings>()->DroneLaunchClearanceCentimeters;
    }
    else
    {
        UE_LOG(LogTemp, Warning,
               TEXT("No city ground at launch; retaining the authored player start"));
    }
    DroneSpawn.SetLocation(Location);
    return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, DroneSpawn);
}

void AAetherGameMode::BeginPlay()
{
    Super::BeginPlay();
    // The city is entirely in the native city map. No synthetic buildings or roads are spawned.
    SetupEnvironmentLighting();
    SpawnCityTraffic();
}

bool AAetherGameMode::SpawnCityTraffic()
{
    if (MaximumMovingVehicles <= 0)
    {
        return true;
    }
    TArray<FVector> Points;
    if (!LoadStreetCircuit(Points))
    {
        return false;
    }
    float ApproximateLength = 0;
    for (int32 RouteIndex = 0; RouteIndex < Points.Num(); ++RouteIndex)
    {
        ApproximateLength +=
            FVector::Distance(Points[RouteIndex], Points[(RouteIndex + 1) % Points.Num()]);
    }
    const int32 VehicleCount =
        FMath::Clamp(MaximumMovingVehicles, 0, MaximumSupportedTrafficVehicles);
    for (int32 RouteIndex = 0; RouteIndex < VehicleCount; ++RouteIndex)
    {
        ACityVehicle *Vehicle = GetWorld()->SpawnActor<ACityVehicle>();
        if (Vehicle)
        {
            Vehicle->ConfigureStreetCircuit(Points, ApproximateLength * RouteIndex / VehicleCount);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("City: %d moving vehicles on the authored street circuit"),
           VehicleCount);
    return true;
}

bool AAetherGameMode::LoadStreetCircuit(TArray<FVector> &Points) const
{
    FString ManifestText;
    const FString ManifestPath = FPaths::ProjectContentDir() / TrafficManifestRelativePath;
    TSharedPtr<FJsonObject> Manifest;
    if (!FFileHelper::LoadFileToString(ManifestText, *ManifestPath) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ManifestText), Manifest) ||
        !Manifest.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Street circuit unavailable: %s"), *ManifestPath);
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>> *RoutePointValues = nullptr;
    if (!Manifest->TryGetArrayField(TEXT("points_cm"), RoutePointValues) ||
        RoutePointValues->Num() < MinimumStreetCircuitPointCount ||
        RoutePointValues->Num() > MaximumStreetCircuitPointCount)
    {
        return false;
    }
    const UCitySimulationSettings *CitySettings = GetDefault<UCitySimulationSettings>();
    for (const auto &PointValue : *RoutePointValues)
    {
        if (!PointValue.IsValid() || PointValue->Type != EJson::Array ||
            PointValue->AsArray().Num() != 3)
        {
            return false;
        }
        const auto &Coordinates = PointValue->AsArray();
        double X = 0;
        double Y = 0;
        double Z = 0;
        if (!Coordinates[0]->TryGetNumber(X) || !Coordinates[1]->TryGetNumber(Y) ||
            !Coordinates[2]->TryGetNumber(Z) || !FMath::IsFinite(X) || !FMath::IsFinite(Y) ||
            !FMath::IsFinite(Z) || FMath::Abs(X) > CitySettings->MaximumRoadCoordinateCentimeters ||
            FMath::Abs(Y) > CitySettings->MaximumRoadCoordinateCentimeters)
        {
            return false;
        }
        // The manifest includes expected street elevation in centimeters.
        // A bounded search avoids selecting roofs or tree crowns above the road.
        FVector Point(X, Y, Z);
        FHitResult StreetGroundHit;
        FCollisionQueryParams StreetQuery(SCENE_QUERY_STAT(StreetCircuitGround), true);
        if (!GetWorld()->LineTraceSingleByObjectType(
                StreetGroundHit, Point + FVector(0, 0, CitySettings->StreetProbeAboveCentimeters),
                Point - FVector(0, 0, CitySettings->StreetProbeBelowCentimeters),
                FCollisionObjectQueryParams(ECC_WorldStatic), StreetQuery))
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("Street circuit leaves the city at %.1f, %.1f cm; traffic disabled"), X, Y);
            return false;
        }
        Point.Z = StreetGroundHit.ImpactPoint.Z;
        Points.Add(Point);
    }
    return true;
}

void AAetherGameMode::SetupEnvironmentLighting()
{
    const UCitySimulationSettings &CitySettings = *GetDefault<UCitySimulationSettings>();
    EnsureSkyAtmosphere();
    ConfigureSunlight(CitySettings);
    if (!ConfigureSkylight(CitySettings))
    {
        return;
    }
    ConfigureExposure(CitySettings);
}

void AAetherGameMode::EnsureSkyAtmosphere()
{
    // An atmosphere supplies the visible sky and aerial perspective, not just ambient light.
    bool bHasAtmosphere = false;
    for (TActorIterator<ASkyAtmosphere> ActorIterator(GetWorld()); ActorIterator; ++ActorIterator)
    {
        bHasAtmosphere = true;
        break;
    }
    if (!bHasAtmosphere)
    {
        GetWorld()->SpawnActor<ASkyAtmosphere>();
    }
}

void AAetherGameMode::ConfigureSunlight(const UCitySimulationSettings &CitySettings)
{
    ADirectionalLight *Sun = nullptr;
    for (TActorIterator<ADirectionalLight> ActorIterator(GetWorld()); ActorIterator;
         ++ActorIterator)
    {
        Sun = *ActorIterator;
        break;
    }

    if (!Sun)
    {
        Sun = GetWorld()->SpawnActor<ADirectionalLight>(
            ADirectionalLight::StaticClass(), FVector::ZeroVector, CitySettings.SunRotationDegrees);
    }
    if (Sun)
    {
        ULightComponent *Light = Sun->GetLightComponent();
        Light->SetMobility(EComponentMobility::Movable);
        // High sun and neutral daylight illuminate the original brick and greenery.
        Sun->SetActorRotation(CitySettings.SunRotationDegrees);
        // Directional intensity is illuminance in lux; 50 klux is the daytime preset.
        Light->SetIntensity(CitySettings.SunIlluminanceLux);
        Light->SetLightColor(CitySettings.SunColor);
        Light->SetCastShadows(true);
        UDirectionalLightComponent *SunComponent = Cast<UDirectionalLightComponent>(Light);
        if (SunComponent)
        {
            SunComponent->bAtmosphereSunLight = true;
            SunComponent->MarkRenderStateDirty();
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create the scene directional light"));
    }
}

bool AAetherGameMode::ConfigureSkylight(const UCitySimulationSettings &CitySettings)
{
    ASkyLight *Sky = nullptr;
    for (TActorIterator<ASkyLight> ActorIterator(GetWorld()); ActorIterator; ++ActorIterator)
    {
        Sky = *ActorIterator;
        break;
    }

    if (!Sky)
    {
        Sky = GetWorld()->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector::ZeroVector,
                                                FRotator::ZeroRotator);
    }
    if (!Sky)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create the scene sky light"));
        return false;
    }

    USkyLightComponent *Light = Sky->GetLightComponent();
    Light->SetMobility(EComponentMobility::Movable);
    Light->SetIntensity(CitySettings.SkylightIntensity);
    Light->SetLightColor(FLinearColor::White);
    Light->RecaptureSky();

    return true;
}

void AAetherGameMode::ConfigureExposure(const UCitySimulationSettings &CitySettings)
{
    APostProcessVolume *ExposureVolume = nullptr;
    for (TActorIterator<APostProcessVolume> ActorIterator(GetWorld()); ActorIterator;
         ++ActorIterator)
    {
        ExposureVolume = *ActorIterator;
        break;
    }

    if (!ExposureVolume)
    {
        ExposureVolume = GetWorld()->SpawnActor<APostProcessVolume>(
            APostProcessVolume::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
    }
    if (!ExposureVolume)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create the scene exposure volume"));
        return;
    }

    ExposureVolume->bUnbound = true;
    ExposureVolume->Settings.bOverride_AutoExposureMethod = true;
    ExposureVolume->Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
    ExposureVolume->Settings.bOverride_AutoExposureBias = true;
    ExposureVolume->Settings.AutoExposureBias = 0.0F;
    // Fixed daytime exposure prevents brightness pumping as the FPV camera turns.
    // ISO 100, f/8 and 1/100 s correspond to EV100 approximately 12.6.
    ExposureVolume->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    ExposureVolume->Settings.AutoExposureApplyPhysicalCameraExposure = true;
    ExposureVolume->Settings.bOverride_CameraISO = true;
    ExposureVolume->Settings.CameraISO = CitySettings.CameraISO;
    ExposureVolume->Settings.bOverride_CameraShutterSpeed = true;
    ExposureVolume->Settings.CameraShutterSpeed = CitySettings.CameraShutterSpeedReciprocalSeconds;
    ExposureVolume->Settings.bOverride_DepthOfFieldFstop = true;
    ExposureVolume->Settings.DepthOfFieldFstop = CitySettings.CameraFStop;
}
