#include "Framework/ForeverTrafficFrameworkComponent.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverStoryFrameworkComponent.h"
#include "Element/VehicleElement.h"
#include "Element/TransitVehicleElement.h"
#include "Element/CitizenElement.h"
#include "Controller/BusController.h"
#include "Controller/VehicleCommandController.h"
#include "traffic/junction.h"
#include "Framework/ForeverTerrainFrameworkComponent.h"
#include "traffic/traffic.h"
#include "traffic/vehicle.h"
#include "traffic/route.h"
#include "traffic/station.h"
#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "map/roadnet.h"
#include "common/utility.h"
#include "story/event.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "player/player.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

using namespace std;

// 和BuildingElement.cpp同名常量/同一个公式：每个需要room-local→世界坐标换算的.cpp各自维护一份，不抽公共函数。
#define TRAFFIC_WORLD_SCALE 1000.f

// 车辆流式生成/销毁判断距离时Z方向的权重，和市民(POPULACE_VERTICAL_DISTANCE_WEIGHT)同一个值。
#define TRAFFIC_VERTICAL_DISTANCE_WEIGHT 5.f

// 载具骨骼网格是bSimulatePhysics=true的刚体(见VehicleElement.cpp构造函数)，对生成时的穿插极其敏感：
// 哪怕嵌进地板1个单位，Chaos也会用很大的分离冲量把车弹飞。生成点要离地板留余量让车靠重力落地，
// 50个单位(0.5米)是比车轮半径大的保守值。
#define TRAFFIC_HEIGHT_EPSILON 50.f

// 公交/火车/飞机线路调试画线用的尺寸(UE单位)，纯debug标记，取值同
// ForeverRoadnetFrameworkComponent.cpp导航图debug可视化(NAV_DEBUG_*)。
#define ROUTE_DEBUG_EDGE_HALF_WIDTH 8.f
#define ROUTE_DEBUG_NODE_HALF_SIZE 40.f
#define ROUTE_DEBUG_HEIGHT 50.f
#define ROUTE_DEBUG_ARROW_LENGTH 120.f
#define ROUTE_DEBUG_ARROW_HALF_WIDTH 40.f
// 每隔多少地图单位采一个样，至少16段。
#define ROUTE_DEBUG_SAMPLE_STEP 0.25f
#define ROUTE_DEBUG_MIN_STEPS 16

namespace {
	float TrafficStreamingDistance(const FVector& a, const FVector& b) {
		FVector delta = a - b;
		delta.Z *= TRAFFIC_VERTICAL_DISTANCE_WEIGHT;
		return delta.Size();
	}

	// 带Traffic前缀：BuildingElement.cpp也有一个同签名的匿名命名空间函数，Unity构建把两个文件
	// 合进同一个编译单元时重名会冲突。
	void TrafficComputeWorldPosition(const Building& building, float localX, float localY,
		float& outWorldX, float& outWorldY) {
		float relX = localX - building.GetBodySizeX() * 0.5f + building.GetBodyOffsetX();
		float relY = localY - building.GetBodySizeY() * 0.5f + building.GetBodyOffsetY();
		float rot = building.GetRotation();
		float c = FMath::Cos(rot), s = FMath::Sin(rot);
		outWorldX = (building.GetPosX() + relX * c - relY * s) * TRAFFIC_WORLD_SCALE;
		outWorldY = (building.GetPosY() + relX * s + relY * c) * TRAFFIC_WORLD_SCALE;
	}

	// 下车点能不能用：既要终点站得下人，也要车到终点这一路没有东西挡着(车贴墙时下车点可能在墙另一侧)。
	// 从车身中心sweep到候选点。坑：不能用Overlap类查询——建筑/房间/园区/市民/车辆的边界盒都是Trigger档案(对所有通道Overlap)，
	// 重叠查询会把它们误判成挡路；扫掠只认Block，真正挡人的墙体/地板/车身(Vehicle档案对Pawn是Block)才会命中。
	bool IsExitPathBlocked(UWorld* world, const FVector& from, const FVector& to, const FCollisionQueryParams& queryParams, FHitResult& outHit) {
		return world->SweepSingleByChannel(outHit,
			from, to, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(42.f, 96.f), queryParams);
	}

	// 双面四边形，同ForeverRoadnetFrameworkComponent.cpp的RoadnetAppendQuadDoubleSided(各.cpp各自维护一份)。
	void RouteAppendQuadDoubleSided(TArray<FVector>& vertices, TArray<int32>& triangles,
		const FVector& v00, const FVector& v10, const FVector& v11, const FVector& v01) {
		int32 base = vertices.Num();
		vertices.Add(v00); vertices.Add(v10); vertices.Add(v11); vertices.Add(v01);
		triangles.Add(base); triangles.Add(base + 2); triangles.Add(base + 1);
		triangles.Add(base); triangles.Add(base + 3); triangles.Add(base + 2);
		triangles.Add(base); triangles.Add(base + 1); triangles.Add(base + 2);
		triangles.Add(base); triangles.Add(base + 2); triangles.Add(base + 3);
	}

	// 站点接口用的小box，同Roadnet的AppendNavBox。
	void AppendRouteStationBox(TArray<FVector>& vertices, TArray<int32>& triangles,
		const FVector2D& center, float halfSize, float zBottom, float zTop) {
		FVector v000(center.X - halfSize, center.Y - halfSize, zBottom);
		FVector v100(center.X + halfSize, center.Y - halfSize, zBottom);
		FVector v110(center.X + halfSize, center.Y + halfSize, zBottom);
		FVector v010(center.X - halfSize, center.Y + halfSize, zBottom);
		FVector v001(center.X - halfSize, center.Y - halfSize, zTop);
		FVector v101(center.X + halfSize, center.Y - halfSize, zTop);
		FVector v111(center.X + halfSize, center.Y + halfSize, zTop);
		FVector v011(center.X - halfSize, center.Y + halfSize, zTop);

		RouteAppendQuadDoubleSided(vertices, triangles, v001, v101, v111, v011);
		RouteAppendQuadDoubleSided(vertices, triangles, v010, v110, v100, v000);
		RouteAppendQuadDoubleSided(vertices, triangles, v000, v100, v101, v001);
		RouteAppendQuadDoubleSided(vertices, triangles, v100, v110, v111, v101);
		RouteAppendQuadDoubleSided(vertices, triangles, v110, v010, v011, v111);
		RouteAppendQuadDoubleSided(vertices, triangles, v010, v000, v001, v011);
	}

	// 两点间的细ribbon，同Roadnet的AppendNavEdgeRibbon。
	void AppendRouteEdgeRibbon(TArray<FVector>& vertices, TArray<int32>& triangles,
		const FVector& from, const FVector& to, float halfWidth) {
		FVector2D dir2D(to.X - from.X, to.Y - from.Y);
		float len = dir2D.Size();
		if (len < 1e-3f) return;
		dir2D /= len;
		FVector offset(-dir2D.Y * halfWidth, dir2D.X * halfWidth, 0.f);

		RouteAppendQuadDoubleSided(vertices, triangles, from - offset, to - offset, to + offset, from + offset);
	}

	// 一条edge(多段segments拼起来)按弧长采样成世界坐标折线。edge->reversed只影响遍历方向/切线符号，
	// 不影响曲线形状，所以不处理。相邻两段共享的端点只采一次。
	void SampleRouteEdge(const Route::RouteEdge& edge, TArray<FVector>& outPoints) {
		for (Connection* seg : edge.segments) {
			if (!seg) continue;
			float segLength = seg->CalcDistance(); // 地图单位
			int steps = FMath::Max(ROUTE_DEBUG_MIN_STEPS, FMath::CeilToInt(segLength / ROUTE_DEBUG_SAMPLE_STEP));
			for (int i = 0; i <= steps; i++) {
				if (!outPoints.IsEmpty() && i == 0) continue; // 和上一段的末尾重复，跳过
				float f = static_cast<float>(i) / static_cast<float>(steps);
				Node point = seg->GetPoint(f);
				outPoints.Add(FVector(point.GetX() * TRAFFIC_WORLD_SCALE, point.GetY() * TRAFFIC_WORLD_SCALE,
					point.GetZ() * TRAFFIC_WORLD_SCALE));
			}
		}
	}

	// 在折线中点画一个小箭头指示行驶方向(reversed时指向相反)，两个相反绕序的三角形双面绘制。
	void AppendRouteArrow(TArray<FVector>& vertices, TArray<int32>& triangles,
		const TArray<FVector>& points, bool reversed) {
		if (points.Num() < 2) return;

		int midIndex = points.Num() / 2;
		FVector a = points[FMath::Max(0, midIndex - 1)];
		FVector b = points[FMath::Min(points.Num() - 1, midIndex)];
		FVector dir = (b - a);
		if (dir.SizeSquared2D() < 1e-3f) return;
		dir = dir.GetSafeNormal2D();
		if (reversed) dir = -dir;

		FVector mid = points[midIndex];
		FVector perp(-dir.Y, dir.X, 0.f);

		FVector tip = mid + dir * (ROUTE_DEBUG_ARROW_LENGTH * 0.5f);
		FVector backLeft = mid - dir * (ROUTE_DEBUG_ARROW_LENGTH * 0.5f) + perp * ROUTE_DEBUG_ARROW_HALF_WIDTH;
		FVector backRight = mid - dir * (ROUTE_DEBUG_ARROW_LENGTH * 0.5f) - perp * ROUTE_DEBUG_ARROW_HALF_WIDTH;

		int32 base = vertices.Num();
		vertices.Add(tip); vertices.Add(backLeft); vertices.Add(backRight);
		triangles.Add(base); triangles.Add(base + 1); triangles.Add(base + 2);
		triangles.Add(base); triangles.Add(base + 2); triangles.Add(base + 1);
	}
}

UForeverTrafficFrameworkComponent::UForeverTrafficFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> pureFinder(
		TEXT("/Game/Asset/Common/Material/PureColor.PureColor"));
	if (pureFinder.Succeeded()) {
		pureBaseMaterial = pureFinder.Object;
	}
}

void UForeverTrafficFrameworkComponent::GenerateVehicles(Map* map, Traffic* traffic) {
	if (!map || !traffic) return;
	mapRef = map;
	trafficRef = traffic;
	junctionTraffic.Reset(); // 换了地图：路口表按新Map重建
	busStates.Empty();
	stationByNodeId.Empty();
	// pendingNavigations不清：game_start（车辆脚本的vehicle_navigate）比GenerateVehicles先发生，请求要留到车辆登记好之后处理。
	pendingNavResults.Empty();
	carDrivers.Empty();
	navTargets.Empty();
	vehicleRoomCache.Empty();
	reachableCache.Empty();
	junctionCenters.Empty();
	bJunctionCentersBuilt = false;

	// 辅助导航节点 id -> 车站：Controller 进出站时按节点 id 反查是哪个站，发 enter_stop/leave_stop。
	for (const auto& [routeName, route] : traffic->GetRoutes()) {
		if (!route) continue;
		for (const vector<Route::RouteLeg>& legs : route->GetLines()) {
			for (const Route::RouteLeg& leg : legs) {
				const StationRoadLink* link = leg.fromStation ? leg.fromStation->GetRoadLink(leg.fromInterfaceIndex) : nullptr;
				if (link && link->stationNode) stationByNodeId.Add(link->stationNode->GetId(), leg.fromStation);
			}
		}
	}

	for (auto& [name, vehicle] : traffic->GetVehicles()) {
		if (!vehicle) continue;

		if (vehicle->GetRoute()) {
			// 公共交通车辆：不需要换算世界坐标，Tick里直接读vehicle->GetTransform()驱动，生成时原点占位。
			UClass* transitClass = ATransitVehicleElement::StaticClass();
			const string& blueprintPath = vehicle->GetBlueprintPath();
			UClass* loaded = nullptr; // 蓝图只加载这一次，transit类与公交物理表示的判断共用
			if (!blueprintPath.empty()) {
				FString path = UTF8_TO_TCHAR(blueprintPath.data());
				// 公交同时带了给停车场驾驶用的BP(继承AVehicleElement)：加载出来不是
				// ATransitVehicleElement的子类就忽略，路线车仍用默认的ATransitVehicleElement。
				loaded = LoadClass<APawn>(nullptr, *path);
				if (loaded && loaded->IsChildOf(ATransitVehicleElement::StaticClass())) {
					transitClass = loaded;
				}
			}

			ATransitVehicleElement* transitElement = GetWorld()->SpawnActor<ATransitVehicleElement>(
				transitClass, FVector::ZeroVector, FRotator::ZeroRotator);
			if (!transitElement) continue;

			transitElement->Init(vehicle);
			FString transitName = UTF8_TO_TCHAR(vehicle->GetName().c_str());
			activeTransitVehicles.Add(transitName, transitElement);

			// 公交：蓝图是 AVehicleElement 子类的话，玩家附近（地形 LOD 范围内）换成它的物理表示+Controller。
			if (vehicle->GetRoute()->GetStationType() == "bus" && !blueprintPath.empty()) {
				UClass* physicsClass = (loaded && loaded->IsChildOf(AVehicleElement::StaticClass())) ? loaded : nullptr;
				if (physicsClass) busPhysicsClasses.Add(physicsClass);
				FBusState state;
				state.vehicle = vehicle;
				state.name = transitName;
				state.bRouteBus = true;
				state.physicsClass = physicsClass;
				state.bNoPhysics = (physicsClass == nullptr);
				state.kinematic = transitElement;
				busStates.Add(transitName, state);
			}
			continue;
		}

		Room* room = vehicle->GetRoom();
		Building* building = room ? room->GetParentBuilding() : nullptr;
		if (!room || !building) continue; // 不是预置在停车位里的车辆(防御性写法)

		float localX, localY;
		vehicle->GetParkingLocalPosition(localX, localY);

		float worldX, worldY;
		TrafficComputeWorldPosition(*building, room->GetPosX() + localX, room->GetPosY() + localY, worldX, worldY);
		float worldZ = TRAFFIC_HEIGHT_EPSILON + building->GetFloorBaseZ(room->GetLayer()) * TRAFFIC_WORLD_SCALE;
		float worldYaw = FMath::RadiansToDegrees(building->GetRotation()) + vehicle->GetParkingRotationDegrees();

		// 车辆Actor每帧把自己的位置写回Vehicle::SetTransform(UE单位)。读档时这份记录非零就说明
		// 车被开动过(或至少生成过一次)，按记录的位置生成，不再放回车位。
		float savedX, savedY, savedZ, savedYaw;
		vehicle->GetTransform(savedX, savedY, savedZ, savedYaw);
		if (savedX != 0.f || savedY != 0.f || savedZ != 0.f || savedYaw != 0.f) {
			worldX = savedX; worldY = savedY; worldZ = savedZ; worldYaw = savedYaw;
		}

		// 只登记，不生成：TickComponent按玩家距离流式生成/销毁。
		FCarEntry entry;
		entry.vehicle = vehicle;
		entry.name = UTF8_TO_TCHAR(vehicle->GetName().c_str());
		entry.blueprintPath = UTF8_TO_TCHAR(vehicle->GetBlueprintPath().data());
		entry.location = FVector(worldX, worldY, worldZ);
		entry.yaw = worldYaw;
		carEntries.Add(entry);
	}
}

bool UForeverTrafficFrameworkComponent::IsNearJunction(const FVector& location) {
	if (minSpawnDistanceFromJunction <= 0.f || !mapRef) return false;
	if (!bJunctionCentersBuilt) {
		bJunctionCentersBuilt = true;
		for (RoadJunction* junction : mapRef->GetJunctions()) {
			Intersection* center = junction ? junction->GetNode() : nullptr;
			if (center) junctionCenters.Add(FVector2D(center->GetX() * TRAFFIC_WORLD_SCALE, center->GetY() * TRAFFIC_WORLD_SCALE));
		}
	}
	const float limit = minSpawnDistanceFromJunction * TRAFFIC_WORLD_SCALE;
	const float limitSq = limit * limit;
	for (const FVector2D& center : junctionCenters) {
		if (FVector2D::DistSquared(center, FVector2D(location.X, location.Y)) < limitSq) return true;
	}
	return false;
}

AVehicleElement* UForeverTrafficFrameworkComponent::SpawnCar(FCarEntry& entry, bool bCheckSpace) {
	UClass* vehicleClass = LoadClass<AVehicleElement>(nullptr, *entry.blueprintPath);
	if (!vehicleClass) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::SpawnCar: 加载车辆蓝图失败，路径=%s，车辆=%s"),
			*entry.blueprintPath, *entry.name);
		entry.bBroken = true;
		return nullptr;
	}

	if (bCheckSpace) {
		// 行进中的车（Core推算中）路口附近不生成，等它开出去；停着的车不限制。
		if (entry.vehicle && entry.vehicle->IsDriving() && IsNearJunction(entry.location)) return nullptr;
		// 生成点和别的车（Vehicle通道：物理车、Controller驱动的车）的车身重叠就先不生成，免得叠在一起。
		// 车身尺寸取蓝图默认对象的carMesh包围盒（略微缩小，贴着停的车不算占用）。
		const AVehicleElement* defaults = vehicleClass->GetDefaultObject<AVehicleElement>();
		const USkeletalMeshComponent* defaultMesh = defaults ? defaults->GetCarMesh() : nullptr;
		FVector extent = defaultMesh ? defaultMesh->CalcBounds(FTransform::Identity).BoxExtent : FVector(250.f, 110.f, 90.f);
		extent = FVector(FMath::Max(extent.X, 150.f), FMath::Max(extent.Y, 80.f), FMath::Max(extent.Z, 60.f));
		const FQuat rotation = FRotator(0.f, entry.yaw, 0.f).Quaternion();
		const FVector center = entry.location + FVector(0.f, 0.f, extent.Z + 20.f);
		FCollisionQueryParams queryParams(SCENE_QUERY_STAT(VehicleSpawnSpace), false);
		if (GetWorld()->OverlapAnyTestByObjectType(center, rotation, FCollisionObjectQueryParams(ECC_Vehicle),
			FCollisionShape::MakeBox(extent * 0.85f), queryParams)) {
			return nullptr;
		}
	}

	// carMesh有真实碰撞，停车位离墙/地板很近，默认的生成时碰撞检测会因"生成点被占用"拒绝生成
	// (日志报"SpawnActor failed because of collision at the spawn location")，所以用AlwaysSpawn跳过。
	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AVehicleElement* vehicleElement = GetWorld()->SpawnActor<AVehicleElement>(
		vehicleClass, entry.location, FRotator(0.f, entry.yaw, 0.f), spawnParams);
	if (!vehicleElement) return nullptr;

	vehicleElement->Init(entry.vehicle, nullptr); // 这时候还没人上车，previousPawn先留空
	activeVehicles.Add(entry.name, vehicleElement);
	entry.element = vehicleElement;
	entry.spawnedAt = GetWorld()->GetTimeSeconds();
	return vehicleElement;
}

void UForeverTrafficFrameworkComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 交通时钟放在所有早返回之前：暂停时DeltaTime为0不走，卡顿的大帧夹到0.25s免得租约被一帧耗光。
	trafficClock += FMath::Min(DeltaTime, 0.25f);
	drivers.RemoveAll([](const TWeakObjectPtr<AVehicleCommandController>& driver) { return !driver.IsValid(); });

	UpdateBusPhysicsHandoff();
	ProcessNavResults();
	ProcessPendingNavigations();

	if (testNavSource) {
		testNavRefreshCountdown -= DeltaTime;
		if (testNavRefreshCountdown <= 0.f) {
			testNavRefreshCountdown = testNavRefreshInterval;
			TArray<FVector> path;
			// 拷一份再调：source里可能重新规划（RefreshGuide会换掉testNavSource，不能在自己执行时被销毁）。
			const TFunction<bool(TArray<FVector>&)> source = testNavSource;
			if (source(path) && path.Num() >= 2) ShowTestNavigationLine(path);
			else ClearTestNavigationLine(); // 任务完成/对象没了
		}
	}

	if (carEntries.Num() == 0) return;
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn) return;
	FVector playerLoc = pawn->GetActorLocation();

	float spawnDistUE = vehicleSpawnDistance * TRAFFIC_WORLD_SCALE;
	float despawnDistUE = vehicleDespawnDistance * TRAFFIC_WORLD_SCALE;
	int32 spawned = 0;
	int32 movingVehicles = 0;
	for (const FCarEntry& entry : carEntries) {
		if (AVehicleElement* active = entry.element.Get()) {
			if (!active->IsPlayerControlled() && IsDrivenByController(active)) movingVehicles++;
		}
	}

	for (FCarEntry& entry : carEntries) {
		AVehicleElement* element = entry.element.Get();
		if (!element) {
			// 没有Actor但在Core里推算行驶的车（vehicle_navigate）：按时间推进位置，走完发抵达，之后照常按位置判断要不要生成。
			if (entry.vehicle && entry.vehicle->IsDriving()) {
				Room* driveDestination = entry.vehicle->GetDriveDestination();
				const bool bRandom = randomTargets.FindRef(entry.name);
				if (entry.vehicle->AdvanceDrive(GetWorld()->GetTimeSeconds())) {
					pendingNavResults.Add({ entry.vehicle, driveDestination, true, bRandom });
					navTargets.Remove(entry.name);
				}
				float dx, dy, dz, dyaw;
				entry.vehicle->GetTransform(dx, dy, dz, dyaw);
				entry.location = FVector(dx, dy, dz);
				entry.yaw = dyaw;
			}
			if (entry.bBroken || spawned >= FMath::Max(1, maxVehicleSpawnsPerFrame)) continue;
			const bool bMoving = entry.vehicle && entry.vehicle->IsDriving();
			if (bMoving && movingVehicles >= maxMovingVehicles) continue; // 行进中车辆名额满了：继续由Core推算
			const float playerDistance = TrafficStreamingDistance(playerLoc, entry.location);
			if (bMoving && playerDistance < minSpawnDistanceFromPlayer * TRAFFIC_WORLD_SCALE) continue; // 离玩家太近：不在眼前生成
			if (playerDistance < spawnDistUE) {
				if (SpawnCar(entry, true)) {
					spawned++;
					if (bMoving) {
						HandOffDriveToActor(entry);
						movingVehicles++;
					}
				}
			}
			continue;
		}

		// 生成着：记下最新位置(销毁后下次从这里生成；Core的Vehicle::SetTransform也由Actor每帧
		// 写，存档不受影响)。正被玩家操控的车(Controller非空)不销毁。
		entry.location = element->GetActorLocation();
		entry.yaw = element->GetActorRotation().Yaw;
		// 正在自己导航的车(测试导航)、被Controller持续驱动的公交也不销毁：跑远了销毁会丢掉运行状态。
		if (!element->IsPlayerControlled() && IsDrivenByController(element)) {
			// 脚本导航中的车：走远了把剩余路径交还Core继续推算并销毁Actor（和市民一样）；
			// 交不回去（公交等）就保留。
			if (TrafficStreamingDistance(playerLoc, entry.location) > despawnDistUE &&
				GetWorld()->GetTimeSeconds() - entry.spawnedAt >= minVehicleLifetime) HandBackDriveToCore(entry);
		}
		else if (!element->IsPlayerControlled() &&
			TrafficStreamingDistance(playerLoc, entry.location) > despawnDistUE &&
			GetWorld()->GetTimeSeconds() - entry.spawnedAt >= minVehicleLifetime) {
			activeVehicles.Remove(entry.name);
			element->Destroy();
			entry.element = nullptr;
		}
	}
}

AVehicleElement* UForeverTrafficFrameworkComponent::FindOrSpawnVehicle(const FString& vehicleName) {
	if (TObjectPtr<AVehicleElement>* found = activeVehicles.Find(vehicleName)) {
		if (AVehicleElement* element = found->Get()) return element;
		activeVehicles.Remove(vehicleName);
	}
	for (FCarEntry& entry : carEntries) {
		if (entry.name == vehicleName) return SpawnCar(entry);
	}
	if (IsSwitchableRouteBus(vehicleName)) return EnsureBusPhysicsElement(vehicleName);
	return nullptr;
}

void UForeverTrafficFrameworkComponent::FireVehicleEvent(Vehicle* vehicle, bool entering, const char* mode) {
	if (!vehicle) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	UForeverStoryFrameworkComponent* storyFramework = framework ? framework->GetStoryFramework() : nullptr;
	if (!storyFramework) return;

	// 只发给被进出的这辆车自己的script，见vehicle_bus.script。
	if (entering) {
		EnterVehicleEvent event(vehicle->GetName(), mode);
		storyFramework->BroadcastToVehicle(event, vehicle);
	}
	else {
		LeaveVehicleEvent event(vehicle->GetName(), mode);
		storyFramework->BroadcastToVehicle(event, vehicle);
	}
}

void UForeverTrafficFrameworkComponent::ApplyEnterVehicle(const FString& vehicleName, APlayerController* controller, bool bSilent) {
	if (!controller) return;

	AVehicleElement* vehicleElement = FindOrSpawnVehicle(vehicleName);
	if (!vehicleElement) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::ApplyEnterVehicle: 找不到车辆 %s"), *vehicleName);
		return;
	}

	APawn* currentPawn = controller->GetPawn();
	if (!currentPawn || currentPawn == vehicleElement) return;

	// 公交 Controller 在运动学段时车没有物理，先切回物理才能开。
	if (Vehicle* driveVehicle = vehicleElement->GetVehicle()) {
		if (FBusState* busState = busStates.Find(UTF8_TO_TCHAR(driveVehicle->GetName().c_str()))) {
			if (ABusController* busController = busState->controller.Get()) busController->ForcePhysicsMode();
		}
	}

	// 玩家要自己开了：这辆车不再算"停在某个房间里"。
	if (Vehicle* coreVehicle = vehicleElement->GetVehicle()) coreVehicle->SetCurrentRoom(nullptr);

	// 隐藏后的pawn留在原地，玩家开车远离后会被Populace的距离流式销毁当成"走远的市民"Destroy掉，
	// previousPawn变悬空指针，下车时人出不来。所以上车期间标记豁免，下车时解除。
	if (ACitizenElement* citizenElement = Cast<ACitizenElement>(currentPawn)) {
		citizenElement->SetDespawnExempt(true);
	}

	// 已知小瑕疵：上车后要走远一步才能再上车(SetActorEnableCollision(false)不保证同步清掉陈旧重叠记录)。
	// 不要再试UpdateOverlaps()强制清(没用)，也不要先挪到远处再隐藏(会触发离开房间/建筑边界盒的误报提示)。
	currentPawn->SetActorHiddenInGame(true);
	currentPawn->SetActorEnableCollision(false);
	vehicleElement->SetPreviousPawn(currentPawn);
	controller->Possess(vehicleElement);
	if (!bSilent) FireVehicleEvent(vehicleElement->GetVehicle(), true, "drive");
}

ATransitVehicleElement* UForeverTrafficFrameworkComponent::FindTransitElement(const FString& vehicleName) const {
	const TObjectPtr<ATransitVehicleElement>* found = activeTransitVehicles.Find(vehicleName);
	return found ? found->Get() : nullptr;
}

void UForeverTrafficFrameworkComponent::ApplyBoardVehicle(const FString& vehicleName, APlayerController* controller, bool bSilent) {
	if (!controller) return;

	// 线路公交：搭乘的是物理公交（玩家一定在地形 LOD 范围内），公交 Controller 继续按引用开。
	if (IsSwitchableRouteBus(vehicleName)) {
		AVehicleElement* bus = EnsureBusPhysicsElement(vehicleName);
		APawn* busCurrentPawn = controller->GetPawn();
		if (!bus || !busCurrentPawn || busCurrentPawn == bus) {
			UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::ApplyBoardVehicle: 公交物理表示不可用 %s"), *vehicleName);
			return;
		}
		if (ACitizenElement* citizenElement = Cast<ACitizenElement>(busCurrentPawn)) {
			citizenElement->SetDespawnExempt(true);
		}
		busCurrentPawn->SetActorHiddenInGame(true);
		busCurrentPawn->SetActorEnableCollision(false);
		bus->SetPreviousPawn(busCurrentPawn);
		bus->SetRideMode(true);
		controller->Possess(bus);
		if (!bSilent) FireVehicleEvent(bus->GetVehicle(), true, "ride");
		return;
	}

	ATransitVehicleElement* transitElement = FindTransitElement(vehicleName);
	if (!transitElement || !transitElement->CanBoard()) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::ApplyBoardVehicle: 找不到可搭乘的载具 %s"), *vehicleName);
		return;
	}

	APawn* currentPawn = controller->GetPawn();
	if (!currentPawn || currentPawn == transitElement) return;

	// 和ApplyEnterVehicle同一套：隐藏原pawn、解除距离销毁豁免要在下车时配对做。
	if (ACitizenElement* citizenElement = Cast<ACitizenElement>(currentPawn)) {
		citizenElement->SetDespawnExempt(true);
	}
	currentPawn->SetActorHiddenInGame(true);
	currentPawn->SetActorEnableCollision(false);
	transitElement->SetPreviousPawn(currentPawn);
	controller->Possess(transitElement);
	if (!bSilent) FireVehicleEvent(transitElement->GetVehicle(), true, "ride");
}

void UForeverTrafficFrameworkComponent::ApplyLeaveVehicle(const FString& vehicleName, APlayerController* controller, bool bForce) {
	if (!controller) return;

	// 脚本指定了载具名时，只在玩家真的在这辆车里才下车(比如公交收班不能把坐在别的车里的玩家踢下来)。
	Vehicle* current = nullptr;
	if (AVehicleElement* vehicleElement = Cast<AVehicleElement>(controller->GetPawn())) current = vehicleElement->GetVehicle();
	else if (ATransitVehicleElement* transitElement = Cast<ATransitVehicleElement>(controller->GetPawn())) current = transitElement->GetVehicle();
	if (!current) return;
	if (!vehicleName.IsEmpty() && vehicleName != UTF8_TO_TCHAR(current->GetName().c_str())) return;

	ExitVehicle(controller, bForce);
}

void UForeverTrafficFrameworkComponent::ExitVehicle(APlayerController* controller, bool bForce) {
	if (!controller) return;

	// 驾驶(AVehicleElement)和搭乘(ATransitVehicleElement)共用这条下车流程：搭乘者不做遮挡判定(公交
	// 的下车点是估计值、车在站里停着)；驾驶者除非强制下车，否则要过遮挡判定。
	APawn* vehicleElement = controller->GetPawn();
	Vehicle* vehicle = nullptr;
	APawn* previousPawn = nullptr;
	bool bRide = false;
	if (AVehicleElement* driven = Cast<AVehicleElement>(vehicleElement)) {
		vehicle = driven->GetVehicle();
		previousPawn = driven->GetPreviousPawn().Get();
		bRide = driven->IsRideMode(); // 搭乘物理公交：和ATransitVehicleElement搭乘一样不做遮挡判定
	}
	else if (ATransitVehicleElement* ridden = Cast<ATransitVehicleElement>(vehicleElement)) {
		vehicle = ridden->GetVehicle();
		previousPawn = ridden->GetPreviousPawn().Get();
		bRide = true;
	}
	if (!vehicle) return;

	// 下车点：车身局部坐标系下的偏移(VehicleMod指定，默认左侧)，按车辆当前完整transform变换成候选世界坐标。
	FVector localOffset(vehicle->GetExitOffsetX(), vehicle->GetExitOffsetY(), vehicle->GetExitOffsetZ());
	FVector candidateLocation = vehicleElement->GetActorTransform().TransformPositionNoScale(localOffset);

	// 检测下车点站不站得下人:用ECC_Pawn通道(Pawn预设忽略ECC_Visibility)，胶囊体尺寸同AForeverCharacter的InitCapsuleSize(42, 96)。
	// 必须忽略载具自己:carMesh的Vehicle档案对ECC_Pawn是Block，否则永远下不了车。
	FCollisionQueryParams queryParams;
	queryParams.AddIgnoredActor(vehicleElement);

	// 起点用车身中心(Z对齐下车点)：sweep只会测到车和下车点之间的外部遮挡(典型：车贴墙停，下车点在墙另一侧)。
	FVector pathStart = vehicleElement->GetActorLocation();
	pathStart.Z = candidateLocation.Z;

	FHitResult blockHit;
	// 胶囊底部离路面只有几个单位（公交原点很低），起点就嵌进路面的ISM会被判"被挡住"（日志里命中点就在起点、
	// 高度10）。sweep整体抬高一点避开地面，下车落点本身不变。
	constexpr float kExitSweepLift = 40.f;
	const FVector sweepLift(0.f, 0.f, kExitSweepLift);
	if (!bRide && !bForce && IsExitPathBlocked(GetWorld(), pathStart + sweepLift, candidateLocation + sweepLift, queryParams, blockHit)) {
		// 带上挡住的对象和命中点，方便定位是哪个东西挡了(而不是只知道"被挡住")；同时写日志，
		// 屏幕消息不会进Forever.log。
		FString blockedText = FString::Printf(
			TEXT("下车位置被挡住，无法下车：%s / %s，起点(%.0f,%.0f,%.0f) 终点(%.0f,%.0f,%.0f) 命中(%.0f,%.0f,%.0f)"),
			*GetNameSafe(blockHit.GetActor()), *GetNameSafe(blockHit.GetComponent()),
			pathStart.X, pathStart.Y, pathStart.Z,
			candidateLocation.X, candidateLocation.Y, candidateLocation.Z,
			blockHit.ImpactPoint.X, blockHit.ImpactPoint.Y, blockHit.ImpactPoint.Z);
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::ExitVehicle: %s"), *blockedText);
		if (GEngine) {
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, blockedText);
		}
		return;
	}

	float dismountYaw = vehicleElement->GetActorRotation().Yaw;
	if (!previousPawn) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::ExitVehicle: previousPawn已失效，玩家将没有可控对象"));
		return;
	}

	// 解除上车时加的距离销毁豁免，恢复正常的流式生成/销毁管理。
	if (ACitizenElement* citizenElement = Cast<ACitizenElement>(previousPawn)) {
		citizenElement->SetDespawnExempt(false);
	}

	// 先恢复显示/碰撞+重新Possess，最后才移动位置，顺序不能变:
	// 1. 移动时碰撞若还是disabled，不会触发UpdateOverlaps，车辆proximity box感知不到人回来，"上车"选项不再弹出。
	// 2. OnOverlapBegin用GetPlayerPawn(0)判断"靠近的是不是玩家"，Possess晚于移动的话，派发事件时它还是载具，
	//    判不通过，事件被丢掉；人站在box里不再有新的Begin/End，选项一直不出现。
	previousPawn->SetActorHiddenInGame(false);
	previousPawn->SetActorEnableCollision(true);
	controller->Possess(previousPawn);
	// 只取车身的Yaw站直——侧翻/颠簸之后Pitch/Roll可能是任意角度，人下车应该始终站直，不应该
	// 继承车身的翻滚姿态，照抄原ToggleVehicle DISMOUNT分支的说明。
	previousPawn->SetActorLocationAndRotation(candidateLocation, FRotator(0.f, dismountYaw, 0.f));
	controller->SetControlRotation(FRotator(0.f, dismountYaw, 0.f));

	// 人已经站回下车点：通知这辆车的script(公交据此把"下车"换回"上车")。
	FireVehicleEvent(vehicle, false, bRide ? "ride" : "drive");

	// 下车瞬间触发盒的"进入"事件不一定能把选项带出来(飞机实测：人在盒内却没有"驾驶"选项，要走出去
	// 再走回来)，下一帧补查一次。
	if (AVehicleElement* drivenElement = Cast<AVehicleElement>(vehicleElement)) {
		TWeakObjectPtr<AVehicleElement> weakVehicle(drivenElement);
		TWeakObjectPtr<APawn> weakPlayer(previousPawn);
		GetWorld()->GetTimerManager().SetTimerForNextTick([weakVehicle, weakPlayer]() {
			if (weakVehicle.IsValid() && weakPlayer.IsValid()) weakVehicle->EnsureOptionsForPlayer(weakPlayer.Get());
		});
	}

	// 车辆本身不销毁，下车后原地留着供下次再上车。
}

void UForeverTrafficFrameworkComponent::GetRouteDebugTarget(const FString& stationType,
	UProceduralMeshComponent*& outMesh, UMaterialInstanceDynamic*& outMaterial) {
	if (stationType == TEXT("bus")) {
		outMesh = busRouteMesh;
		outMaterial = busRouteMaterial;
	} else if (stationType == TEXT("train")) {
		outMesh = trainRouteMesh;
		outMaterial = trainRouteMaterial;
	} else if (stationType == TEXT("plane")) {
		outMesh = planeRouteMesh;
		outMaterial = planeRouteMaterial;
	} else {
		outMesh = otherRouteMesh;
		outMaterial = otherRouteMaterial;
	}
}

void UForeverTrafficFrameworkComponent::BuildRouteDebugMesh(Traffic* traffic) {
	if (!traffic) return;

	AActor* owner = GetOwner();
	if (!owner) return;

	// 四个类别各一个UProceduralMeshComponent，创建方式同ForeverRoadnetFrameworkComponent::BuildNavigationDebugMesh。
	auto ensureMesh = [&](TObjectPtr<UProceduralMeshComponent>& mesh, const TCHAR* debugName) {
		if (!mesh) {
			mesh = NewObject<UProceduralMeshComponent>(owner, debugName);
			mesh->SetupAttachment(owner->GetRootComponent());
			mesh->RegisterComponent();
			owner->AddInstanceComponent(mesh);
		}
	};
	ensureMesh(busRouteMesh, TEXT("BusRouteDebug"));
	ensureMesh(trainRouteMesh, TEXT("TrainRouteDebug"));
	ensureMesh(planeRouteMesh, TEXT("PlaneRouteDebug"));
	ensureMesh(otherRouteMesh, TEXT("OtherRouteDebug"));

	if (!bShowRouteDebug) {
		busRouteMesh->ClearMeshSection(0);
		trainRouteMesh->ClearMeshSection(0);
		planeRouteMesh->ClearMeshSection(0);
		otherRouteMesh->ClearMeshSection(0);
		return;
	}

	if (pureBaseMaterial) {
		if (!busRouteMaterial) {
			busRouteMaterial = UMaterialInstanceDynamic::Create(pureBaseMaterial, this);
			busRouteMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::Green);
		}
		if (!trainRouteMaterial) {
			trainRouteMaterial = UMaterialInstanceDynamic::Create(pureBaseMaterial, this);
			trainRouteMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::Blue);
		}
		if (!planeRouteMaterial) {
			planeRouteMaterial = UMaterialInstanceDynamic::Create(pureBaseMaterial, this);
			planeRouteMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::Red);
		}
		if (!otherRouteMaterial) {
			otherRouteMaterial = UMaterialInstanceDynamic::Create(pureBaseMaterial, this);
		}
	}

	TArray<FVector> busVertices, trainVertices, planeVertices, otherVertices;
	TArray<int32> busTriangles, trainTriangles, planeTriangles, otherTriangles;

	auto targetArrays = [&](const FString& stationType, TArray<FVector>*& outVertices, TArray<int32>*& outTriangles) {
		if (stationType == TEXT("bus")) { outVertices = &busVertices; outTriangles = &busTriangles; }
		else if (stationType == TEXT("train")) { outVertices = &trainVertices; outTriangles = &trainTriangles; }
		else if (stationType == TEXT("plane")) { outVertices = &planeVertices; outTriangles = &planeTriangles; }
		else { outVertices = &otherVertices; outTriangles = &otherTriangles; }
	};

	for (auto& [routeName, route] : traffic->GetRoutes()) {
		if (!route) continue;
		FString stationType = UTF8_TO_TCHAR(route->GetStationType().c_str());

		TArray<FVector>* vertices;
		TArray<int32>* triangles;
		targetArrays(stationType, vertices, triangles);

		for (const vector<Route::RouteLeg>& legs : route->GetLines()) {
			for (const Route::RouteLeg& leg : legs) {
				if (!leg.edge) continue;

				TArray<FVector> points;
				SampleRouteEdge(*leg.edge, points);
				for (int32 i = 0; i + 1 < points.Num(); i++) {
					AppendRouteEdgeRibbon(*vertices, *triangles, points[i], points[i + 1], ROUTE_DEBUG_EDGE_HALF_WIDTH);
				}
				AppendRouteArrow(*vertices, *triangles, points, leg.edge->reversed);
			}
		}
	}

	for (auto& [stationName, station] : traffic->GetStations()) {
		if (!station) continue;
		FString stationType = UTF8_TO_TCHAR(station->GetStationType().c_str());

		TArray<FVector>* vertices;
		TArray<int32>* triangles;
		targetArrays(stationType, vertices, triangles);

		for (const StationInterface& iface : station->GetInterfaces()) {
			FVector world(iface.x * TRAFFIC_WORLD_SCALE, iface.y * TRAFFIC_WORLD_SCALE, iface.z * TRAFFIC_WORLD_SCALE);
			AppendRouteStationBox(*vertices, *triangles, FVector2D(world.X, world.Y),
				ROUTE_DEBUG_NODE_HALF_SIZE, world.Z, world.Z + ROUTE_DEBUG_HEIGHT);
		}
	}

	auto applySection = [](UProceduralMeshComponent* mesh, UMaterialInstanceDynamic* material,
		const TArray<FVector>& vertices, const TArray<int32>& triangles) {
		mesh->ClearMeshSection(0);
		if (triangles.Num() == 0) return;
		mesh->CreateMeshSection(0, vertices, triangles,
			TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		if (material) mesh->SetMaterial(0, material);
	};
	applySection(busRouteMesh, busRouteMaterial, busVertices, busTriangles);
	applySection(trainRouteMesh, trainRouteMaterial, trainVertices, trainTriangles);
	applySection(planeRouteMesh, planeRouteMaterial, planeVertices, planeTriangles);
	applySection(otherRouteMesh, otherRouteMaterial, otherVertices, otherTriangles);
}

void UForeverTrafficFrameworkComponent::ClearTestNavigationLine() {
	testNavSource = nullptr;
	if (testNavMesh) testNavMesh->ClearMeshSection(0);
}

void UForeverTrafficFrameworkComponent::TrackTestNavigationLine(TFunction<bool(TArray<FVector>&)> source, float interval) {
	testNavSource = MoveTemp(source);
	testNavRefreshInterval = FMath::Max(0.02f, interval);
	testNavRefreshCountdown = 0.f; // 下一帧就画
}

void UForeverTrafficFrameworkComponent::StartGuide(Room* target) {
	guide.target = target;
	guide.path.Reset();
	RefreshGuide();
}

void UForeverTrafficFrameworkComponent::ClearGuide() {
	guide = FGuide();
	ClearTestNavigationLine();
}

void UForeverTrafficFrameworkComponent::HideGuideLine() {
	guide.path.Reset();
	ClearTestNavigationLine();
}

void UForeverTrafficFrameworkComponent::RefreshGuide() {
	if (!guide.target) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Map* map = framework ? framework->GetMap() : nullptr;
	Player* corePlayer = framework ? framework->GetPlayer() : nullptr;
	APlayerController* controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* pawn = controller ? controller->GetPawn() : nullptr;
	if (!map || !pawn) { HideGuideLine(); return; }

	Map::VehicleRoutePlan plan;
	bool bOutside = false;
	if (const ACharacter* character = Cast<ACharacter>(pawn)) {
		// 步行（含玩家占有的市民）：人行道导航，终点就是目标房间。
		FVector feet = pawn->GetActorLocation();
		if (const UCapsuleComponent* capsule = character->GetCapsuleComponent()) feet.Z -= capsule->GetScaledCapsuleHalfHeight();
		plan = map->PlanPedestrianRoute(corePlayer ? corePlayer->GetCurrentRoom() : nullptr,
			feet.X / TRAFFIC_WORLD_SCALE, feet.Y / TRAFFIC_WORLD_SCALE, guide.target);
	}
	else if (const AVehicleElement* vehicle = Cast<AVehicleElement>(pawn)) {
		if (vehicle->IsRideMode()) { HideGuideLine(); return; } // 公交乘客：不画
		// 开车：车道导航。先试直接到房间；车到不了就去建筑1楼外部连接点附近的车道。
		const FVector location = pawn->GetActorLocation();
		const FVector forward = pawn->GetActorForwardVector();
		const float x = location.X / TRAFFIC_WORLD_SCALE, y = location.Y / TRAFFIC_WORLD_SCALE;
		Room* startRoom = vehicle->GetVehicle() ? vehicle->GetVehicle()->GetCurrentRoom() : nullptr;
		plan = map->PlanVehicleRoute(startRoom, x, y, guide.target);
		if (plan.status != Map::VehicleRoutePlan::OK || plan.points.empty()) {
			bOutside = true;
			plan = Map::VehicleRoutePlan();
			Building* building = guide.target->GetParentBuilding();
			float bestDistance = FLT_MAX;
			if (building) {
				// 外部连接点"任意一个"：取离车最近的，路线最短。
				for (Node* outside : building->GetOutsideNodes()) {
					if (!outside) continue;
					const float dx = outside->GetX() - x, dy = outside->GetY() - y;
					const float distance = dx * dx + dy * dy;
					if (distance >= bestDistance) continue;
					Map::VehicleRoutePlan candidate = map->PlanVehicleRouteToPoint(startRoom, x, y, outside->GetX(), outside->GetY(), true, forward.X, forward.Y);
					if (candidate.status != Map::VehicleRoutePlan::OK) continue;
					bestDistance = distance;
					plan = std::move(candidate);
				}
			}
		}
	}
	else {
		HideGuideLine(); // 其它Pawn：不画
		return;
	}

	if (plan.status != Map::VehicleRoutePlan::OK || plan.points.size() < 2) {
		UE_LOG(LogTemp, Warning, TEXT("visualize_navigation: 规划不到 %s（status=%d）"),
			UTF8_TO_TCHAR(guide.target->GetAddress().c_str()), static_cast<int32>(plan.status));
		HideGuideLine();
		return;
	}
	guide.bEndsAtOutside = bOutside;
	guide.path.Reset();
	for (const Map::NavPoint& point : plan.points) {
		guide.path.Add(FVector(point.x * TRAFFIC_WORLD_SCALE, point.y * TRAFFIC_WORLD_SCALE, point.z * TRAFFIC_WORLD_SCALE));
	}
	TWeakObjectPtr<UForeverTrafficFrameworkComponent> weakThis(this);
	TrackTestNavigationLine([weakThis](TArray<FVector>& outPath) {
		UForeverTrafficFrameworkComponent* self = weakThis.Get();
		return self && self->UpdateGuide(outPath);
	}, guideRefreshInterval);
}

bool UForeverTrafficFrameworkComponent::UpdateGuide(TArray<FVector>& outPath) {
	if (!guide.target || guide.path.Num() < 2) return false;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* corePlayer = framework ? framework->GetPlayer() : nullptr;
	APlayerController* controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* pawn = controller ? controller->GetPawn() : nullptr;
	if (!pawn) return false;

	// 到达目标房间：导航完成（线终止在外部连接点的不销毁，要下车刷新后再走进房间）。
	if (!guide.bEndsAtOutside && corePlayer && corePlayer->GetCurrentRoom() == guide.target) {
		guide = FGuide();
		return false;
	}

	// 玩家投影到折线上最近的一段：已走过的前缀裁掉；偏离太远重新规划。
	const FVector position = pawn->GetActorLocation();
	int32 nearestSegment = 0;
	float nearestDistanceSq = FLT_MAX;
	FVector nearestPoint = guide.path[0];
	for (int32 i = 0; i + 1 < guide.path.Num(); i++) {
		const FVector closest = FMath::ClosestPointOnSegment(position, guide.path[i], guide.path[i + 1]);
		const float distanceSq = FVector::DistSquared(closest, position);
		if (distanceSq < nearestDistanceSq) { nearestDistanceSq = distanceSq; nearestSegment = i; nearestPoint = closest; }
	}
	const float replan = guideReplanDistance * TRAFFIC_WORLD_SCALE;
	if (nearestDistanceSq > replan * replan) {
		RefreshGuide();
		if (!guide.target || guide.path.Num() < 2) return false;
		outPath = guide.path;
		return true;
	}
	TArray<FVector> remaining;
	if (nearestSegment + 1 >= guide.path.Num() - 1) {
		// 已经在最后一段：保留这一段不裁完，线停在终点上（外部连接点的线不消失）。
		remaining.Add(guide.path[guide.path.Num() - 2]);
		remaining.Add(guide.path.Last());
	}
	else {
		remaining.Add(nearestPoint);
		for (int32 i = nearestSegment + 1; i < guide.path.Num(); i++) remaining.Add(guide.path[i]);
	}
	guide.path = remaining;
	outPath = remaining;
	return true;
}

void UForeverTrafficFrameworkComponent::ShowTestNavigationLine(const TArray<FVector>& path) {
	// 导航折线：浅蓝色水平扁带，抬高一点免得埋进路面。
	AActor* owner = GetOwner();
	if (owner && path.Num() >= 2) {
		if (!testNavMesh) {
			testNavMesh = NewObject<UProceduralMeshComponent>(owner, TEXT("TestNavDebug"));
			testNavMesh->SetupAttachment(owner->GetRootComponent());
			testNavMesh->RegisterComponent();
			owner->AddInstanceComponent(testNavMesh);
		}
		if (pureBaseMaterial && !testNavMaterial) {
			testNavMaterial = UMaterialInstanceDynamic::Create(pureBaseMaterial, this);
			testNavMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.45f, 0.78f, 1.f));
		}
		constexpr float kLift = 80.f, kHalfWidth = 30.f;
		TArray<FVector> vertices;
		TArray<int32> triangles;
		for (int32 i = 0; i + 1 < path.Num(); i++) {
			FVector a = path[i] + FVector(0.f, 0.f, kLift), b = path[i + 1] + FVector(0.f, 0.f, kLift);
			AppendRouteEdgeRibbon(vertices, triangles, a, b, kHalfWidth);
		}
		AppendRouteStationBox(vertices, triangles, FVector2D(path.Last().X, path.Last().Y),
			ROUTE_DEBUG_NODE_HALF_SIZE, path.Last().Z + kLift, path.Last().Z + kLift + 200.f);
		testNavMesh->ClearMeshSection(0);
		testNavMesh->CreateMeshSection(0, vertices, triangles,
			TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		if (testNavMaterial) testNavMesh->SetMaterial(0, testNavMaterial);
	}
}

UInstancedStaticMeshComponent* UForeverTrafficFrameworkComponent::GetOrCreateTrackISM(const string& meshPath) {
	if (meshPath.empty()) return nullptr;
	FString key = UTF8_TO_TCHAR(meshPath.c_str());

	if (auto* found = trackMeshInstances.Find(key)) {
		return *found;
	}

	AActor* owner = GetOwner();
	if (!owner) return nullptr;

	UStaticMesh* mesh = LoadObject<UStaticMesh>(nullptr, *key);
	if (!mesh) {
		debugf("UForeverTrafficFrameworkComponent::GetOrCreateTrackISM: failed to load track mesh %s.\n", meshPath.c_str());
		return nullptr;
	}

	UInstancedStaticMeshComponent* ism = NewObject<UInstancedStaticMeshComponent>(owner, NAME_None, RF_Transient);
	ism->SetStaticMesh(mesh);
	ism->SetupAttachment(owner->GetRootComponent());
	ism->RegisterComponent();
	owner->AddInstanceComponent(ism);

	trackMeshInstances.Add(key, ism);
	return ism;
}

void UForeverTrafficFrameworkComponent::BuildTracks(Traffic* traffic) {
	if (!traffic) return;

	// 重铺前清掉上一次的实例(同一个Actor上重新开局/读档)。
	for (auto& [key, ism] : trackMeshInstances) {
		if (ism) ism->ClearInstances();
	}

	for (auto& [routeName, route] : traffic->GetRoutes()) {
		if (!route || !route->ShouldInstantiate()) continue;

		const string& trackMesh = route->GetTrackMesh();
		float unit = route->GetTrackUnit();
		if (trackMesh.empty() || unit <= 0.f) {
			debugf("UForeverTrafficFrameworkComponent::BuildTracks: route %s has no trackMesh/trackUnit, skip laying tracks.\n",
				route->GetName().c_str());
			continue;
		}
		const string& tunnelMesh = route->GetTrackTunnelMesh().empty() ? trackMesh : route->GetTrackTunnelMesh();

		// 正反向共用同一份几何，同一条边只铺一次(跳过reversed和重复的边指针)。
		TSet<const Route::RouteEdge*> laid;
		int32 instanceCount = 0;
		for (const vector<Route::RouteLeg>& legs : route->GetLines()) {
			for (const Route::RouteLeg& leg : legs) {
				const Route::RouteEdge* edge = leg.edge;
				if (!edge || edge->reversed || laid.Contains(edge)) continue;
				laid.Add(edge);

				for (size_t segIndex = 0; segIndex < edge->segments.size(); segIndex++) {
					Connection* conn = edge->segments[segIndex];
					bool tunnel = segIndex < edge->track.size() && edge->track[segIndex].tunnel;
					UInstancedStaticMeshComponent* ism = GetOrCreateTrackISM(tunnel ? tunnelMesh : trackMesh);
					if (!ism || !conn) continue;

					// 同BuildRoadInstances：n夹在[0.8,1.2]*unit之内，scaleAlong把每块拉伸到实际段长。
					float segLen = conn->CalcDistance();
					int nLow = FMath::CeilToInt(segLen / (1.2f * unit));
					int nHigh = FMath::FloorToInt(segLen / (0.8f * unit));
					int n = (nLow <= nHigh && nLow > 0) ? FMath::Clamp(FMath::RoundToInt(segLen / unit), nLow, nHigh) : FMath::Max(1, FMath::RoundToInt(segLen / unit));
					float scaleAlong = (segLen / n) / unit;

					for (int k = 0; k < n; k++) {
						float t0 = static_cast<float>(k) / n;
						float t1 = static_cast<float>(k + 1) / n;
						Node start = conn->GetPoint(t0);
						Node end = conn->GetPoint(t1);
						FVector startW(start.GetX() * TRAFFIC_WORLD_SCALE, start.GetY() * TRAFFIC_WORLD_SCALE, start.GetZ() * TRAFFIC_WORLD_SCALE);
						FVector endW(end.GetX() * TRAFFIC_WORLD_SCALE, end.GetY() * TRAFFIC_WORLD_SCALE, end.GetZ() * TRAFFIC_WORLD_SCALE);

						// 轨道mesh沿局部+X延伸、轴心在起点边：摆在块的起点，朝向取起点->终点的3D方向。
						FVector fwd = (endW - startW).GetSafeNormal();
						if (fwd.IsNearlyZero()) fwd = FVector::ForwardVector;
						ism->AddInstance(FTransform(fwd.Rotation(), startW, FVector(scaleAlong, 1.f, 1.f)));
						instanceCount++;
					}
				}
			}
		}
		debugf("UForeverTrafficFrameworkComponent::BuildTracks: route %s laid %d track instance(s).\n",
			route->GetName().c_str(), instanceCount);
	}
}

AVehicleElement* UForeverTrafficFrameworkComponent::FindVehicleElement(const FString& vehicleName) const {
	const TObjectPtr<AVehicleElement>* found = activeVehicles.Find(vehicleName);
	return found ? found->Get() : nullptr;
}


// ---------------------------------------------------------------------------------------------
// 公交 Controller 驱动
// ---------------------------------------------------------------------------------------------

void UForeverTrafficFrameworkComponent::FireStopEvent(Vehicle* vehicle, int32 stationNodeId, bool entering) {
	if (!vehicle) return;
	Station** found = stationByNodeId.Find(stationNodeId);
	if (!found || !*found) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	UForeverStoryFrameworkComponent* storyFramework = framework ? framework->GetStoryFramework() : nullptr;
	if (!storyFramework) return;

	// 和 AForeverFrameworkActor 里转发 Core 时刻表停站事件是同一对事件，只发给这辆车自己的 script。
	if (entering) {
		EnterStopEvent event((*found)->GetName());
		storyFramework->BroadcastToVehicle(event, vehicle);
	}
	else {
		LeaveStopEvent event((*found)->GetName());
		storyFramework->BroadcastToVehicle(event, vehicle);
	}
}

UForeverTrafficFrameworkComponent::FBusState* UForeverTrafficFrameworkComponent::FindOrAddBusState(const FString& vehicleName) {
	if (FBusState* found = busStates.Find(vehicleName)) return found;
	// 停车场公交第一次被 Controller 驱动：登记状态（没有时刻表、没有运动学表示）。
	for (FCarEntry& entry : carEntries) {
		if (entry.name == vehicleName && entry.vehicle) {
			FBusState state;
			state.vehicle = entry.vehicle;
			state.name = vehicleName;
			state.bNoPhysics = true;
			return &busStates.Add(vehicleName, state);
		}
	}
	return nullptr;
}

bool UForeverTrafficFrameworkComponent::IsDrivenByController(const AVehicleElement* element) const {
	if (!element || !element->GetVehicle()) return false;
	const FString elementName = UTF8_TO_TCHAR(element->GetVehicle()->GetName().c_str());
	if (const TWeakObjectPtr<AVehicleCommandController>* driver = carDrivers.Find(elementName)) {
		if (driver->IsValid() && (*driver)->HasCommands()) return true;
	}
	const FBusState* state = busStates.Find(elementName);
	if (!state) return false;
	const ABusController* controller = state->controller.Get();
	return controller && controller->HasCommands();
}

AVehicleElement* UForeverTrafficFrameworkComponent::FindBusPhysicsElement(const FString& vehicleName) const {
	const FBusState* state = busStates.Find(vehicleName);
	return state ? state->element.Get() : nullptr;
}

bool UForeverTrafficFrameworkComponent::IsSwitchableRouteBus(const FString& vehicleName) const {
	const FBusState* state = busStates.Find(vehicleName);
	return state && state->bRouteBus && !state->bNoPhysics;
}

AVehicleElement* UForeverTrafficFrameworkComponent::EnsureBusPhysicsElement(const FString& vehicleName) {
	FBusState* state = busStates.Find(vehicleName);
	if (!state) return nullptr;
	if (AVehicleElement* element = state->element.Get()) return element;
	if (state->bRouteBus && !state->bNoPhysics && BusToPhysics(*state)) return state->element.Get();
	return nullptr;
}

void UForeverTrafficFrameworkComponent::RegisterDriver(AVehicleCommandController* driver) {
	if (driver) drivers.AddUnique(driver);
}

JunctionTraffic* UForeverTrafficFrameworkComponent::GetJunctionTraffic() {
	if (!junctionTraffic.IsValid() && mapRef) {
		junctionTraffic = MakeShared<JunctionTraffic>();
		junctionTraffic->BuildFromMap(*mapRef);
		UE_LOG(LogTemp, Log, TEXT("UForeverTrafficFrameworkComponent: junction traffic built (%d connections)."),
			static_cast<int32>(junctionTraffic->GetConnectionCount()));
	}
	return junctionTraffic.Get();
}

ABusController* UForeverTrafficFrameworkComponent::SpawnBusController(AVehicleElement* element) {
	return Cast<ABusController>(SpawnDriver(ABusController::StaticClass(), element));
}

AVehicleCommandController* UForeverTrafficFrameworkComponent::SpawnDriver(UClass* controllerClass, AVehicleElement* element) {
	if (!element || !controllerClass || !GetWorld()) return nullptr;
	FActorSpawnParameters params;
	params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVehicleCommandController* controller = Cast<AVehicleCommandController>(GetWorld()->SpawnActor<AActor>(
		controllerClass, FVector::ZeroVector, FRotator::ZeroRotator, params));
	if (!controller) return nullptr;

	controller->Setup(mapRef, this);
	// 范围判定带滞回：已经是物理 -> 用完整范围才离开；还是运动学 -> 进缩小后的范围才进入。
	TWeakObjectPtr<UForeverTrafficFrameworkComponent> weakThis(this);
	controller->SetPhysicsRangeQuery([weakThis](const FVector& position, bool bCurrentlyPhysics) {
		UForeverTrafficFrameworkComponent* self = weakThis.Get();
		AForeverFrameworkActor* framework = self ? Cast<AForeverFrameworkActor>(self->GetOwner()) : nullptr;
		UForeverTerrainFrameworkComponent* terrain = framework ? framework->GetTerrainFramework() : nullptr;
		if (!terrain || !terrain->IsTerrainGenerated()) return true; // 地形没好：保持原状（物理）
		return terrain->IsInsideLodLevel(position, self->busPhysicsLodLevel, bCurrentlyPhysics ? 1.f : self->busPhysicsEnterScale);
	});
	controller->Possess(element);
	return controller;
}

ABusController* UForeverTrafficFrameworkComponent::GetOrCreateBusController(FBusState& state) {
	if (ABusController* existing = state.controller.Get()) return existing;
	AVehicleElement* element = state.element.Get();
	if (!element) return nullptr;
	ABusController* controller = SpawnBusController(element);
	state.controller = controller;
	return controller;
}

void UForeverTrafficFrameworkComponent::CollectBusLoops(TArray<TArray<int32>>& outLoops) const {
	if (!trafficRef) return;
	for (const auto& [routeName, route] : trafficRef->GetRoutes()) {
		if (!route || route->GetStationType() != "bus") continue;
		for (size_t lineIdx = 0; lineIdx < route->GetLines().size(); lineIdx++) {
			std::vector<int> ids = route->GetLineStopNodeIds(lineIdx);
			if (ids.size() < 2) continue;
			TArray<int32> loop;
			for (int id : ids) loop.Add(id);
			outLoops.Add(MoveTemp(loop));
		}
	}
}

void UForeverTrafficFrameworkComponent::UpdateBusPhysicsHandoff() {
	if (busStates.Num() == 0) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	if (!framework || !framework->IsInitialGenerationDone()) return;
	UForeverTerrainFrameworkComponent* terrain = framework->GetTerrainFramework();
	if (!terrain || !terrain->IsTerrainGenerated()) return;

	// 每帧最多切换一辆（生成物理公交是整车 Chaos 组件，成本高）。
	for (TPair<FString, FBusState>& pair : busStates) {
		FBusState& state = pair.Value;
		if (!state.bRouteBus || state.bDetached || state.bNoPhysics || !state.vehicle) continue;

		AVehicleElement* element = state.element.Get();
		const bool bPhysics = element != nullptr;
		if (!bPhysics && state.vehicle->IsPhysicsDriven()) {
			// 物理 Actor 被意外销毁：交回时刻表。
			state.vehicle->SetPhysicsDriven(false);
			if (Route* route = state.vehicle->GetRoute()) route->ResetStopState(state.vehicle);
			if (ATransitVehicleElement* kinematic = state.kinematic.Get()) kinematic->SetSuspended(false);
		}

		FVector position;
		if (element) {
			position = element->GetActorLocation();
		}
		else {
			float x, y, z, yaw;
			state.vehicle->GetTransform(x, y, z, yaw);
			position = FVector(x, y, z) * TRAFFIC_WORLD_SCALE;
		}
		const bool bInside = terrain->IsInsideLodLevel(position, busPhysicsLodLevel, bPhysics ? 1.f : busPhysicsEnterScale);
		if (bInside && !bPhysics) {
			if (BusToPhysics(state)) return;
		}
		else if (!bInside && bPhysics) {
			if (element->IsPlayerControlled()) continue; // 玩家在开/搭乘：不能收回
			BusToKinematic(state);
			return;
		}
	}
}

bool UForeverTrafficFrameworkComponent::BusToPhysics(FBusState& state) {
	Vehicle* vehicle = state.vehicle;
	Route* route = vehicle ? vehicle->GetRoute() : nullptr;
	if (!route || !state.physicsClass || !GetWorld()) {
		state.bNoPhysics = true;
		return false;
	}
	const Route::ScheduleState schedule = route->QueryVehicleState(vehicle);
	if (!schedule.valid) return false;
	std::vector<int> ids = route->GetLineStopNodeIds(static_cast<size_t>(schedule.line));
	if (ids.size() < 2) {
		state.bNoPhysics = true;
		return false;
	}
	TArray<int32> nodes;
	for (int id : ids) nodes.Add(id);

	float x, y, z, yaw;
	vehicle->GetTransform(x, y, z, yaw);
	const FVector location(x * TRAFFIC_WORLD_SCALE, y * TRAFFIC_WORLD_SCALE, z * TRAFFIC_WORLD_SCALE + TRAFFIC_HEIGHT_EPSILON);

	// 先挂起运动学那辆（撤掉它名下的选项），再生成物理的：新车的触发盒注册时会补出选项。
	if (ATransitVehicleElement* kinematic = state.kinematic.Get()) kinematic->SetSuspended(true);

	FActorSpawnParameters params;
	params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVehicleElement* element = GetWorld()->SpawnActor<AVehicleElement>(
		state.physicsClass, location, FRotator(0.f, yaw, 0.f), params);
	if (!element) {
		if (ATransitVehicleElement* kinematic = state.kinematic.Get()) kinematic->SetSuspended(false);
		return false;
	}
	vehicle->SetPhysicsDriven(true);
	element->Init(vehicle, nullptr);
	const FVector forward = FRotator(0.f, yaw, 0.f).Vector();
	element->ResumePhysics(forward * schedule.speedMapPerRealSecond * TRAFFIC_WORLD_SCALE);

	state.element = element;
	activeVehicles.Add(state.name, element);
	ABusController* controller = SpawnBusController(element);
	state.controller = controller;
	if (!controller) return true;

	// 时刻表 -> 指令：legIndex 那一腿正驶向第 legIndex+1 站；停靠中则车在第 legIndex+1 站里。
	const int32 count = nodes.Num();
	FVehicleCommand command = FVehicleCommand::MakeLoop(nodes, 0);
	if (schedule.dwelling) {
		const int32 stopIndex = (schedule.legIndex + 1) % count;
		command.loopStart = (stopIndex + 1) % count;
		command.bStartAtStop = true;
		command.startDwellSeconds = schedule.dwellRemainingSeconds;
		command.bStopAlreadyEntered = true; // Core 时刻表段已经发过 enter_stop，Controller 只负责补出站
	}
	else {
		command.loopStart = (schedule.legIndex + 1) % count;
	}
	controller->PushCommand(command);
	return true;
}

void UForeverTrafficFrameworkComponent::BusToKinematic(FBusState& state, bool bSyncProgress) {
	AVehicleElement* element = state.element.Get();
	// 先把时刻表进度对齐到实车当前位置（必须在ClearCommands之前读控制器状态）：物理车比名义速度慢，
	// 不对齐的话交还瞬间时刻表算出来的位置早已领先一大截，公交会"瞬移"到玩家附近。
	if (ABusController* controller = bSyncProgress ? state.controller.Get() : nullptr) {
		int32 legIndex;
		float legFraction, dwellFraction;
		bool bDwelling;
		if (state.vehicle && controller->QueryLoopProgress(legIndex, legFraction, bDwelling, dwellFraction)) {
			if (Route* route = state.vehicle->GetRoute()) {
				route->SyncVehicleProgress(state.vehicle, legIndex, legFraction, bDwelling, dwellFraction);
			}
		}
	}
	if (ABusController* controller = state.controller.Get()) {
		controller->ClearCommands(); // 停站中会补发 leave_stop
		controller->Destroy();
	}
	if (element) {
		activeVehicles.Remove(state.name);
		element->Destroy();
	}
	state.element = nullptr;
	state.controller = nullptr;

	if (state.vehicle) {
		state.vehicle->SetPhysicsDriven(false);
		if (Route* route = state.vehicle->GetRoute()) route->ResetStopState(state.vehicle);
	}
	if (ATransitVehicleElement* kinematic = state.kinematic.Get()) kinematic->SetSuspended(false);
}

void UForeverTrafficFrameworkComponent::ResyncBusesAfterTimeSkip() {
	for (TPair<FString, FBusState>& pair : busStates) {
		FBusState& state = pair.Value;
		if (!state.bRouteBus || state.bDetached || !state.element.IsValid()) continue;
		if (state.element->IsPlayerControlled()) continue; // 玩家在开/搭乘：不动
		BusToKinematic(state, false); // 之后UpdateBusPhysicsHandoff会按时刻表位置重新决定要不要切物理
	}
}

void UForeverTrafficFrameworkComponent::StartBusTestRun(const FString& vehicleName) {
	AVehicleElement* element = IsSwitchableRouteBus(vehicleName) ? EnsureBusPhysicsElement(vehicleName) : FindOrSpawnVehicle(vehicleName);
	if (!element) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::StartBusTestRun: 找不到车辆 %s"), *vehicleName);
		return;
	}
	FBusState* state = FindOrAddBusState(vehicleName);
	if (!state) return;
	state->element = element;

	TArray<TArray<int32>> loops;
	CollectBusLoops(loops);
	if (loops.Num() == 0) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::StartBusTestRun: 没有可用的公交环线"));
		return;
	}
	ABusController* controller = GetOrCreateBusController(*state);
	if (!controller) return;

	const TArray<int32>& loop = loops[FMath::RandRange(0, loops.Num() - 1)];
	const int32 stopIndex = FMath::RandRange(0, loop.Num() - 1);
	controller->ClearCommands();
	controller->PushCommand(FVehicleCommand::MakeNavigate(loop[stopIndex]));
	FVehicleCommand loopCommand = FVehicleCommand::MakeLoop(loop, (stopIndex + 1) % loop.Num());
	loopCommand.bStartAtStop = true;               // 导航到这个站之后，接着像刚进站一样先停靠再出发
	loopCommand.startDwellSeconds = controller->dwellSeconds;
	loopCommand.bFireEnterAtStart = true;
	controller->PushCommand(loopCommand);

	if (state->bRouteBus) {
		// 线路公交退出时刻表，之后一直由 Controller 持有（LOD 外靠 Controller 的运动学模式）。
		state->bDetached = true;
		if (state->vehicle) state->vehicle->SetPhysicsDriven(true);
	}
}

// ---------------------------------------------------------------------------------------------
// 脚本驱动的车辆导航（vehicle_navigate / vehicle_arrive）
// ---------------------------------------------------------------------------------------------

void UForeverTrafficFrameworkComponent::NavigateVehicle(const FString& vehicleName, const FString& destination) {
	pendingNavigations.Add({ vehicleName, destination });
}

void UForeverTrafficFrameworkComponent::ProcessPendingNavigations() {
	if (!mapRef || !trafficRef) return; // 车辆还没登记（GenerateVehicles之前）：请求先留着
	int32 budget = FMath::Max(1, maxVehicleSpawnsPerFrame);
	while (pendingNavigations.Num() > 0 && budget-- > 0) {
		const FNavRequest request = pendingNavigations[0];
		pendingNavigations.RemoveAt(0);
		StartVehicleNavigation(request);
	}
}

Room* UForeverTrafficFrameworkComponent::PickRandomVehicleRoom(const Room* exclude) {
	if (!mapRef) return nullptr;
	if (vehicleRoomCache.Num() == 0) {
		// 有车行节点(在vehicleNavGraph里)的房间，地下室(layer<0)优先；没有地下室的就用全部。
		const auto& graph = mapRef->GetVehicleNavGraph();
		TArray<Room*> basement, other;
		for (const auto& [buildingName, building] : mapRef->GetBuildings()) {
			if (!building) continue;
			for (Room* room : building->GetRooms()) {
				Node* node = room ? room->GetNavigationNode() : nullptr;
				if (!node || graph.find(node->GetId()) == graph.end()) continue;
				(room->GetLayer() < 0 ? basement : other).Add(room);
			}
		}
		vehicleRoomCache = basement.Num() > 0 ? basement : other;
		UE_LOG(LogTemp, Log, TEXT("UForeverTrafficFrameworkComponent: %d rooms with vehicle nodes (basement %d)."),
			vehicleRoomCache.Num(), basement.Num());
	}
	if (vehicleRoomCache.Num() == 0) return nullptr;

	// 车在房间里：只在从它出发沿车行图真正走得到的房间里挑（很多地下室互相不连通，乱挑大多规划不到）。
	Node* startNode = exclude ? exclude->GetNavigationNode() : nullptr;
	if (startNode) {
		TSet<int32>* reachable = reachableCache.Find(startNode->GetId());
		if (!reachable) {
			TSet<int32> visited;
			TArray<int32> stack;
			visited.Add(startNode->GetId());
			stack.Add(startNode->GetId());
			const auto& graph = mapRef->GetVehicleNavGraph();
			while (stack.Num() > 0) {
				const int32 current = stack.Pop();
				auto it = graph.find(current);
				if (it == graph.end()) continue;
				for (const auto& edge : it->second) {
					bool bAlready = false;
					visited.Add(edge.first, &bAlready);
					if (!bAlready) stack.Add(edge.first);
				}
			}
			reachable = &reachableCache.Add(startNode->GetId(), MoveTemp(visited));
		}
		TArray<Room*> candidates;
		for (Room* room : vehicleRoomCache) {
			Node* node = room->GetNavigationNode();
			if (room != exclude && node && reachable->Contains(node->GetId())) candidates.Add(room);
		}
		return candidates.Num() > 0 ? candidates[FMath::RandRange(0, candidates.Num() - 1)] : nullptr;
	}

	for (int32 attempt = 0; attempt < 8; attempt++) {
		Room* room = vehicleRoomCache[FMath::RandRange(0, vehicleRoomCache.Num() - 1)];
		if (room != exclude || vehicleRoomCache.Num() == 1) return room;
	}
	return vehicleRoomCache[0];
}

AVehicleCommandController* UForeverTrafficFrameworkComponent::GetOrCreateCarDriver(const FString& vehicleName, AVehicleElement* element) {
	if (TWeakObjectPtr<AVehicleCommandController>* found = carDrivers.Find(vehicleName)) {
		if (AVehicleCommandController* existing = found->Get()) {
			if (existing->GetDrivenVehicle() == element) return existing;
		}
		carDrivers.Remove(vehicleName);
	}
	AVehicleCommandController* controller = SpawnDriver(AVehicleCommandController::StaticClass(), element);
	if (!controller) return nullptr;
	TWeakObjectPtr<UForeverTrafficFrameworkComponent> weakThis(this);
	controller->SetNavigateFinishedCallback([weakThis](AVehicleCommandController* c, int32 nodeId, bool bArrived) {
		if (UForeverTrafficFrameworkComponent* self = weakThis.Get()) self->OnCarNavigateFinished(c, nodeId, bArrived);
	});
	carDrivers.Add(vehicleName, controller);
	return controller;
}

void UForeverTrafficFrameworkComponent::StartVehicleNavigation(const FNavRequest& request) {
	if (!mapRef) return;
	// 线路公交有自己的时刻表/循环指令，不接这条路。
	if (const FBusState* busState = busStates.Find(request.vehicleName)) {
		if (busState->bRouteBus) return;
	}
	FCarEntry* entry = nullptr;
	for (FCarEntry& candidate : carEntries) {
		if (candidate.name == request.vehicleName) { entry = &candidate; break; }
	}
	if (!entry || !entry->vehicle) {
		UE_LOG(LogTemp, Warning, TEXT("vehicle_navigate: 找不到车辆 %s"), *request.vehicleName);
		return;
	}
	Vehicle* core = entry->vehicle;
	AVehicleElement* element = entry->element.Get();
	if (element && element->IsPlayerControlled()) return; // 玩家在开

	// 车停在没有车行节点的房间里（只有工厂有）必然规划失败，直接跳过。
	Room* parked = core->GetCurrentRoom();
	Node* parkedNode = parked ? parked->GetNavigationNode() : nullptr;
	if (parked && (!parkedNode || mapRef->GetVehicleNavGraph().find(parkedNode->GetId()) == mapRef->GetVehicleNavGraph().end())) {
		static int32 skipped = 0;
		if (++skipped % 100 == 1) {
			UE_LOG(LogTemp, Warning, TEXT("vehicle_navigate: 车辆 %s 停在没有车行节点的房间，跳过（共跳过 %d 辆）"), *request.vehicleName, skipped);
		}
		return;
	}

	const bool bRandom = request.destination == TEXT("random");
	Room* destination = bRandom ? PickRandomVehicleRoom(core->GetCurrentRoom())
		: mapRef->LocateRoom(TCHAR_TO_UTF8(*request.destination));
	Node* node = destination ? destination->GetNavigationNode() : nullptr;
	if (!node) {
		UE_LOG(LogTemp, Warning, TEXT("vehicle_navigate: 车辆 %s 找不到目的地 %s"), *request.vehicleName, *request.destination);
		return;
	}
	navTargets.Add(request.vehicleName, destination);
	randomTargets.Add(request.vehicleName, bRandom);

	// 有Actor（在玩家附近）：交给Controller开。
	if (element) {
		AVehicleCommandController* controller = GetOrCreateCarDriver(request.vehicleName, element);
		if (!controller) return;
		controller->ClearCommands();
		controller->PushCommand(FVehicleCommand::MakeNavigate(node->GetId()));
		return;
	}

	// 没有Actor：和市民一样，Core按时间沿路线推算位置，进入生成距离才生成Actor（见TickComponent）。
	float x, y, z, yaw;
	core->GetTransform(x, y, z, yaw);
	const float yawRad = FMath::DegreesToRadians(yaw);
	Map::VehicleRoutePlan plan = mapRef->PlanVehicleRouteToNode(core->GetCurrentRoom(),
		x / TRAFFIC_WORLD_SCALE, y / TRAFFIC_WORLD_SCALE, node->GetId(), true, FMath::Cos(yawRad), FMath::Sin(yawRad));
	if (plan.status != Map::VehicleRoutePlan::OK) {
		UE_LOG(LogTemp, Log, TEXT("vehicle_navigate: 车辆 %s 从 %s 规划到 %s 失败，status=%d(1=无路径到终点,2=起点无出路)"), *request.vehicleName,
			core->GetCurrentRoom() ? UTF8_TO_TCHAR(core->GetCurrentRoom()->GetAddress().c_str()) : TEXT("室外"),
			UTF8_TO_TCHAR(destination->GetAddress().c_str()), static_cast<int32>(plan.status));
		pendingNavResults.Add({ core, destination, false, bRandom });
		navTargets.Remove(request.vehicleName);
		return;
	}
	std::vector<Vehicle::DrivePoint> path;
	for (const Map::NavPoint& p : plan.points) {
		path.push_back({ p.x * TRAFFIC_WORLD_SCALE, p.y * TRAFFIC_WORLD_SCALE, p.z * TRAFFIC_WORLD_SCALE + TRAFFIC_HEIGHT_EPSILON });
	}
	core->SetCurrentRoom(nullptr);
	core->BeginDrive(path, destination, GetWorld()->GetTimeSeconds(),
		GetDefault<AVehicleCommandController>()->maxSpeed * kUnspawnedDriveSpeedScale);
	if (!core->IsDriving()) { // 已经在目的地/路线只有一个点：当作立刻到达
		core->SetCurrentRoom(destination);
		pendingNavResults.Add({ core, destination, true, bRandom });
		navTargets.Remove(request.vehicleName);
	}
}

void UForeverTrafficFrameworkComponent::HandOffDriveToActor(FCarEntry& entry) {
	Vehicle* core = entry.vehicle;
	AVehicleElement* element = entry.element.Get();
	Room* destination = core ? core->GetDriveDestination() : nullptr;
	Node* node = destination ? destination->GetNavigationNode() : nullptr;
	if (!element || !node) return;
	core->EndDrive();
	if (AVehicleCommandController* controller = GetOrCreateCarDriver(entry.name, element)) {
		controller->ClearCommands();
		controller->PushCommand(FVehicleCommand::MakeNavigate(node->GetId()));
	}
}

bool UForeverTrafficFrameworkComponent::HandBackDriveToCore(FCarEntry& entry) {
	AVehicleElement* element = entry.element.Get();
	Room** destination = navTargets.Find(entry.name);
	TWeakObjectPtr<AVehicleCommandController>* driver = carDrivers.Find(entry.name);
	if (!element || !destination || !*destination || !driver || !driver->IsValid()) return false;
	TArray<FVector> remaining;
	if (!(*driver)->GetRemainingWaypoints(remaining) || remaining.Num() < 2) return false;
	std::vector<Vehicle::DrivePoint> path;
	for (const FVector& point : remaining) path.push_back({ static_cast<float>(point.X), static_cast<float>(point.Y), static_cast<float>(point.Z) });
	entry.vehicle->SetCurrentRoom(nullptr);
	entry.vehicle->BeginDrive(path, *destination, GetWorld()->GetTimeSeconds(), (*driver)->maxSpeed * kUnspawnedDriveSpeedScale);
	if (!entry.vehicle->IsDriving()) return false;
	(*driver)->ClearCommands();
	carDrivers.Remove(entry.name);
	activeVehicles.Remove(entry.name);
	element->Destroy();
	entry.element = nullptr;
	return true;
}

void UForeverTrafficFrameworkComponent::OnCarNavigateFinished(AVehicleCommandController* controller, int32 nodeId, bool bArrived) {
	AVehicleElement* element = controller ? controller->GetDrivenVehicle() : nullptr;
	Vehicle* core = element ? element->GetVehicle() : nullptr;
	if (!core) return;
	const FString name = UTF8_TO_TCHAR(core->GetName().c_str());
	Room** target = navTargets.Find(name);
	if (!target || !*target) return;
	pendingNavResults.Add({ core, *target, bArrived, randomTargets.FindRef(name) });
	navTargets.Remove(name);
}

void UForeverTrafficFrameworkComponent::ProcessNavResults() {
	if (pendingNavResults.Num() == 0) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	UForeverStoryFrameworkComponent* storyFramework = framework ? framework->GetStoryFramework() : nullptr;
	const TArray<FNavResult> results = MoveTemp(pendingNavResults);
	pendingNavResults.Reset();
	for (const FNavResult& result : results) {
		if (!result.bArrived) {
			// 随机目的地可能连不通（比如只有出口没有入口的地下室），换一个再试，最多5次。
			const FString failedName = UTF8_TO_TCHAR(result.vehicle->GetName().c_str());
			int32& tries = randomRetries.FindOrAdd(failedName);
			if (result.bRandom && ++tries <= 5) {
				pendingNavigations.Add({ failedName, TEXT("random") });
				continue;
			}
			tries = 0;
			UE_LOG(LogTemp, Warning, TEXT("vehicle_navigate: 车辆 %s 规划不到 %s，放弃"),
				UTF8_TO_TCHAR(result.vehicle->GetName().c_str()), UTF8_TO_TCHAR(result.room->GetAddress().c_str()));
			continue;
		}
		// 车现在停在这个房间的导航节点上：记成"在房间里"，下一次规划就从房间出发。
		result.vehicle->SetCurrentRoom(result.room);
		if (!storyFramework) continue;
		randomRetries.Remove(UTF8_TO_TCHAR(result.vehicle->GetName().c_str()));
		UE_LOG(LogTemp, Log, TEXT("vehicle_arrive: 车辆 %s 抵达 %s"), UTF8_TO_TCHAR(result.vehicle->GetName().c_str()), UTF8_TO_TCHAR(result.room->GetAddress().c_str()));
		VehicleArriveEvent event(result.vehicle->GetName(), result.room->GetAddress());
		storyFramework->BroadcastToVehicle(event, result.vehicle);
	}
}

void UForeverTrafficFrameworkComponent::StopBusRun(const FString& vehicleName) {
	FBusState* state = busStates.Find(vehicleName);
	if (!state) return;
	if (state->bRouteBus) {
		// 线路公交：停止运行 = 退出时刻表，由 Controller 刹停并持有。先确保物理表示+Controller 存在。
		EnsureBusPhysicsElement(vehicleName);
		if (state->vehicle) state->vehicle->SetPhysicsDriven(true);
		state->bDetached = true;
	}
	if (ABusController* controller = GetOrCreateBusController(*state)) {
		controller->StopNow();
	}
}
