// Drives an externally authored car along authored street routes with swept collision.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CityVehicle.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USplineComponent;

UCLASS(Config = Game)

class AERIALWORLDLAB_API ACityVehicle : public AActor
{
    GENERATED_BODY()
  public:
    ACityVehicle();
    // Straight-route compatibility is retained for repeatable collision experiments.
    void ConfigureRoute(const FVector &StartCentimeters, const FVector &EndCentimeters);
    void ConfigureStreetCircuit(const TArray<FVector> &StreetPointsCentimeters,
                                float InitialDistanceCentimeters);
    virtual void Tick(float DeltaSeconds) override;

  private:
    bool FitAuthoredVehicle();
    void TickStreetCircuit(float StepSeconds);
    void TickStraightRoute(float StepSeconds);
    bool SampleCityGround(const FVector &Position, FHitResult &Hit) const;

    // A fitted primitive supplies stable moving collision; visual glass/interior are not colliders.
    UPROPERTY()
    TObjectPtr<UBoxComponent> CollisionBody;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY()
    TObjectPtr<USplineComponent> StreetCircuit;

    FVector RouteStart = FVector::ZeroVector;
    FVector RouteDirection = FVector::ForwardVector;
    float RouteLengthCentimeters = 0.0F;
    float ProgressCentimeters = 0.0F;
    float SpeedCentimetersPerSecond = 0.0F;
    float TravelDirection = 1.0F;
    float HalfHeightCentimeters = 70.0F;
    bool bUseStreetCircuit = false;

    UPROPERTY(EditAnywhere, Config, Category = "Traffic|Asset")

    FString VehicleMeshPath = TEXT("/Game/External/CarConcept/SurveyTrafficCar.SurveyTrafficCar");
    UPROPERTY(EditAnywhere, Config, Category = "Traffic|Dimensions")
    float VehicleLengthCentimeters = 450.0F;
    // Exclude tires from the swept envelope so rough road surfaces do not jam it.
    UPROPERTY(EditAnywhere, Config, Category = "Traffic|Dimensions")
    float BodyGroundClearanceCentimeters = 20.0F;
    UPROPERTY(EditAnywhere, Config, Category = "Traffic|Cornering")
    float MaximumLateralAccelerationMetersPerSecondSquared = 1.5F;
    UPROPERTY(EditAnywhere, Config, Category = "Traffic|Cornering")
    float MinimumLookaheadCentimeters = 400.0F;
    UPROPERTY(EditAnywhere, Config, Category = "Traffic|Cornering")
    float LookaheadBufferCentimeters = 300.0F;
    UPROPERTY(EditAnywhere, Config, Category = "Traffic|Timing")
    float MaximumStepSeconds = 0.05F;

    UPROPERTY(EditAnywhere, Config, Category = "Traffic", meta = (ClampMin = "0"))

    float CruiseSpeedMetersPerSecond = 5.0F;
    UPROPERTY(EditAnywhere, Config, Category = "Traffic", meta = (ClampMin = "0.1"))
    float AccelerationMetersPerSecondSquared = 2.0F;
};
