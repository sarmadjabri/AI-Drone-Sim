#include "Flight/CityVehicle.h"
#include "Flight/SimulationUnits.h"
#include "Flight/CitySimulationSettings.h"

#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"

ACityVehicle::ACityVehicle()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    CollisionBody = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleCollision"));
    SetRootComponent(CollisionBody);
    CollisionBody->SetBoxExtent(FVector(225, 95, 70));
    CollisionBody->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AuthoredVehicle"));
    Mesh->SetupAttachment(CollisionBody);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    StreetCircuit = CreateDefaultSubobject<USplineComponent>(TEXT("AuthoredStreetCircuit"));
    StreetCircuit->SetupAttachment(CollisionBody);
    // The route is fixed in the city, even while its owning vehicle moves and rotates.
    StreetCircuit->SetAbsolute(true, true, true);
    StreetCircuit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool ACityVehicle::FitAuthoredVehicle()
{
    if (!Mesh->GetStaticMesh())
    {
        // Resolve only when a vehicle is configured, after engine/import modules
        // have initialized. A clean checkout can build before assets are imported.
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *VehicleMeshPath));
    }
    if (!Mesh->GetStaticMesh())
    {
        SetActorTickEnabled(false);
        return false;
    }
    const FBox Bounds = Mesh->GetStaticMesh()->GetBoundingBox();
    const FVector Size = Bounds.GetSize();
    const float Length = FMath::Max(Size.X, Size.Y);
    if (Length <= 0 || Size.Z <= 0)
    {
        return false;
    }
    const float Scale = VehicleLengthCentimeters / Length;
    FRotator VisualRotation = FRotator::ZeroRotator;
    if (Size.Y > Size.X)
    {
        VisualRotation.Yaw = -90;
    }
    HalfHeightCentimeters =
        FMath::Max(10.0F, (Size.Z * Scale - BodyGroundClearanceCentimeters) * 0.5F);
    const float HalfWidth = FMath::Min(Size.X, Size.Y) * Scale * 0.5F;
    CollisionBody->SetBoxExtent(
        FVector(VehicleLengthCentimeters * 0.5F, HalfWidth, HalfHeightCentimeters));
    Mesh->SetRelativeRotation(VisualRotation);
    Mesh->SetRelativeScale3D(FVector(Scale));
    // Put the authored model's lower surface at the bottom of the collision body.
    FVector VisualOffset = Bounds.GetCenter() * Scale;
    VisualOffset.Z = Bounds.Min.Z * Scale;
    Mesh->SetRelativeLocation(
        -VisualRotation.RotateVector(VisualOffset) -
        FVector(0, 0, HalfHeightCentimeters + BodyGroundClearanceCentimeters));
    return true;
}

bool ACityVehicle::SampleCityGround(const FVector &Position, FHitResult &Hit) const
{
    const UCitySimulationSettings *CitySettings = GetDefault<UCitySimulationSettings>();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(TrafficCityGround), true, this);
    // Sample close to the spline's expected street grade. A full-height ray
    // would make traffic climb overhead tree canopies or roofs.
    // Ignore other moving cars: only the static city geometry determines road height.
    return GetWorld()->LineTraceSingleByObjectType(
        Hit, Position + FVector(0, 0, CitySettings->StreetProbeAboveCentimeters),
        Position - FVector(0, 0, CitySettings->StreetProbeBelowCentimeters),
        FCollisionObjectQueryParams(ECC_WorldStatic), Query);
}

void ACityVehicle::ConfigureStreetCircuit(const TArray<FVector> &StreetPointsCentimeters,
                                          float InitialDistanceCentimeters)
{
    if (StreetPointsCentimeters.Num() < 4 || !FitAuthoredVehicle())
    {
        SetActorTickEnabled(false);
        return;
    }
    StreetCircuit->SetSplinePoints(StreetPointsCentimeters, ESplineCoordinateSpace::World, false);
    StreetCircuit->SetClosedLoop(true, false);
    for (int32 Index = 0; Index < StreetPointsCentimeters.Num(); ++Index)
    {
        StreetCircuit->SetSplinePointType(Index, ESplinePointType::CurveClamped, false);
    }
    StreetCircuit->UpdateSpline();
    RouteLengthCentimeters = StreetCircuit->GetSplineLength();
    ProgressCentimeters =
        FMath::Fmod(InitialDistanceCentimeters, FMath::Max(1.0F, RouteLengthCentimeters));
    bUseStreetCircuit = true;
    FVector Location = StreetCircuit->GetLocationAtDistanceAlongSpline(
        ProgressCentimeters, ESplineCoordinateSpace::World);
    FHitResult Ground;
    if (SampleCityGround(Location, Ground))
    {
        Location.Z = Ground.ImpactPoint.Z;
    }
    SetActorLocation(Location +
                     FVector(0, 0, HalfHeightCentimeters + BodyGroundClearanceCentimeters));
    SetActorRotation(
        StreetCircuit
            ->GetDirectionAtDistanceAlongSpline(ProgressCentimeters, ESplineCoordinateSpace::World)
            .Rotation());
}

void ACityVehicle::ConfigureRoute(const FVector &StartCentimeters, const FVector &EndCentimeters)
{
    if (!FitAuthoredVehicle())
    {
        return;
    }
    bUseStreetCircuit = false;
    RouteDirection = (EndCentimeters - StartCentimeters).GetSafeNormal();
    RouteLengthCentimeters = FVector::Distance(StartCentimeters, EndCentimeters);
    RouteStart = StartCentimeters + FVector(0, 0, HalfHeightCentimeters);
    SetActorLocation(RouteStart);
    SetActorRotation(RouteDirection.Rotation());
}

void ACityVehicle::TickStreetCircuit(float StepSeconds)
{
    const float Acceleration =
        AccelerationMetersPerSecondSquared * Aether::Units::CentimetersPerMeter;
    const FVector Direction = StreetCircuit->GetDirectionAtDistanceAlongSpline(
        ProgressCentimeters, ESplineCoordinateSpace::World);
    // Look ahead by at least the stopping distance before asking for a tight turn.
    const float StoppingDistanceCentimeters =
        SpeedCentimetersPerSecond * SpeedCentimetersPerSecond / (2 * Acceleration);
    const float Lookahead = FMath::Max(MinimumLookaheadCentimeters,
                                       StoppingDistanceCentimeters + LookaheadBufferCentimeters);
    const float AheadDistance =
        FMath::Fmod(ProgressCentimeters + Lookahead, RouteLengthCentimeters);
    const FVector AheadDirection = StreetCircuit->GetDirectionAtDistanceAlongSpline(
        AheadDistance, ESplineCoordinateSpace::World);
    const float Angle =
        FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction, AheadDirection), -1.0, 1.0));
    float TargetSpeed = CruiseSpeedMetersPerSecond * Aether::Units::CentimetersPerMeter;
    if (Angle > 0.001F)
    {
        // a_lateral = v^2 * curvature; cap requested cornering acceleration at 1.5 m/s^2.
        TargetSpeed = FMath::Min(
            TargetSpeed, FMath::Sqrt(MaximumLateralAccelerationMetersPerSecondSquared *
                                     Aether::Units::CentimetersPerMeter * Lookahead / Angle));
    }
    SpeedCentimetersPerSecond =
        FMath::FInterpConstantTo(SpeedCentimetersPerSecond, TargetSpeed, StepSeconds, Acceleration);
    const float NextDistance = FMath::Fmod(
        ProgressCentimeters + SpeedCentimetersPerSecond * StepSeconds, RouteLengthCentimeters);
    FVector Location = StreetCircuit->GetLocationAtDistanceAlongSpline(
        NextDistance, ESplineCoordinateSpace::World);
    FHitResult Ground;
    if (!SampleCityGround(Location, Ground))
    {
        SpeedCentimetersPerSecond = 0;
        return; // Never drive across an absent city surface or float across a data gap.
    }
    Location.Z = Ground.ImpactPoint.Z + HalfHeightCentimeters + BodyGroundClearanceCentimeters;
    const FVector PreviousLocation = GetActorLocation();
    FHitResult Collision;
    SetActorLocation(Location, true, &Collision);
    if (Collision.bBlockingHit)
    {
        ProgressCentimeters =
            FMath::Fmod(ProgressCentimeters + FVector::Dist2D(PreviousLocation, GetActorLocation()),
                        RouteLengthCentimeters);
        SpeedCentimetersPerSecond = 0;
        return;
    }
    ProgressCentimeters = NextDistance;
    const FVector Heading = StreetCircuit->GetDirectionAtDistanceAlongSpline(
        ProgressCentimeters, ESplineCoordinateSpace::World);
    SetActorRotation(FRotationMatrix::MakeFromXZ(Heading, Ground.ImpactNormal).Rotator());
}

void ACityVehicle::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (DeltaSeconds <= 0 || RouteLengthCentimeters <= 0)
    {
        return;
    }
    const float StepSeconds = FMath::Min(DeltaSeconds, MaximumStepSeconds);
    if (bUseStreetCircuit)
    {
        TickStreetCircuit(StepSeconds);
        return;
    }
    TickStraightRoute(StepSeconds);
}

void ACityVehicle::TickStraightRoute(float StepSeconds)
{
    float Remaining = RouteLengthCentimeters - ProgressCentimeters;
    if (TravelDirection < 0)
    {
        Remaining = ProgressCentimeters;
    }
    const float Acceleration =
        AccelerationMetersPerSecondSquared * Aether::Units::CentimetersPerMeter;
    const float SafeSpeed = FMath::Sqrt(2 * Acceleration * FMath::Max(0.0F, Remaining));
    const float TargetSpeed =
        FMath::Min(CruiseSpeedMetersPerSecond * Aether::Units::CentimetersPerMeter, SafeSpeed);
    SpeedCentimetersPerSecond =
        FMath::FInterpConstantTo(SpeedCentimetersPerSecond, TargetSpeed, StepSeconds, Acceleration);
    const float NextProgress = FMath::Clamp(
        ProgressCentimeters + TravelDirection * SpeedCentimetersPerSecond * StepSeconds, 0.0F,
        RouteLengthCentimeters);
    FHitResult Collision;
    SetActorLocation(RouteStart + RouteDirection * NextProgress, true, &Collision);
    ProgressCentimeters = FVector::DotProduct(GetActorLocation() - RouteStart, RouteDirection);
    if (Collision.bBlockingHit)
    {
        SpeedCentimetersPerSecond = 0;
        return;
    }
    if (Remaining < 5 && SpeedCentimetersPerSecond < 50)
    {
        SpeedCentimetersPerSecond = 0;
        TravelDirection = -TravelDirection;
    }
}
