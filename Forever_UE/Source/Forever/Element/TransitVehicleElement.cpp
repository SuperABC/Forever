#include "Element/TransitVehicleElement.h"

#include "traffic/vehicle.h"

#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

// 和ForeverTrafficFrameworkComponent.cpp的TRAFFIC_WORLD_SCALE同一个值，这次按本文件既有
// 约定各自维护一份(项目既有约定，见ForeverTrafficFrameworkComponent.cpp顶部注释)。
#define TRANSIT_WORLD_SCALE 1000.f

// Cube.Cube默认边长100cm，缩放系数=目标尺寸/100。
#define TRANSIT_CUBE_DEFAULT_SIZE 100.f

ATransitVehicleElement::ATransitVehicleElement()
{
	PrimaryActorTick.bCanEverTick = true;

	bodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	RootComponent = bodyMesh;

	// 纯展示物，不参与任何碰撞查询——这次不做操控/搭乘，不需要挡住别的东西，也不需要被
	// 别的东西的重叠检测测到，见VehicleMod::drivable/boardable的说明。
	bodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	bodyMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> cubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (cubeFinder.Succeeded()) {
		bodyMesh->SetStaticMesh(cubeFinder.Object);
	}
}

void ATransitVehicleElement::Init(Vehicle* inVehicle)
{
	vehicle = inVehicle;
	if (!vehicle || !bodyMesh) return;

	float sizeX, sizeY, sizeZ;
	vehicle->GetSize(sizeX, sizeY, sizeZ);
	bodyMesh->SetWorldScale3D(FVector(
		sizeX / TRANSIT_CUBE_DEFAULT_SIZE,
		sizeY / TRANSIT_CUBE_DEFAULT_SIZE,
		sizeZ / TRANSIT_CUBE_DEFAULT_SIZE));
}

void ATransitVehicleElement::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Vehicle所有权在Traffic身上，这里不delete，和ACitizenElement/AVehicleElement同一套
	// 安全原则。
	vehicle = nullptr;
	Super::EndPlay(EndPlayReason);
}

void ATransitVehicleElement::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!vehicle) return;

	float x, y, z, yaw;
	vehicle->GetTransform(x, y, z, yaw);
	// Route::Update()直接用地图单位写transform(不像AVehicleElement::Tick那样反过来从UE坐标
	// 换算回去)，这里要乘WORLD_SCALE换成UE单位，和ForeverTrafficFrameworkComponent.cpp里
	// ComputeWorldPosition同一套换算约定。
	SetActorLocationAndRotation(
		FVector(x * TRANSIT_WORLD_SCALE, y * TRANSIT_WORLD_SCALE, z * TRANSIT_WORLD_SCALE),
		FRotator(0.f, yaw, 0.f));
}
