#include "Flight/AetherGameMode.h"
#include "Flight/CityVehicle.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#if WITH_EDITOR
#include "StaticMeshCompiler.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightCityTest, "Aether.Flight.CityEnvironment",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

bool FFlightCityTest::RunTest(const FString &Parameters)
{
    TestTrue(TEXT("Native authored city map exists"),
             FPackageName::DoesPackageExist(TEXT("/Game/Maps/NewYorkDaytime")));
    FString ManifestText;
    const FString ManifestPath =
        FPaths::ProjectDir() / TEXT("Content/CityData/new_york_scene.json");
    TSharedPtr<FJsonObject> Manifest;
    if (!FFileHelper::LoadFileToString(ManifestText, *ManifestPath) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ManifestText), Manifest))
    {
        AddError(TEXT("Missing authored city asset manifest"));
        return false;
    }
    UWorld *World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    World->bBegunPlay = true;
    FBox CityBounds(ForceInit);
    // Load the same authored meshes as the native map, without depending on editor UI/map loading.
    for (const auto &Value : Manifest->GetArrayField(TEXT("batches")))
    {
        const FString Name = Value->AsObject()->GetStringField(TEXT("name"));
        UStaticMesh *Mesh = LoadObject<UStaticMesh>(
            nullptr, *(TEXT("/Game/External/NewYorkBrownstone/") + Name + TEXT(".") + Name));
        if (!TestNotNull(TEXT("Authored city mesh loads"), Mesh))
        {
            continue;
        }
        AStaticMeshActor *Surface = World->SpawnActor<AStaticMeshActor>();
        UStaticMeshComponent *Component = Surface->GetStaticMeshComponent();
        Component->SetCanEverAffectNavigation(false);
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetStaticMesh(Mesh);
        Surface->SetActorScale3D(FVector(1));
        Component->SetCollisionProfileName(TEXT("BlockAll"));
        bool bCollision = true;
        Value->AsObject()->TryGetBoolField(TEXT("collision"), bCollision);
        if (!bCollision)
        {
            Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        Component->SetMobility(EComponentMobility::Static);
        CityBounds += Component->Bounds.GetBox();
    }
#if WITH_EDITOR
    FStaticMeshCompilingManager::Get().FinishAllCompilation();
#endif
    const auto &Minimum = Manifest->GetArrayField(TEXT("bounds_min_source_cm"));
    const auto &Maximum = Manifest->GetArrayField(TEXT("bounds_max_source_cm"));
    const FVector ExpectedSize(Maximum[0]->AsNumber() - Minimum[0]->AsNumber(),
                               Maximum[1]->AsNumber() - Minimum[1]->AsNumber(),
                               Maximum[2]->AsNumber() - Minimum[2]->AsNumber());
    TestTrue(TEXT("Native city scale matches the source manifest"),
             CityBounds.GetSize().Equals(ExpectedSize, 100.0));
    TestTrue(TEXT("City includes ground and building elevations"), CityBounds.GetSize().Z > 2000);
    FHitResult Ground;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CityGroundTest), true);
    TestTrue(TEXT("Drone launch has authored ground collision"),
             World->LineTraceSingleByObjectType(
                 Ground, FVector(-5900, 202, 200), FVector(-5900, 202, -300),
                 FCollisionObjectQueryParams(ECC_WorldStatic), Query));
    AddInfo(FString::Printf(TEXT("City bounds: %s; launch surface Z %.1f cm"),
                            *CityBounds.ToString(), Ground.ImpactPoint.Z));

    // Optional diagnostics sample the actual game-world collision, rather than
    // an editor world whose physics scene may not be initialized in a commandlet.
    if (FParse::Param(FCommandLine::Get(), TEXT("ExportCityRoadGrid")))
    {
        FString Samples = TEXT("x_cm,y_cm,z_cm\n");
        for (int32 Y = -8000; Y <= 6000; Y += 100)
        {
            for (int32 X = -15000; X <= -5000; X += 100)
            {
                FHitResult Surface;
                float Height = -10000;
                if (World->LineTraceSingleByObjectType(
                        Surface, FVector(X, Y, 150), FVector(X, Y, -300),
                        FCollisionObjectQueryParams(ECC_WorldStatic), Query))
                {
                    Height = Surface.ImpactPoint.Z;
                }
                Samples += FString::Printf(TEXT("%d,%d,%.3f\n"), X, Y, Height);
            }
        }
        FFileHelper::SaveStringToFile(Samples,
                                      *(FPaths::ProjectSavedDir() / TEXT("CityRoadHeights.csv")));
    }

    AAetherGameMode *GameMode = World->SpawnActor<AAetherGameMode>();
    TArray<ACityVehicle *> Traffic;
    for (TActorIterator<ACityVehicle> It(World); It; ++It)
    {
        Traffic.Add(*It);
    }
    // An isolated test world does not own an authoritative game mode; load its traffic explicitly
    // if needed.
    if (Traffic.IsEmpty())
    {
        TestTrue(TEXT("Street circuit can be grounded on the city"), GameMode->SpawnCityTraffic());
        for (TActorIterator<ACityVehicle> It(World); It; ++It)
        {
            Traffic.Add(*It);
        }
    }
    TestEqual(TEXT("Four detailed vehicles are present"), Traffic.Num(), 4);
    TArray<FVector> InitialLocations;
    for (ACityVehicle *Vehicle : Traffic)
    {
        InitialLocations.Add(Vehicle->GetActorLocation());
    }
    TArray<float> Distances;
    Distances.Init(0.0F, Traffic.Num());
    // Run enough simulated time to pass every street corner, so a car that
    // moves initially but stalls against a curb cannot satisfy this regression.
    for (int32 Step = 0; Step < 6000; ++Step)
    {
        for (int32 Index = 0; Index < Traffic.Num(); ++Index)
        {
            const FVector Previous = Traffic[Index]->GetActorLocation();
            Traffic[Index]->Tick(0.02F);
            Distances[Index] += FVector::Dist2D(Previous, Traffic[Index]->GetActorLocation());
        }
    }
    for (int32 Index = 0; Index < Traffic.Num(); ++Index)
    {
        AddInfo(FString::Printf(TEXT("Car %d: start %s; traveled %.1f cm in 120 simulated seconds"),
                                Index, *InitialLocations[Index].ToString(), Distances[Index]));
        TestTrue(TEXT("Each car starts at street height"),
                 InitialLocations[Index].Z > -100 && InitialLocations[Index].Z < 300);
        TestTrue(TEXT("Each car continues through a full street circuit"),
                 Distances[Index] > 45000);
    }

    ACityVehicle *Vehicle = World->SpawnActor<ACityVehicle>();
    Vehicle->ConfigureRoute(FVector(50000, 50000, 100), FVector(55000, 50000, 100));
    const FVector InitialPosition = Vehicle->GetActorLocation();
    for (int32 Step = 0; Step < 50; ++Step)
    {
        const FVector Previous = Vehicle->GetActorLocation();
        Vehicle->Tick(0.02F);
        TestTrue(TEXT("Vehicle motion obeys the speed bound"),
                 FVector::Distance(Previous, Vehicle->GetActorLocation()) <= 10.01F);
    }
    TestTrue(TEXT("Imported car moves along its isolated route"),
             Vehicle->GetActorLocation().X > InitialPosition.X + 50);
    AStaticMeshActor *Barrier = World->SpawnActor<AStaticMeshActor>();
    auto *BarrierMesh = Barrier->GetStaticMeshComponent();
    BarrierMesh->SetMobility(EComponentMobility::Movable);
    BarrierMesh->SetStaticMesh(
        LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    BarrierMesh->SetCollisionProfileName(TEXT("BlockAll"));
    Barrier->SetActorScale3D(FVector(1, 10, 10));
    Barrier->SetActorLocation(FVector(50800, 50000, 200));
    for (int32 Step = 0; Step < 250; ++Step)
    {
        Vehicle->Tick(0.02F);
    }
    TestTrue(TEXT("Moving car stops before a blocking obstacle"),
             Vehicle->GetActorLocation().X < 50800);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
