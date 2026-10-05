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

	// transitMeshPath非空时优先加载真实静态网格——模型自己的尺寸就是对的，不再按
	// sizeX/Y/Z缩放立方体。加载失败(mod的Plugin没挂载上/路径打错)时退化回立方体占位，
	// 和留空时的默认行为一致，见vehicle_mod.h::transitMeshPath的说明。
	const std::string& meshPath = vehicle->GetTransitMeshPath();
	if (!meshPath.empty()) {
		if (UStaticMesh* mesh = LoadObject<UStaticMesh>(nullptr, UTF8_TO_TCHAR(meshPath.c_str()))) {
			bodyMesh->SetStaticMesh(mesh);
			float meshScale = vehicle->GetMeshScale();
			bodyMesh->SetWorldScale3D(FVector(meshScale, meshScale, meshScale));
			// 不能在这里直接SetRelativeRotation——bodyMesh是RootComponent，Tick()里
			// SetActorLocationAndRotation直接设的就是RootComponent的世界旋转，这里设的
			// 任何值都会被下一次Tick覆盖掉。缓存这个偏移，叠进Tick()每帧算的朝向里。
			meshYawOffsetDegrees = vehicle->GetMeshYawOffsetDegrees();
			return;
		}
	}

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
	// ComputeWorldPosition同一套换算约定。叠加meshYawOffsetDegrees修正模型自己的正前方
	// 和行驶方向没对齐的问题，见Init()里的说明。
	SetActorLocationAndRotation(
		FVector(x * TRANSIT_WORLD_SCALE, y * TRANSIT_WORLD_SCALE, z * TRANSIT_WORLD_SCALE),
		FRotator(0.f, yaw + meshYawOffsetDegrees, 0.f));
}
