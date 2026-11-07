#include "Flight/AetherGameMode.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Flight/AetherHUD.h"
#include "Flight/QuadrotorPawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

AAetherGameMode::AAetherGameMode()
{
    DefaultPawnClass = AQuadrotorPawn::StaticClass();
    HUDClass = AAetherHUD::StaticClass();
}

APawn* AAetherGameMode::SpawnDefaultPawnAtTransform_Implementation(
    AController* NewPlayer,
    const FTransform& SpawnTransform)
{
    FTransform DroneSpawn = SpawnTransform;
    DroneSpawn.SetLocation(FVector(
        SpawnTransform.GetLocation().X,
        SpawnTransform.GetLocation().Y,
        FMath::Max(SpawnTransform.GetLocation().Z, 220.0F)));
    return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, DroneSpawn);
}

void AAetherGameMode::BeginPlay()
{
    Super::BeginPlay();

    const FLinearColor Asphalt(0.075F, 0.095F, 0.11F);
    const FLinearColor Concrete(0.34F, 0.38F, 0.39F);
    const FLinearColor WhiteMarking(0.78F, 0.79F, 0.72F);

    SpawnTestBox(FVector(0.0, 0.0, -25.0), FVector(100.0, 100.0, 0.5),
                 FLinearColor(0.17F, 0.21F, 0.20F));
    SpawnTestBox(FVector(0.0, 0.0, 1.0), FVector(100.0, 7.0, 0.1), Asphalt);
    SpawnTestBox(FVector(0.0, 0.0, 1.0), FVector(7.0, 100.0, 0.1), Asphalt);
    SpawnTestBox(FVector(0.0, 420.0, 2.0), FVector(100.0, 0.08, 0.015), WhiteMarking);
    SpawnTestBox(FVector(0.0, -420.0, 2.0), FVector(100.0, 0.08, 0.015), WhiteMarking);
    SpawnTestBox(FVector(420.0, 0.0, 2.0), FVector(0.08, 100.0, 0.015), WhiteMarking);
    SpawnTestBox(FVector(-420.0, 0.0, 2.0), FVector(0.08, 100.0, 0.015), WhiteMarking);

    for (int32 Segment = -8; Segment <= 8; ++Segment)
    {
        if (Segment == 0)
        {
            continue;
        }
        const float Offset = Segment * 550.0F;
        SpawnTestBox(FVector(Offset, 0.0, 2.1), FVector(2.2, 0.08, 0.01), WhiteMarking);
        SpawnTestBox(FVector(0.0, Offset, 2.1), FVector(0.08, 2.2, 0.01), WhiteMarking);
    }

    SpawnTestBox(FVector(0.0, 700.0, 14.0), FVector(100.0, 0.8, 0.28), Concrete);
    SpawnTestBox(FVector(0.0, -700.0, 14.0), FVector(100.0, 0.8, 0.28), Concrete);
    SpawnTestBox(FVector(700.0, 0.0, 14.0), FVector(0.8, 100.0, 0.28), Concrete);
    SpawnTestBox(FVector(-700.0, 0.0, 14.0), FVector(0.8, 100.0, 0.28), Concrete);

    SpawnBuilding(FVector(-1700.0, -1700.0, 0.0), FVector(7.0, 6.0, 15.0),
                  FLinearColor(0.28F, 0.34F, 0.39F), 7, 4);
    SpawnBuilding(FVector(1700.0, -1700.0, 0.0), FVector(8.0, 6.0, 19.0),
                  FLinearColor(0.37F, 0.34F, 0.30F), 9, 5);
    SpawnBuilding(FVector(-1700.0, 1700.0, 0.0), FVector(6.0, 8.0, 13.0),
                  FLinearColor(0.32F, 0.38F, 0.37F), 6, 4);
    SpawnBuilding(FVector(1700.0, 1700.0, 0.0), FVector(8.0, 7.0, 17.0),
                  FLinearColor(0.31F, 0.33F, 0.40F), 8, 5);
    SpawnBuilding(FVector(-3500.0, -1100.0, 0.0), FVector(10.0, 8.0, 23.0),
                  FLinearColor(0.27F, 0.34F, 0.42F), 11, 6);
    SpawnBuilding(FVector(3500.0, -1100.0, 0.0), FVector(9.0, 8.0, 20.0),
                  FLinearColor(0.40F, 0.35F, 0.30F), 10, 5);
    SpawnBuilding(FVector(-3500.0, 1900.0, 0.0), FVector(9.0, 8.0, 18.0),
                  FLinearColor(0.31F, 0.39F, 0.40F), 9, 5);
    SpawnBuilding(FVector(3500.0, 1900.0, 0.0), FVector(10.0, 7.0, 22.0),
                  FLinearColor(0.35F, 0.34F, 0.41F), 11, 6);

    SpawnTestBox(FVector(-2300.0, 2600.0, 180.0), FVector(1.8, 1.8, 3.6),
                 FLinearColor(0.78F, 0.24F, 0.08F));
    SpawnTestBox(FVector(2400.0, -2600.0, 130.0), FVector(1.2, 1.2, 2.6),
                 FLinearColor(0.78F, 0.48F, 0.08F));
}

void AAetherGameMode::SpawnBuilding(
    const FVector& Location,
    const FVector& Scale,
    const FLinearColor& FacadeColor,
    int32 Floors,
    int32 WindowColumns)
{
    AStaticMeshActor* Building = SpawnTestBox(
        Location + FVector(0.0, 0.0, Scale.Z * 50.0F),
        Scale,
        FacadeColor);
    if (!Building)
    {
        return;
    }

    UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(
        nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UMaterialInterface* ShapeMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!CubeMesh || !ShapeMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot load built-in facade mesh or material"));
        return;
    }

    UHierarchicalInstancedStaticMeshComponent* Windows =
        NewObject<UHierarchicalInstancedStaticMeshComponent>(Building, TEXT("FacadeWindows"));
    Building->AddInstanceComponent(Windows);
    Windows->SetupAttachment(Building->GetStaticMeshComponent());
    Windows->SetRelativeScale3D(FVector(1.0 / Scale.X, 1.0 / Scale.Y, 1.0 / Scale.Z));
    Windows->SetStaticMesh(CubeMesh);
    Windows->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Windows->SetMobility(EComponentMobility::Static);
    UMaterialInstanceDynamic* WindowMaterial =
        UMaterialInstanceDynamic::Create(ShapeMaterial, Windows);
    if (WindowMaterial)
    {
        WindowMaterial->SetVectorParameterValue(
            TEXT("Color"), FLinearColor(0.025F, 0.16F, 0.21F));
        Windows->SetMaterial(0, WindowMaterial);
    }
    Windows->RegisterComponent();

    const float HalfWidth = Scale.X * 50.0F;
    const float HalfDepth = Scale.Y * 50.0F;
    const float BuildingHeight = Scale.Z * 100.0F;
    const float FloorHeight = BuildingHeight / FMath::Max(Floors, 1);
    const FVector WindowScale(0.42F, 0.035F, FMath::Min(0.85F, FloorHeight / 100.0F * 0.62F));
    const auto AddWindow = [Windows, &WindowScale](
        const FVector& Position,
        float Yaw)
    {
        Windows->AddInstance(FTransform(FRotator(0.0, Yaw, 0.0), Position, WindowScale));
    };

    for (int32 Floor = 0; Floor < Floors; ++Floor)
    {
        const float Z = -BuildingHeight * 0.5F + (Floor + 0.5F) * FloorHeight;
        for (int32 Column = 0; Column < WindowColumns; ++Column)
        {
            const float X = -HalfWidth +
                (Column + 0.5F) * (2.0F * HalfWidth / WindowColumns);
            AddWindow(FVector(X, -HalfDepth - 1.0F, Z), 0.0F);
            AddWindow(FVector(X, HalfDepth + 1.0F, Z), 180.0F);

            const float Y = -HalfDepth +
                (Column + 0.5F) * (2.0F * HalfDepth / WindowColumns);
            AddWindow(FVector(-HalfWidth - 1.0F, Y, Z), 90.0F);
            AddWindow(FVector(HalfWidth + 1.0F, Y, Z), -90.0F);
        }
    }

    SpawnTestBox(
        Location + FVector(0.0, 0.0, BuildingHeight + 20.0F),
        FVector(Scale.X * 0.35F, Scale.Y * 0.35F, 0.4F),
        FLinearColor(0.16F, 0.19F, 0.21F));
}

AStaticMeshActor* AAetherGameMode::SpawnTestBox(
    const FVector& Location,
    const FVector& Scale,
    const FLinearColor& Color)
{
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AStaticMeshActor* Box = GetWorld()->SpawnActor<AStaticMeshActor>(
        AStaticMeshActor::StaticClass(), Location, FRotator::ZeroRotator, SpawnParameters);
    if (!Box)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to spawn test environment geometry"));
        return nullptr;
    }

    UStaticMeshComponent* MeshComponent = Box->GetStaticMeshComponent();
    UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(
        nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!CubeMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("Unreal basic cube mesh could not be loaded"));
        Box->Destroy();
        return nullptr;
    }

    MeshComponent->SetStaticMesh(CubeMesh);
    MeshComponent->SetMobility(EComponentMobility::Static);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
    MeshComponent->SetWorldScale3D(Scale);
    UMaterialInterface* ShapeMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (ShapeMaterial)
    {
        UMaterialInstanceDynamic* DynamicMaterial =
            UMaterialInstanceDynamic::Create(ShapeMaterial, MeshComponent);
        if (DynamicMaterial)
        {
            DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
            MeshComponent->SetMaterial(0, DynamicMaterial);
        }
    }
    return Box;
}
