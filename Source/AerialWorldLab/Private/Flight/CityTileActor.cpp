#include "Flight/CityTileActor.h"

#include "Components/StaticMeshComponent.h"

ACityTileActor::ACityTileActor()
{
    PrimaryActorTick.bCanEverTick = false;
    // Drone flight and spline traffic use direct collision queries. They do not
    // need Unreal's navigation octree to copy millions of city triangles.
    GetStaticMeshComponent()->SetCanEverAffectNavigation(false);
}
