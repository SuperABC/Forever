#include "Framework/ForeverTrafficFrameworkComponent.h"

#include "Framework/CoreGuard.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverStoryFrameworkComponent.h"
#include "Element/VehicleElement.h"
#include "Element/TransitVehicleElement.h"
#include "Element/CitizenElement.h"
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
#include "map/green.h"
#include "map/roadnet.h"
#include "common/utility.h"
#include "common/random.h"
#include "story/event.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "player/player.h"
#include "populace/populace.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"
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

	// 一条路径(多段connections拼起来)按弧长采样成世界坐标折线。相邻两段共享的端点只采一次。
	void SampleRouteEdge(const TrackPath& edge, TArray<FVector>& outPoints) {
		for (Connection* seg : edge.connections) {
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
	// 组件默认在TG_DuringPhysics里tick，和Chaos物理(含载具的异步模拟线程)并行；这里会销毁/生成载具Actor，
	// 销毁时悬挂约束被释放，而物理线程可能正在ApplySuspensionForces里读它(打包版路边走路时Chaos悬挂崩溃)。
	// 放到物理结束之后tick，销毁和物理线程就不会重叠。
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
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
	lineVehicleStates.Empty();
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
		for (const Route::RouteLine& line : route->GetLines()) {
			for (const Route::RouteRun& run : line.runs) {
				const StationRoadLink* link = run.fromStation ? run.fromStation->GetRoadLink(run.fromStop) : nullptr;
				if (link && link->stationNode) stationByNodeId.Add(link->stationNode->GetId(), run.fromStation);
			}
		}
	}

	TSet<const UClass*> reportedNoLineController; // 没指定线路控制器的载具蓝图，每个只说一次
	for (auto& [name, vehicle] : traffic->GetVehicles()) {
		if (!vehicle) continue;

		if (vehicle->GetRoute()) {
			// 公共交通车辆：不需要换算世界坐标，Tick里直接读vehicle->GetTransform()驱动，生成时原点占位。
			UClass* transitClass = ATransitVehicleElement::StaticClass();
			const string& blueprintPath = vehicle->GetBlueprintPath();
			UClass* loaded = nullptr; // 蓝图只加载这一次，transit类与物理表示的判断共用
			if (!blueprintPath.empty()) {
				FString path = UTF8_TO_TCHAR(blueprintPath.data());
				// 线路车的mod可以同时带可驾驶的BP(继承AVehicleElement，停车位驾驶和近处物理段用)：加载出来不是
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

			// 线路车在玩家附近（地形 LOD 范围内）换不换成物理表示，由它的载具蓝图自己声明：蓝图是 AVehicleElement 子类、
			// 并且指定了线路控制器(lineControllerClass)才换，换过去之后由那个控制器开。这里不看线路/站点/载具是什么
			// 种类，也不看线路走不走路网。
			UClass* physicsClass = (loaded && loaded->IsChildOf(AVehicleElement::StaticClass())) ? loaded : nullptr;
			if (physicsClass && GetDefault<AVehicleElement>(physicsClass)->lineControllerClass.Get()) {
				linePhysicsClasses.Add(physicsClass); // 控制器类被这个蓝图的默认对象引用着，跟着它一起不被GC
				FLineVehicleState state;
				state.vehicle = vehicle;
				state.name = transitName;
				state.bRouteVehicle = true;
				state.physicsClass = physicsClass;
				state.kinematic = transitElement;
				lineVehicleStates.Add(transitName, state);
			}
			else if (physicsClass && !reportedNoLineController.Contains(physicsClass)) {
				reportedNoLineController.Add(physicsClass);
				UE_LOG(LogTemp, Log, TEXT("UForeverTrafficFrameworkComponent::GenerateVehicles: 载具蓝图 %s 没有指定线路控制器(Line Controller Class)，用它的线路车一直是运动学的，不切物理。"),
					*physicsClass->GetPathName());
			}
			continue;
		}

		Room* room = vehicle->GetRoom();
		Building* building = room ? room->GetParentBuilding() : nullptr;
		if (!room || !building) continue; // 不是预置在停车位里的车辆(防御性写法)

		FVector parkedLocation;
		float worldYaw;
		GetParkedPose(vehicle, parkedLocation, worldYaw);
		float worldX = parkedLocation.X, worldY = parkedLocation.Y, worldZ = parkedLocation.Z;

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

	// 读档恢复行程状态：存档时正在行驶的车(快照在Vehicle上)接着开，存档时有Actor的车强制生成回来(TickComponent限量处理)。
	const double now = GetWorld()->GetTimeSeconds();
	for (FCarEntry& entry : carEntries) {
		Vehicle* vehicle = entry.vehicle;
		if (!vehicle) continue;
		if (vehicle->HasDriveSnapshot()) {
			const Vehicle::DriveSnapshot snapshot = vehicle->GetDriveSnapshot();
			vehicle->ClearDriveSnapshot();
			if (snapshot.path.size() >= 2 && snapshot.destination) {
				vehicle->SetCurrentRoom(nullptr);
				vehicle->BeginDrive(snapshot.path, snapshot.destination, now, snapshot.speed);
				if (vehicle->IsDriving()) {
					navTargets.Add(entry.name, snapshot.destination);
					randomTargets.Add(entry.name, false);
				}
			}
			else if (Room* reservedRoom = vehicle->GetReservedRoom()) {
				// 存档时路径还没规划出来(Controller刚收到指令)：从预订的车位重新规划。
				NavigateVehicleToSlot(entry.name, reservedRoom, vehicle->GetReservedSlotIndex());
			}
		}
		if (vehicle->WasSpawned()) {
			pendingRestoreVehicleSpawns.Add(entry.name);
			vehicle->SetWasSpawned(false);
		}
	}
}

void UForeverTrafficFrameworkComponent::SyncDrivesToCore() {
	if (!trafficRef || !mapRef) return;
	// 已入队未处理的导航请求先处理完(不限预算)，再落完待发的到达，免得存档丢请求。
	ProcessNavResults();
	while (pendingNavigations.Num() > 0) {
		const FNavRequest request = pendingNavigations[0];
		pendingNavigations.RemoveAt(0);
		StartVehicleNavigation(request);
	}
	ProcessNavResults();

	const double now = GetWorld()->GetTimeSeconds();
	for (FCarEntry& entry : carEntries) {
		Vehicle* vehicle = entry.vehicle;
		if (!vehicle || vehicle->GetRoute()) continue; // 线路车由时刻表推算，不存行驶状态
		AVehicleElement* element = entry.element.Get();
		vehicle->SetWasSpawned(element != nullptr);
		vehicle->ClearDriveSnapshot();
		if (element && element->IsPlayerControlled()) continue; // 玩家开的车由"正在开的车"恢复
		Vehicle::DriveSnapshot snapshot;
		if (vehicle->IsDriving()) {
			snapshot.destination = vehicle->GetDriveDestination();
			snapshot.speed = vehicle->GetDriveSpeed();
			vehicle->GetRemainingDrive(now, snapshot.path);
			vehicle->SetDriveSnapshot(snapshot);
		}
		else if (element) {
			Room** target = navTargets.Find(entry.name);
			TWeakObjectPtr<AVehicleCommandController>* driver = carDrivers.Find(entry.name);
			if (!target || !*target || !driver || !driver->IsValid()) continue;
			snapshot.destination = *target;
			snapshot.speed = (*driver)->maxSpeed * kUnspawnedDriveSpeedScale;
			TArray<FVector> remaining;
			bool bReverseFirst = false;
			if ((*driver)->GetRemainingWaypoints(remaining, &bReverseFirst) && remaining.Num() >= 2) {
				for (const FVector& point : remaining) snapshot.path.push_back({ static_cast<float>(point.X), static_cast<float>(point.Y), static_cast<float>(point.Z) });
				if (bReverseFirst) snapshot.path[0].reverse = true;
			}
			vehicle->SetDriveSnapshot(snapshot); // 路径为空=读档后从预订的车位重新规划
		}
	}
}

void UForeverTrafficFrameworkComponent::ClearDriveSnapshots() {
	for (FCarEntry& entry : carEntries) {
		if (entry.vehicle) entry.vehicle->ClearDriveSnapshot();
	}
}

bool UForeverTrafficFrameworkComponent::GetParkedPose(Vehicle* vehicle, FVector& outLocation, float& outYaw) const {
	Room* room = vehicle ? vehicle->GetRoom() : nullptr;
	Building* building = room ? room->GetParentBuilding() : nullptr;
	if (!room || !building) return false;
	float localX, localY;
	vehicle->GetParkingLocalPosition(localX, localY);
	float worldX, worldY;
	TrafficComputeWorldPosition(*building, room->GetPosX() + localX, room->GetPosY() + localY, worldX, worldY);
	outLocation = FVector(worldX, worldY, TRAFFIC_HEIGHT_EPSILON + building->GetFloorWorldZ(room->GetLayer()) * TRAFFIC_WORLD_SCALE);
	outYaw = FMath::RadiansToDegrees(building->GetRotation()) + vehicle->GetParkingRotationDegrees();
	return true;
}

UForeverTrafficFrameworkComponent::FCarEntry* UForeverTrafficFrameworkComponent::FindCarEntry(const Vehicle* vehicle) {
	for (FCarEntry& entry : carEntries) {
		if (entry.vehicle == vehicle) return &entry;
	}
	return nullptr;
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

	// 行进中的车(Core推算中)：车头对准当前路线的前进方向(倒车段则车尾朝前进方向)，不用上次记下的朝向，
	// 否则刚生成就要掉头。取剩余路径里第一段有长度的线段。
	if (entry.vehicle && entry.vehicle->IsDriving() && GetWorld()) {
		std::vector<Vehicle::DrivePoint> remaining;
		entry.vehicle->GetRemainingDrive(GetWorld()->GetTimeSeconds(), remaining);
		for (size_t i = 1; i < remaining.size(); i++) {
			const float dx = remaining[i].x - remaining[i - 1].x, dy = remaining[i].y - remaining[i - 1].y;
			if (dx * dx + dy * dy < 1.f) continue;
			entry.yaw = FMath::RadiansToDegrees(FMath::Atan2(dy, dx)) + (remaining[i - 1].reverse ? 180.f : 0.f);
			break;
		}
	}

	if (bCheckSpace) {
		// 行进中的车（Core推算中）路口附近不生成，等它开出去；停着的车不限制。
		if (entry.vehicle && entry.vehicle->IsDriving() && IsNearJunction(entry.location)) return nullptr;
		// 行进中的车车头朝向远离玩家(车头方向和"玩家→车"的水平方向夹角小于90度)：不生成，等它开远或转向朝着玩家再说；停着的车不限制。
		if (entry.vehicle && entry.vehicle->IsDriving()) {
			if (const APawn* playerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0)) {
				const FVector playerLocation = playerPawn->GetActorLocation();
				const FVector2D toVehicle(entry.location.X - playerLocation.X, entry.location.Y - playerLocation.Y);
				const float yawRad = FMath::DegreesToRadians(entry.yaw);
				if (FVector2D::DotProduct(toVehicle, FVector2D(FMath::Cos(yawRad), FMath::Sin(yawRad))) > 0.f) return nullptr;
			}
		}
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
	CORE_GUARD_BEGIN("UForeverTrafficFrameworkComponent::TickComponent")

	// 交通时钟放在所有早返回之前：暂停时DeltaTime为0不走，卡顿的大帧夹到0.25s免得租约被一帧耗光。
	trafficClock += FMath::Min(DeltaTime, 0.25f);
	drivers.RemoveAll([](const TWeakObjectPtr<AVehicleCommandController>& driver) { return !driver.IsValid(); });

	UpdatePhysicsHandoff();
	ProcessNavResults();
	ProcessPendingNavigations();
	UpdateRouteWatches(DeltaTime);

	// 读档后把存档时有Actor的车强制生成回来(不看距离)，在Core里行驶的接着交给Controller开。
	if (pendingRestoreVehicleSpawns.Num() > 0 && trafficRef) {
		int32 restored = 0;
		while (pendingRestoreVehicleSpawns.Num() > 0 && restored < FMath::Max(1, maxVehicleSpawnsPerFrame)) {
			const FString name = pendingRestoreVehicleSpawns.Pop(EAllowShrinking::No);
			FCarEntry* restoredEntry = nullptr;
			for (FCarEntry& candidate : carEntries) {
				if (candidate.name == name) { restoredEntry = &candidate; break; }
			}
			if (!restoredEntry || restoredEntry->element.IsValid() || !restoredEntry->vehicle) continue;
			if (restoredEntry->vehicle->GetParkState() == Vehicle::ParkState::Vanished) continue;
			if (SpawnCar(*restoredEntry, false) && restoredEntry->vehicle->IsDriving()) HandOffDriveToActor(*restoredEntry);
			restored++;
		}
	}

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
			// 被玩家停车挤掉位置、已从地图上消失的车：不生成，等下次寻路瞬移到目的车位。
			if (entry.vehicle && entry.vehicle->GetParkState() == Vehicle::ParkState::Vanished) continue;
			if (skipDrives.Contains(entry.name)) continue; // 跳时间虚拟时钟上的车：没有位置，不生成
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
			if (entry.respawnBlockedUntil > 0.0 && GetWorld()->GetTimeSeconds() < entry.respawnBlockedUntil) continue; // 刚脱困：先让Core带着它走远
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
		// 正在自己导航的车(测试导航)、被线路控制器持续驱动的车也不销毁：跑远了销毁会丢掉运行状态。
		if (!element->IsPlayerControlled() && IsDrivenByController(element)) {
			// 脱困：在开却长时间没前进(现实时间)，强制交回Core推算。暂停时不计。
			if (stuckTimeoutSeconds > 0.f && DeltaTime > 0.f && !GetWorld()->IsPaused()) {
				const double realNow = FPlatformTime::Seconds();
				// 等红灯/排队(被路口或前车挡着)时限放宽，免得排长队的车被交还Core后无视红灯。
				const AVehicleCommandController* activeDriver = FindActiveDriver(element);
				const float timeout = activeDriver && activeDriver->IsTrafficStopping() ?
					stuckTimeoutSeconds * FMath::Max(1.f, trafficWaitStuckFactor) : stuckTimeoutSeconds;
				if (entry.stuckSinceReal == 0.0 || FVector::Dist(entry.location, entry.stuckAnchor) > stuckMoveThreshold) {
					entry.stuckAnchor = entry.location;
					entry.stuckSinceReal = realNow;
				}
				else if (realNow - entry.stuckSinceReal >= timeout) {
					UE_LOG(LogTemp, Warning, TEXT("脱困：车辆 %s 超过%.0f秒没有前进，强制流式销毁并交回Core推算。"), *entry.name, timeout);
					entry.stuckSinceReal = realNow; // 交不回去(线路车等)就再等一个周期
					if (HandBackDriveToCore(entry)) {
						entry.respawnBlockedUntil = GetWorld()->GetTimeSeconds() + 30.0;
						entry.stuckSinceReal = 0.0;
						continue;
					}
					// 线路车没有导航目标交不回去：直接传送到下一站。
					if (TeleportLineVehicleToNextStop(entry.name)) {
						UE_LOG(LogTemp, Warning, TEXT("脱困：线路车 %s 传送到下一站。"), *entry.name);
						entry.stuckSinceReal = 0.0;
						continue;
					}
				}
			}
			else {
				entry.stuckSinceReal = 0.0;
			}
			// 脚本导航中的车：走远了把剩余路径交还Core继续推算并销毁Actor（和市民一样）；
			// 交不回去（线路车等）就保留。
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
	CORE_GUARD_END
}

AVehicleElement* UForeverTrafficFrameworkComponent::FindOrSpawnVehicle(const FString& vehicleName) {
	if (TObjectPtr<AVehicleElement>* found = activeVehicles.Find(vehicleName)) {
		if (AVehicleElement* element = found->Get()) return element;
		activeVehicles.Remove(vehicleName);
	}
	for (FCarEntry& entry : carEntries) {
		if (entry.name == vehicleName) return SpawnCar(entry);
	}
	if (IsSwitchableRouteVehicle(vehicleName)) return EnsureLinePhysicsElement(vehicleName);
	return nullptr;
}

void UForeverTrafficFrameworkComponent::FireVehicleEvent(Vehicle* vehicle, bool entering, const char* mode) {
	if (!vehicle) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	UForeverStoryFrameworkComponent* storyFramework = framework ? framework->GetStoryFramework() : nullptr;
	if (!storyFramework) return;

	// 只发给被进出的这辆车自己的script。
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

	const double enterStart = FPlatformTime::Seconds();
	AVehicleElement* vehicleElement = FindOrSpawnVehicle(vehicleName);
	const double spawnedAt = FPlatformTime::Seconds();
	if (!vehicleElement) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::ApplyEnterVehicle: 找不到车辆 %s"), *vehicleName);
		return;
	}

	APawn* currentPawn = controller->GetPawn();
	if (!currentPawn || currentPawn == vehicleElement) return;

	// 线路控制器在运动学段时车没有物理，先切回物理才能开。
	if (Vehicle* driveVehicle = vehicleElement->GetVehicle()) {
		if (FLineVehicleState* lineState = lineVehicleStates.Find(UTF8_TO_TCHAR(driveVehicle->GetName().c_str()))) {
			if (AVehicleCommandController* lineController = lineState->controller.Get()) lineController->ForcePhysicsMode();
		}
	}

	// 玩家要自己开了：这辆车不再算"停在某个房间里"，车位让出来，在途的预定取消，脚本导航的驾驶指令作废。
	if (Vehicle* coreVehicle = vehicleElement->GetVehicle()) {
		coreVehicle->ReleaseSlot();
		coreVehicle->ClearReservation();
		coreVehicle->SetCurrentRoom(nullptr);
		const FString enteredName = UTF8_TO_TCHAR(coreVehicle->GetName().c_str());
		if (TWeakObjectPtr<AVehicleCommandController>* driver = carDrivers.Find(enteredName)) {
			if (driver->IsValid()) driver->Get()->ClearCommands();
		}
		navTargets.Remove(enteredName);
		// 这辆车正在执行组织任务：任务取消，员工下车，组织脚本收到task_failed。
		AForeverFrameworkActor* taskFramework = Cast<AForeverFrameworkActor>(GetOwner());
		if (UForeverPopulaceFrameworkComponent* taskPopulace = taskFramework ? taskFramework->GetPopulaceFramework() : nullptr) {
			taskPopulace->OnTaskVehicleTaken(coreVehicle);
		}
	}

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
	const double possessStart = FPlatformTime::Seconds();
	controller->Possess(vehicleElement);
	const double possessEnd = FPlatformTime::Seconds();
	if (!bSilent) FireVehicleEvent(vehicleElement->GetVehicle(), true, "drive");
	const double enterEnd = FPlatformTime::Seconds();
	if (enterEnd - enterStart > 0.02) {
		UE_LOG(LogTemp, Warning, TEXT("ApplyEnterVehicle %s: 总 %.1f ms (FindOrSpawnVehicle %.1f, 隐藏原pawn等 %.1f, Possess %.1f, 事件 %.1f)"),
			*vehicleName, (enterEnd - enterStart) * 1000.0, (spawnedAt - enterStart) * 1000.0,
			(possessStart - spawnedAt) * 1000.0, (possessEnd - possessStart) * 1000.0, (enterEnd - possessEnd) * 1000.0);
	}
}

ATransitVehicleElement* UForeverTrafficFrameworkComponent::FindTransitElement(const FString& vehicleName) const {
	const TObjectPtr<ATransitVehicleElement>* found = activeTransitVehicles.Find(vehicleName);
	return found ? found->Get() : nullptr;
}

void UForeverTrafficFrameworkComponent::ApplyBoardVehicle(const FString& vehicleName, APlayerController* controller, bool bSilent) {
	if (!controller) return;

	// 能切物理的线路车：搭乘的是物理车（玩家一定在地形 LOD 范围内），线路控制器继续按引用开。
	if (IsSwitchableRouteVehicle(vehicleName)) {
		AVehicleElement* lineElement = EnsureLinePhysicsElement(vehicleName);
		APawn* riderPawn = controller->GetPawn();
		if (!lineElement || !riderPawn || riderPawn == lineElement) {
			UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::ApplyBoardVehicle: 线路车的物理表示不可用 %s"), *vehicleName);
			return;
		}
		if (ACitizenElement* citizenElement = Cast<ACitizenElement>(riderPawn)) {
			citizenElement->SetDespawnExempt(true);
		}
		riderPawn->SetActorHiddenInGame(true);
		riderPawn->SetActorEnableCollision(false);
		lineElement->SetPreviousPawn(riderPawn);
		lineElement->SetRideMode(true);
		controller->Possess(lineElement);
		if (!bSilent) FireVehicleEvent(lineElement->GetVehicle(), true, "ride");
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
		bRide = driven->IsRideMode(); // 搭乘物理线路车：和ATransitVehicleElement搭乘一样不做遮挡判定
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

	if (!bRide) ParkPlayerVehicle(vehicle, vehicleElement);

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

void UForeverTrafficFrameworkComponent::ParkPlayerVehicle(Vehicle* vehicle, const AActor* vehicleActor) {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	Room* room = player ? player->GetCurrentRoom() : nullptr;
	Building* building = room ? room->GetParentBuilding() : nullptr;
	if (!vehicle || !vehicleActor || !building || room->GetParkingSlots().empty()) return;

	// 车辆位姿换成建筑局部坐标(地图单位、数学角)后和车位比：朝向相反、车中心在矩形内、车位没停车。
	const FVector location = vehicleActor->GetActorLocation();
	const std::pair<float, float> local = building->WorldToLocal(location.X / TRAFFIC_WORLD_SCALE, location.Y / TRAFFIC_WORLD_SCALE);
	const float localYaw = vehicleActor->GetActorRotation().Yaw - FMath::RadiansToDegrees(building->GetRotation());
	const int slotIndex = room->FindSlotContaining(local.first, local.second, localYaw);
	if (slotIndex < 0) return;
	ParkingSlot* slot = room->GetParkingSlots()[slotIndex];
	if (!slot->Accepts(vehicle->GetCategory())) return;

	// 车位上原本有车预定了(在路上)：先把预定摘给玩家的车，再给它另找车位。
	Vehicle* displaced = slot->reservedBy;
	vehicle->BindSlot(room, slotIndex);
	if (displaced && displaced != vehicle) {
		displaced->ClearReservation();
		RerouteOrVanishVehicle(displaced, room);
	}
}

void UForeverTrafficFrameworkComponent::RerouteOrVanishVehicle(Vehicle* other, Room* room) {
	const FString name = UTF8_TO_TCHAR(other->GetName().c_str());
	FCarEntry* entry = FindCarEntry(other);
	const bool bRandom = randomTargets.FindRef(name);

	const int32 newIndex = room->FindFreeSlot(other->GetCategory());
	if (newIndex >= 0 && other->ReserveSlot(room, newIndex)) {
		ParkingSlot* newSlot = other->GetReservedSlot();
		Node* node = newSlot ? newSlot->navigationNode : nullptr;
		if (node) {
			AVehicleElement* element = entry ? entry->element.Get() : nullptr;
			if (element) {
				// 有Actor：Controller从当前位置重新规划到新车位。
				if (AVehicleCommandController* controller = GetOrCreateCarDriver(name, element)) {
					controller->ClearCommands();
					controller->PushCommand(FVehicleCommand::MakeNavigate(node->GetId()));
					return;
				}
			}
			else if (other->IsDriving()) {
				// Core推算中：从当前推算位置重新规划。
				float x, y, z, yaw;
				other->GetTransform(x, y, z, yaw);
				const float yawRad = FMath::DegreesToRadians(yaw);
				Map::VehicleRoutePlan plan = mapRef->PlanVehicleRouteToNode(nullptr,
					x / TRAFFIC_WORLD_SCALE, y / TRAFFIC_WORLD_SCALE, node->GetId(), true, FMath::Cos(yawRad), FMath::Sin(yawRad));
				if (plan.status == Map::VehicleRoutePlan::OK && plan.points.size() >= 2) {
					std::vector<Vehicle::DrivePoint> path;
					for (const Map::NavPoint& p : plan.points) {
						path.push_back({ p.x * TRAFFIC_WORLD_SCALE, p.y * TRAFFIC_WORLD_SCALE, p.z * TRAFFIC_WORLD_SCALE + TRAFFIC_HEIGHT_EPSILON });
					}
					other->BeginDrive(path, room, GetWorld()->GetTimeSeconds(),
						GetDefault<AVehicleCommandController>()->maxSpeed * kUnspawnedDriveSpeedScale);
					return;
				}
			}
		}
		other->ClearReservation();
	}

	// 同房间没有别的空车位(或重新规划不出来)：这辆车从地图上消失，视为已到达这个房间。
	if (entry) {
		if (TWeakObjectPtr<AVehicleCommandController>* driver = carDrivers.Find(name)) {
			if (driver->IsValid()) driver->Get()->ClearCommands();
			carDrivers.Remove(name);
		}
		if (AVehicleElement* element = entry->element.Get()) {
			activeVehicles.Remove(name);
			entry->element = nullptr;
			// 刚生成的Chaos载具物理代理没建好就销毁会崩，还没到最短存活时间的先隐藏、到点再销毁。
			const float remaining = minVehicleLifetime - static_cast<float>(GetWorld()->GetTimeSeconds() - entry->spawnedAt);
			if (remaining > 0.f) {
				element->SetActorHiddenInGame(true);
				element->SetActorEnableCollision(false);
				TWeakObjectPtr<AVehicleElement> weakElement(element);
				FTimerHandle handle;
				GetWorld()->GetTimerManager().SetTimer(handle, [weakElement]() {
					if (weakElement.IsValid()) weakElement->Destroy();
				}, remaining, false);
			}
			else {
				element->Destroy();
			}
		}
	}
	other->EndDrive();
	other->SetVanished(room);
	navTargets.Remove(name);
	pendingNavResults.Add({ other, room, true, bRandom });
	UE_LOG(LogTemp, Log, TEXT("车辆 %s 的目的车位被玩家的车占了且同房间没有空位，已从地图上消失"), *name);
}

void UForeverTrafficFrameworkComponent::BuildRouteDebugMesh(Traffic* traffic) {
	if (!traffic) return;

	AActor* owner = GetOwner();
	if (!owner) return;

	if (!bShowRouteDebug) {
		for (auto& [type, mesh] : routeDebugMeshes) if (mesh) mesh->ClearMeshSection(0);
		return;
	}

	// 每种stationType一组几何，不认识任何具体类型。
	struct FRouteGeometry { TArray<FVector> vertices; TArray<int32> triangles; };
	TMap<FString, FRouteGeometry> geometry;

	for (auto& [routeName, route] : traffic->GetRoutes()) {
		if (!route) continue;
		FRouteGeometry& target = geometry.FindOrAdd(UTF8_TO_TCHAR(route->GetStationType().c_str()));
		TArray<FVector>* vertices = &target.vertices;
		TArray<int32>* triangles = &target.triangles;

		// 多条线路共用同一段路径只画一次。
		TSet<const TrackPath*> drawn;
		for (const Route::RouteLine& line : route->GetLines()) {
			for (const Route::RouteRun& run : line.runs) {
				for (const Route::RoutePiece& piece : run.pieces) {
					if (!piece.path || drawn.Contains(piece.path.get())) continue;
					drawn.Add(piece.path.get());

					TArray<FVector> points;
					SampleRouteEdge(*piece.path, points);
					for (int32 i = 0; i + 1 < points.Num(); i++) {
						AppendRouteEdgeRibbon(*vertices, *triangles, points[i], points[i + 1], ROUTE_DEBUG_EDGE_HALF_WIDTH);
					}
					AppendRouteArrow(*vertices, *triangles, points, false);
				}
			}
		}
	}

	for (auto& [stationName, station] : traffic->GetStations()) {
		if (!station) continue;
		FRouteGeometry& target = geometry.FindOrAdd(UTF8_TO_TCHAR(station->GetStationType().c_str()));
		TArray<FVector>* vertices = &target.vertices;
		TArray<int32>* triangles = &target.triangles;

		for (const vector<StationPort>* ports : { &station->GetInlets(), &station->GetOutlets(), &station->GetStops() }) {
			for (const StationPort& port : *ports) {
				FVector world(port.x * TRAFFIC_WORLD_SCALE, port.y * TRAFFIC_WORLD_SCALE, port.z * TRAFFIC_WORLD_SCALE);
				AppendRouteStationBox(*vertices, *triangles, FVector2D(world.X, world.Y),
					ROUTE_DEBUG_NODE_HALF_SIZE, world.Z, world.Z + ROUTE_DEBUG_HEIGHT);
			}
		}
	}

	// 上次画过、这次没有的类型清掉。
	for (auto& [type, mesh] : routeDebugMeshes) if (mesh && !geometry.Contains(type)) mesh->ClearMeshSection(0);

	for (auto& [type, data] : geometry) {
		// 创建方式同ForeverRoadnetFrameworkComponent::BuildNavigationDebugMesh。
		TObjectPtr<UProceduralMeshComponent>& mesh = routeDebugMeshes.FindOrAdd(type);
		if (!mesh) {
			mesh = NewObject<UProceduralMeshComponent>(owner);
			mesh->SetupAttachment(owner->GetRootComponent());
			mesh->RegisterComponent();
			owner->AddInstanceComponent(mesh);
		}
		// 颜色按类型名哈希取色相：同一个类型每次都是同一个颜色，不同类型大概率分得开。pureBaseMaterial缺失时不上材质。
		TObjectPtr<UMaterialInstanceDynamic>& material = routeDebugMaterials.FindOrAdd(type);
		if (!material && pureBaseMaterial) {
			material = UMaterialInstanceDynamic::Create(pureBaseMaterial, this);
			const uint8 hue = static_cast<uint8>(FCrc::StrCrc32(*type) % 256);
			material->SetVectorParameterValue(TEXT("Color"), FLinearColor::MakeFromHSV8(hue, 255, 255));
		}
		mesh->ClearMeshSection(0);
		if (data.triangles.Num() == 0) continue;
		mesh->CreateMeshSection(0, data.vertices, data.triangles,
			TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		if (material) mesh->SetMaterial(0, material);
	}
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
	guide = FGuide();
	guide.target = target;
	RefreshGuide();
}

void UForeverTrafficFrameworkComponent::StartGuideToGreen(Green* green, int circleIndex, float endX, float endY) {
	guide = FGuide();
	guide.green = green;
	guide.greenCircle = circleIndex;
	guide.greenEndX = endX;
	guide.greenEndY = endY;
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
	if (!guide.target && !guide.green) return;
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
		if (guide.green) {
			plan = map->PlanPedestrianRouteToGreen(corePlayer ? corePlayer->GetCurrentRoom() : nullptr,
				feet.X / TRAFFIC_WORLD_SCALE, feet.Y / TRAFFIC_WORLD_SCALE, guide.green, guide.greenCircle,
				guide.greenEndX, guide.greenEndY);
		}
		else {
			plan = map->PlanPedestrianRoute(corePlayer ? corePlayer->GetCurrentRoom() : nullptr,
				feet.X / TRAFFIC_WORLD_SCALE, feet.Y / TRAFFIC_WORLD_SCALE, guide.target);
		}
	}
	else if (const AVehicleElement* vehicle = Cast<AVehicleElement>(pawn)) {
		if (vehicle->IsRideMode()) { HideGuideLine(); return; } // 线路车乘客：不画
		// 开车：车道导航。先试直接到房间；车到不了就去建筑1楼外部连接点附近的车道。
		const FVector location = pawn->GetActorLocation();
		const FVector forward = pawn->GetActorForwardVector();
		const float x = location.X / TRAFFIC_WORLD_SCALE, y = location.Y / TRAFFIC_WORLD_SCALE;
		Room* startRoom = vehicle->GetVehicle() ? vehicle->GetVehicle()->GetCurrentRoom() : nullptr;
		if (guide.green) {
			// 车进不了绿化区：规划到圆心最近的车道点，线终止在那里(下车后刷新成步行线)。
			bOutside = true;
			const std::vector<Green::Circle>& circles = guide.green->GetCircles();
			if (guide.greenCircle >= 0 && guide.greenCircle < static_cast<int>(circles.size())) {
				const Green::Circle& circle = circles[guide.greenCircle];
				plan = map->PlanVehicleRouteToPoint(startRoom, x, y, circle.x, circle.y, true, forward.X, forward.Y);
			}
		}
		else plan = map->PlanVehicleRoute(startRoom, x, y, guide.target);
		if (!guide.green && (plan.status != Map::VehicleRoutePlan::OK || plan.points.empty())) {
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
			*FString(UTF8_TO_TCHAR((guide.target ? guide.target->GetAddress() : guide.green->GetAddress()).c_str())),
			static_cast<int32>(plan.status));
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
	if ((!guide.target && !guide.green) || guide.path.Num() < 2) return false;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* corePlayer = framework ? framework->GetPlayer() : nullptr;
	APlayerController* controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* pawn = controller ? controller->GetPawn() : nullptr;
	if (!pawn) return false;

	// 到达目标房间：导航完成（线终止在外部连接点的不销毁，要下车刷新后再走进房间）。
	if (!guide.bEndsAtOutside && guide.target && corePlayer && corePlayer->GetCurrentRoom() == guide.target) {
		guide = FGuide();
		return false;
	}
	// 到达绿化区圆内终点：按水平距离判定(线终止在车道上的不算，要下车刷新成步行线后再走)。
	if (!guide.bEndsAtOutside && guide.green) {
		const FVector end = guide.path.Last();
		const float arrive = guideArriveDistance * TRAFFIC_WORLD_SCALE;
		if (FVector::DistSquared2D(pawn->GetActorLocation(), end) <= arrive * arrive) {
			guide = FGuide();
			return false;
		}
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
		if ((!guide.target && !guide.green) || guide.path.Num() < 2) return false;
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
		debugf(DebugLevel::Warning, "UForeverTrafficFrameworkComponent::GetOrCreateTrackISM: failed to load track mesh %s.\n", meshPath.c_str());
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

		// 路径图里的每条路径(站内连线+站间连线，已去重)按每一段自带的mesh/meshUnit铺；mesh为空的段(站内直通等)不铺。
		int32 instanceCount = 0;
		for (const shared_ptr<TrackPath>& path : route->GetTrackPaths()) {
			if (!path) continue;
			for (size_t segIndex = 0; segIndex < path->connections.size() && segIndex < path->segments.size(); segIndex++) {
				Connection* conn = path->connections[segIndex];
				const TrackSegment& segment = path->segments[segIndex];
				float unit = segment.meshUnit;
				if (!conn || segment.mesh.empty() || unit <= 0.f) continue;
				UInstancedStaticMeshComponent* ism = GetOrCreateTrackISM(segment.mesh);
				if (!ism) continue;

				// 同BuildRoadInstances：n夹在[0.75,1.5]*unit之内(ROAD_TILE_SCALE_MIN/MAX)，scaleAlong把每块拉伸到实际段长。
				float segLen = conn->CalcDistance();
				int nLow = FMath::CeilToInt(segLen / (1.5f * unit));
				int nHigh = FMath::FloorToInt(segLen / (0.75f * unit));
				int n = (nLow <= nHigh && nLow > 0) ? FMath::Clamp(FMath::RoundToInt(segLen / unit), nLow, nHigh) : FMath::Max(1, FMath::RoundToInt(segLen / unit));
				float scaleAlong = (segLen / n) / unit;

				for (int k = 0; k < n; k++) {
					float t0 = static_cast<float>(k) / n;
					float t1 = static_cast<float>(k + 1) / n;
					Node start = conn->GetPoint(t0);
					Node end = conn->GetPoint(t1);
					FVector startW(start.GetX() * TRAFFIC_WORLD_SCALE, start.GetY() * TRAFFIC_WORLD_SCALE, start.GetZ() * TRAFFIC_WORLD_SCALE);
					FVector endW(end.GetX() * TRAFFIC_WORLD_SCALE, end.GetY() * TRAFFIC_WORLD_SCALE, end.GetZ() * TRAFFIC_WORLD_SCALE);

					// 轨道mesh沿局部+X延伸、轴心在中心：摆在块的中点，朝向取起点->终点的3D方向。
					FVector fwd = (endW - startW).GetSafeNormal();
					if (fwd.IsNearlyZero()) fwd = FVector::ForwardVector;
					ism->AddInstance(FTransform(fwd.Rotation(), (startW + endW) * 0.5f, FVector(scaleAlong, 1.f, 1.f)));
					instanceCount++;
				}
			}
		}
		debugf(DebugLevel::Log, "UForeverTrafficFrameworkComponent::BuildTracks: route %s laid %d track instance(s).\n",
			route->GetName().c_str(), instanceCount);
	}
}

void UForeverTrafficFrameworkComponent::BuildPiers(Map* map, Traffic* traffic) {
	if (!map || !traffic) return;

	// 桥墩和轨道共用trackMeshInstances(同一套ISM缓存)；清实例由BuildTracks开头统一做，所以必须在它之后调用。
	// mesh空串=Asset/Common默认圆柱。
	const std::string defaultMesh = "/Game/Asset/Common/Mesh/Cylinder.Cylinder";
	for (auto& [routeName, route] : traffic->GetRoutes()) {
		if (!route) continue;
		int32 pierCount = 0;
		for (const Route::Pier& pier : route->GetPiers()) {
			UInstancedStaticMeshComponent* ism = GetOrCreateTrackISM(pier.mesh.empty() ? defaultMesh : pier.mesh);
			if (!ism || !ism->GetStaticMesh()) continue;

			// 墩底贴地形(水面不管，取到的就是水底)，墩顶贴请求点高度；太矮的Core已经过滤。
			float baseZ = map->SampleHeight(pier.x, pier.y);
			float height = (pier.topZ - baseZ) * TRAFFIC_WORLD_SCALE;
			if (height <= 0.f) continue;

			// 按mesh包围盒适配尺寸(不假设资产原尺寸和轴心)：水平直径=2*radius，竖直正好撑满墩高，包围盒中心对齐墩的中心。
			const FBoxSphereBounds bounds = ism->GetStaticMesh()->GetBounds();
			float meshDiameter = FMath::Max(bounds.BoxExtent.X, bounds.BoxExtent.Y) * 2.f;
			float meshHeight = bounds.BoxExtent.Z * 2.f;
			if (meshDiameter < KINDA_SMALL_NUMBER || meshHeight < KINDA_SMALL_NUMBER) continue;
			float horizontalScale = pier.radius * 2.f * TRAFFIC_WORLD_SCALE / meshDiameter;
			FVector scale(horizontalScale, horizontalScale, height / meshHeight);
			FVector center(pier.x * TRAFFIC_WORLD_SCALE, pier.y * TRAFFIC_WORLD_SCALE, (baseZ + pier.topZ) * 0.5f * TRAFFIC_WORLD_SCALE);
			ism->AddInstance(FTransform(FRotator::ZeroRotator, center - bounds.Origin * scale, scale));
			pierCount++;
		}
		if (pierCount > 0) {
			debugf(DebugLevel::Log, "UForeverTrafficFrameworkComponent::BuildPiers: route %s placed %d pier instance(s).\n", route->GetName().c_str(), pierCount);
		}
	}
}

AVehicleElement* UForeverTrafficFrameworkComponent::FindVehicleElement(const FString& vehicleName) const {
	const TObjectPtr<AVehicleElement>* found = activeVehicles.Find(vehicleName);
	return found ? found->Get() : nullptr;
}


// ---------------------------------------------------------------------------------------------
// 线路车的 Controller 驱动与远近切换
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

UForeverTrafficFrameworkComponent::FLineVehicleState* UForeverTrafficFrameworkComponent::FindOrAddLineVehicleState(const FString& vehicleName) {
	if (FLineVehicleState* found = lineVehicleStates.Find(vehicleName)) return found;
	// 停着的车第一次被 Controller 驱动：登记状态（没有时刻表、没有运动学表示）。
	for (FCarEntry& entry : carEntries) {
		if (entry.name == vehicleName && entry.vehicle) {
			FLineVehicleState state;
			state.vehicle = entry.vehicle;
			state.name = vehicleName;
			state.bNoPhysics = true;
			return &lineVehicleStates.Add(vehicleName, state);
		}
	}
	return nullptr;
}

bool UForeverTrafficFrameworkComponent::IsDrivenByController(const AVehicleElement* element) const {
	return FindActiveDriver(element) != nullptr;
}

const AVehicleCommandController* UForeverTrafficFrameworkComponent::FindActiveDriver(const AVehicleElement* element) const {
	if (!element || !element->GetVehicle()) return nullptr;
	const FString elementName = UTF8_TO_TCHAR(element->GetVehicle()->GetName().c_str());
	if (const TWeakObjectPtr<AVehicleCommandController>* driver = carDrivers.Find(elementName)) {
		if (driver->IsValid() && (*driver)->HasCommands()) return driver->Get();
	}
	const FLineVehicleState* state = lineVehicleStates.Find(elementName);
	if (!state) return nullptr;
	const AVehicleCommandController* controller = state->controller.Get();
	return controller && controller->HasCommands() ? controller : nullptr;
}

AVehicleElement* UForeverTrafficFrameworkComponent::FindLinePhysicsElement(const FString& vehicleName) const {
	const FLineVehicleState* state = lineVehicleStates.Find(vehicleName);
	return state ? state->element.Get() : nullptr;
}

bool UForeverTrafficFrameworkComponent::IsSwitchableRouteVehicle(const FString& vehicleName) const {
	const FLineVehicleState* state = lineVehicleStates.Find(vehicleName);
	return state && state->bRouteVehicle && !state->bNoPhysics;
}

AVehicleElement* UForeverTrafficFrameworkComponent::EnsureLinePhysicsElement(const FString& vehicleName) {
	FLineVehicleState* state = lineVehicleStates.Find(vehicleName);
	if (!state) return nullptr;
	if (AVehicleElement* element = state->element.Get()) return element;
	if (state->bRouteVehicle && !state->bNoPhysics && HandoffToPhysics(*state)) return state->element.Get();
	return nullptr;
}

void UForeverTrafficFrameworkComponent::RegisterDriver(AVehicleCommandController* driver) {
	if (driver) drivers.AddUnique(driver);
}

void UForeverTrafficFrameworkComponent::WarmVehicleClasses() {
	UWorld* world = GetWorld();
	if (!world) return;
	TSet<FString> donePaths;
	for (const FCarEntry& entry : carEntries) {
		if (entry.blueprintPath.IsEmpty() || donePaths.Contains(entry.blueprintPath)) continue;
		donePaths.Add(entry.blueprintPath);
		UClass* vehicleClass = LoadClass<AVehicleElement>(nullptr, *entry.blueprintPath);
		if (!vehicleClass) continue; // 加载失败的警告留给真正SpawnCar时报
		warmedVehicleClasses.Add(vehicleClass);

		// 临时生成一辆再销毁：把组件注册/物理体创建(骨骼网格物理资产、Chaos载具)的一次性开销摊到加载界面。
		// 放在车位上方很高处，生成后同一帧销毁(不会Tick)，不会和真车重叠或产生任何游戏效果；没调Init，车辆Actor的vehicle指针为空。
		FActorSpawnParameters spawnParams;
		spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AVehicleElement* temp = world->SpawnActor<AVehicleElement>(vehicleClass, entry.location + FVector(0.f, 0.f, 500000.f), FRotator::ZeroRotator, spawnParams)) {
			temp->Destroy();
		}
	}
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

AVehicleCommandController* UForeverTrafficFrameworkComponent::SpawnLineController(AVehicleElement* element) {
	// 每种载具用自己的线路控制器，由载具蓝图指定；没指定的车不能跑线路。
	UClass* controllerClass = element ? element->lineControllerClass.Get() : nullptr;
	return controllerClass ? SpawnDriver(controllerClass, element) : nullptr;
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
		return terrain->IsInsideLodLevel(position, self->linePhysicsLodLevel, bCurrentlyPhysics ? 1.f : self->linePhysicsEnterScale);
	});
	controller->Possess(element);
	return controller;
}

AVehicleCommandController* UForeverTrafficFrameworkComponent::GetOrCreateLineController(FLineVehicleState& state) {
	if (AVehicleCommandController* existing = state.controller.Get()) return existing;
	AVehicleElement* element = state.element.Get();
	if (!element) return nullptr;
	AVehicleCommandController* controller = SpawnLineController(element);
	state.controller = controller;
	return controller;
}

void UForeverTrafficFrameworkComponent::CollectLineLoops(const std::string& vehicleType, TArray<TArray<int32>>& outLoops) const {
	if (!trafficRef || vehicleType.empty()) return;
	for (const auto& [routeName, route] : trafficRef->GetRoutes()) {
		// 只要"用这种载具跑的线路"，不认线路/站点的种类名。站点没接上路网的线路 GetLineStopNodeIds 给空，自然跳过。
		if (!route || route->GetVehicleType() != vehicleType) continue;
		for (size_t lineIdx = 0; lineIdx < route->GetLines().size(); lineIdx++) {
			std::vector<int> ids = route->GetLineStopNodeIds(lineIdx);
			if (ids.size() < 2) continue;
			TArray<int32> loop;
			for (int id : ids) loop.Add(id);
			outLoops.Add(MoveTemp(loop));
		}
	}
}

void UForeverTrafficFrameworkComponent::UpdatePhysicsHandoff() {
	if (lineVehicleStates.Num() == 0) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	if (!framework || !framework->IsInitialGenerationDone()) return;
	UForeverTerrainFrameworkComponent* terrain = framework->GetTerrainFramework();
	if (!terrain || !terrain->IsTerrainGenerated()) return;

	// 每帧最多切换一辆（生成物理车是整车 Chaos 组件，成本高）。
	for (TPair<FString, FLineVehicleState>& pair : lineVehicleStates) {
		FLineVehicleState& state = pair.Value;
		if (!state.bRouteVehicle || state.bDetached || state.bNoPhysics || !state.vehicle) continue;

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
		const bool bInside = terrain->IsInsideLodLevel(position, linePhysicsLodLevel, bPhysics ? 1.f : linePhysicsEnterScale);
		if (bInside && !bPhysics) {
			if (HandoffToPhysics(state)) return;
		}
		else if (!bInside && bPhysics) {
			if (element->IsPlayerControlled()) continue; // 玩家在开/搭乘：不能收回
			HandoffToKinematic(state);
			return;
		}
	}
}

bool UForeverTrafficFrameworkComponent::HandoffToPhysics(FLineVehicleState& state) {
	Vehicle* vehicle = state.vehicle;
	Route* route = vehicle ? vehicle->GetRoute() : nullptr;
	if (!route || !state.physicsClass || !GetWorld()) {
		state.bNoPhysics = true;
		return false;
	}
	// 时刻表按游戏秒算；初速度和剩余停站时间要真实秒，按当前实际的时间倍率换算。
	AForeverFrameworkActor* scheduleFramework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* schedulePlayer = scheduleFramework ? scheduleFramework->GetPlayer() : nullptr;
	const double gameSecondsPerRealSecond = 60.0 * (schedulePlayer ? schedulePlayer->GetTimeFlowRatio() : 10.0);
	const Route::ScheduleState schedule = route->QueryVehicleState(vehicle, gameSecondsPerRealSecond);
	if (!schedule.valid) return false;
	std::vector<int> ids = route->GetLineStopNodeIds(static_cast<size_t>(schedule.line));
	if (ids.size() < 2) {
		// 交接给控制器的内容目前是"一圈站点的路网节点"：站点没接上路网的线路给不出来，这辆车就一直走时刻表。
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::HandoffToPhysics: 线路车 %s 的蓝图指定了线路控制器，但它的线路站点没有接上路网，无法交给控制器，保持运动学。"), *state.name);
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
	AVehicleCommandController* controller = SpawnLineController(element);
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
		// 这一腿正在站间路上：控制器沿这一腿的原路线(出发站->下一站)走，不要从当前位置重新规划。
		command.legFromNode = nodes[schedule.legIndex % count];
	}
	controller->PushCommand(command);
	return true;
}

void UForeverTrafficFrameworkComponent::HandoffToKinematic(FLineVehicleState& state, bool bSyncProgress) {
	AVehicleElement* element = state.element.Get();
	// 先把时刻表进度对齐到实车当前位置（必须在ClearCommands之前读控制器状态）：物理车比名义速度慢，
	// 不对齐的话交还瞬间时刻表算出来的位置早已领先一大截，车会"瞬移"到玩家附近。
	if (AVehicleCommandController* controller = bSyncProgress ? state.controller.Get() : nullptr) {
		int32 legIndex;
		float legFraction, dwellFraction;
		bool bDwelling;
		if (state.vehicle && controller->QueryLoopProgress(legIndex, legFraction, bDwelling, dwellFraction)) {
			if (Route* route = state.vehicle->GetRoute()) {
				route->SyncVehicleProgress(state.vehicle, legIndex, legFraction, bDwelling, dwellFraction);
			}
		}
	}
	if (AVehicleCommandController* controller = state.controller.Get()) {
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

bool UForeverTrafficFrameworkComponent::TeleportLineVehicleToNextStop(const FString& vehicleName) {
	FLineVehicleState* state = lineVehicleStates.Find(vehicleName);
	if (!state || !state->bRouteVehicle || state->bDetached || !state->vehicle) return false;
	AVehicleElement* element = state->element.Get();
	AVehicleCommandController* controller = state->controller.Get();
	if (!element || !controller || element->IsPlayerControlled()) return false;
	Route* route = state->vehicle->GetRoute();
	if (!route) return false;
	int32 legIndex;
	float legFraction, dwellFraction;
	bool bDwelling;
	if (!controller->QueryLoopProgress(legIndex, legFraction, bDwelling, dwellFraction) || bDwelling) return false;
	// 这一腿正驶向第legIndex+1站：进度设成这一腿走完(=到站)，再收回物理Actor；位置由时刻表给出。
	if (!route->SyncVehicleProgress(state->vehicle, legIndex, 1.f, false, 0.f)) return false;
	HandoffToKinematic(*state, false);
	return true;
}

void UForeverTrafficFrameworkComponent::ResyncLineVehiclesAfterTimeSkip() {
	for (TPair<FString, FLineVehicleState>& pair : lineVehicleStates) {
		FLineVehicleState& state = pair.Value;
		if (!state.bRouteVehicle || state.bDetached || !state.element.IsValid()) continue;
		if (state.element->IsPlayerControlled()) continue; // 玩家在开/搭乘：不动
		HandoffToKinematic(state, false); // 之后UpdatePhysicsHandoff会按时刻表位置重新决定要不要切物理
	}
}

void UForeverTrafficFrameworkComponent::StartVehicleLineRun(const FString& vehicleName) {
	AVehicleElement* element = IsSwitchableRouteVehicle(vehicleName) ? EnsureLinePhysicsElement(vehicleName) : FindOrSpawnVehicle(vehicleName);
	if (!element) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::StartVehicleLineRun: 找不到车辆 %s"), *vehicleName);
		return;
	}
	if (!element->lineControllerClass.Get()) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::StartVehicleLineRun: 车辆 %s 的蓝图没有指定线路控制器(Line Controller Class)，不能按线路运行"), *vehicleName);
		return;
	}
	FLineVehicleState* state = FindOrAddLineVehicleState(vehicleName);
	if (!state) return;
	state->element = element;

	TArray<TArray<int32>> loops;
	CollectLineLoops(state->vehicle ? state->vehicle->GetType() : std::string(), loops);
	if (loops.Num() == 0) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::StartVehicleLineRun: 没有使用车辆 %s 这种载具、并且站点接上路网的线路"), *vehicleName);
		return;
	}
	AVehicleCommandController* controller = GetOrCreateLineController(*state);
	if (!controller) return;

	CORE_RANDOM_SCOPE("core.traffic");
	const TArray<int32>& loop = loops[GetRandom(loops.Num())];
	const int32 stopIndex = GetRandom(loop.Num());
	controller->ClearCommands();
	controller->PushCommand(FVehicleCommand::MakeNavigate(loop[stopIndex]));
	FVehicleCommand loopCommand = FVehicleCommand::MakeLoop(loop, (stopIndex + 1) % loop.Num());
	loopCommand.bStartAtStop = true;               // 导航到这个站之后，接着像刚进站一样先停靠再出发
	loopCommand.startDwellSeconds = controller->GetStopDwellSeconds(loop[stopIndex]);
	loopCommand.bFireEnterAtStart = true;
	controller->PushCommand(loopCommand);

	if (state->bRouteVehicle) {
		// 线路车退出时刻表，之后一直由 Controller 持有（LOD 外靠 Controller 的运动学模式）。
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

void UForeverTrafficFrameworkComponent::NavigateVehicleToSlot(const FString& vehicleName, Room* room, int32 slotIndex) {
	FNavRequest request;
	request.vehicleName = vehicleName;
	request.presetRoom = room;
	request.presetSlot = slotIndex;
	pendingNavigations.Add(request);
}

bool UForeverTrafficFrameworkComponent::GetVehicleRoutePath(Vehicle* vehicle, TArray<FVector>& outPath) {
	outPath.Reset();
	if (!vehicle || !GetWorld()) return false;
	const FString name = UTF8_TO_TCHAR(vehicle->GetName().c_str());
	if (const FSkipDrive* drive = skipDrives.Find(name)) {
		for (const Vehicle::DrivePoint& point : drive->path) outPath.Add(FVector(point.x, point.y, point.z - TRAFFIC_HEIGHT_EPSILON));
		return outPath.Num() >= 2;
	}
	if (FCarEntry* entry = FindCarEntry(vehicle)) {
		if (entry->element.IsValid()) {
			if (TWeakObjectPtr<AVehicleCommandController>* driver = carDrivers.Find(name); driver && driver->IsValid()) {
				return (*driver)->GetRemainingWaypoints(outPath) && outPath.Num() >= 2;
			}
			return false;
		}
	}
	if (vehicle->IsDriving()) {
		std::vector<Vehicle::DrivePoint> remaining;
		vehicle->GetRemainingDrive(GetWorld()->GetTimeSeconds(), remaining);
		for (const Vehicle::DrivePoint& point : remaining) outPath.Add(FVector(point.x, point.y, point.z - TRAFFIC_HEIGHT_EPSILON));
	}
	return outPath.Num() >= 2;
}

void UForeverTrafficFrameworkComponent::ToggleRouteWatch(const FString& name) {
	if (FRouteWatch* existing = routeWatches.Find(name)) {
		if (existing->mesh) existing->mesh->DestroyComponent();
		routeWatches.Remove(name);
		return;
	}
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	AActor* owner = GetOwner();
	if (!framework || !owner) return;
	const std::string nameUtf8 = TCHAR_TO_UTF8(*name);
	FRouteWatch watch;
	if (trafficRef && trafficRef->FindVehicleByName(nameUtf8)) {
		watch.bCitizen = false;
	}
	else if (framework->GetPopulace() && framework->GetPopulace()->FindCitizenByName(nameUtf8)) {
		watch.bCitizen = true;
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("show_route: 找不到载具或市民 %s"), *name);
		return;
	}
	watch.mesh = NewObject<UProceduralMeshComponent>(owner, NAME_None);
	watch.mesh->SetupAttachment(owner->GetRootComponent());
	watch.mesh->RegisterComponent();
	owner->AddInstanceComponent(watch.mesh);
	if (pureBaseMaterial) {
		watch.material = UMaterialInstanceDynamic::Create(pureBaseMaterial, this);
		watch.material->SetVectorParameterValue(TEXT("Color"), watch.bCitizen ? FLinearColor(0.3f, 1.f, 0.35f) : FLinearColor(1.f, 0.6f, 0.15f));
		watch.mesh->SetMaterial(0, watch.material);
	}
	routeWatches.Add(name, watch);
	routeWatchCountdown = 0.f; // 下一帧就画
}

void UForeverTrafficFrameworkComponent::UpdateRouteWatches(float deltaTime) {
	if (routeWatches.Num() == 0) return;
	routeWatchCountdown -= deltaTime;
	if (routeWatchCountdown > 0.f) return;
	routeWatchCountdown = 0.2f;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	for (TPair<FString, FRouteWatch>& pair : routeWatches) {
		FRouteWatch& watch = pair.Value;
		if (!watch.mesh) continue;
		const std::string nameUtf8 = TCHAR_TO_UTF8(*pair.Key);
		TArray<FVector> path;
		bool bHasPath = false;
		if (watch.bCitizen) {
			Citizen* citizen = framework && framework->GetPopulace() ? framework->GetPopulace()->FindCitizenByName(nameUtf8) : nullptr;
			UForeverPopulaceFrameworkComponent* populaceFramework = framework ? framework->GetPopulaceFramework() : nullptr;
			bHasPath = citizen && populaceFramework && populaceFramework->GetRoutePath(citizen, path);
		}
		else {
			bHasPath = GetVehicleRoutePath(trafficRef ? trafficRef->FindVehicleByName(nameUtf8) : nullptr, path);
		}
		watch.mesh->ClearMeshSection(0);
		if (!bHasPath) continue;
		constexpr float kLift = 80.f, kHalfWidth = 30.f;
		TArray<FVector> vertices;
		TArray<int32> triangles;
		for (int32 i = 0; i + 1 < path.Num(); i++) {
			AppendRouteEdgeRibbon(vertices, triangles, path[i] + FVector(0.f, 0.f, kLift), path[i + 1] + FVector(0.f, 0.f, kLift), kHalfWidth);
		}
		AppendRouteStationBox(vertices, triangles, FVector2D(path.Last().X, path.Last().Y),
			ROUTE_DEBUG_NODE_HALF_SIZE, path.Last().Z + kLift, path.Last().Z + kLift + 200.f);
		watch.mesh->CreateMeshSection(0, vertices, triangles,
			TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		if (watch.material) watch.mesh->SetMaterial(0, watch.material);
	}
}

void UForeverTrafficFrameworkComponent::TeardownVehicleActor(FCarEntry* entry, const FString& name) {
	if (!entry) return;
	if (TWeakObjectPtr<AVehicleCommandController>* driver = carDrivers.Find(name)) {
		if (driver->IsValid()) driver->Get()->ClearCommands();
		carDrivers.Remove(name);
	}
	if (AVehicleElement* element = entry->element.Get()) {
		activeVehicles.Remove(name);
		entry->element = nullptr;
		const float remaining = minVehicleLifetime - static_cast<float>(GetWorld()->GetTimeSeconds() - entry->spawnedAt);
		if (remaining > 0.f) {
			element->SetActorHiddenInGame(true);
			element->SetActorEnableCollision(false);
			TWeakObjectPtr<AVehicleElement> weakElement(element);
			FTimerHandle handle;
			GetWorld()->GetTimerManager().SetTimer(handle, [weakElement]() {
				if (weakElement.IsValid()) weakElement->Destroy();
			}, remaining, false);
		}
		else {
			element->Destroy();
		}
	}
}

namespace {
	double DrivePathLength(const std::vector<Vehicle::DrivePoint>& path) {
		double length = 0.0;
		for (size_t i = 1; i < path.size(); i++) {
			const double dx = path[i].x - path[i - 1].x, dy = path[i].y - path[i - 1].y, dz = path[i].z - path[i - 1].z;
			length += FMath::Sqrt(dx * dx + dy * dy + dz * dz);
		}
		return length;
	}
}

void UForeverTrafficFrameworkComponent::RegisterSkipDrive(Vehicle* core, Room* destination, const std::vector<Vehicle::DrivePoint>& path, bool bRandom) {
	if (!core || !destination) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	const double gameSecondsPerRealSecond = 60.0 * (player ? player->GetTimeFlowRatio() : 10.0);
	const double speedUU = FMath::Max(1.0, static_cast<double>(GetDefault<AVehicleCommandController>()->maxSpeed) * kUnspawnedDriveSpeedScale);
	FSkipDrive& drive = skipDrives.FindOrAdd(UTF8_TO_TCHAR(core->GetName().c_str()));
	drive.vehicle = core;
	drive.destination = destination;
	drive.path = path;
	drive.bRandom = bRandom;
	drive.totalSeconds = DrivePathLength(path) / speedUU * gameSecondsPerRealSecond;
	drive.remainingSeconds = drive.totalSeconds - (framework ? framework->GetSkipCreditSeconds() : 0.0);
}

void UForeverTrafficFrameworkComponent::BeginTimeSkip() {
	bTimeSkipping = true;
	if (!trafficRef || !GetWorld()) return;
	const double now = GetWorld()->GetTimeSeconds();
	for (FCarEntry& entry : carEntries) {
		Vehicle* core = entry.vehicle;
		if (!core || core->GetRoute() || !navTargets.Contains(entry.name)) continue; // 只转脚本/通勤/任务导航中的车
		if (AVehicleElement* element = entry.element.Get()) {
			if (element->IsPlayerControlled()) continue;
			HandBackDriveToCore(entry); // 交回Core推算，下面按剩余路径转成腿
		}
		if (!core->IsDriving()) continue;
		Room* destination = core->GetDriveDestination();
		if (!destination) continue;
		std::vector<Vehicle::DrivePoint> remaining;
		core->GetRemainingDrive(now, remaining);
		core->EndDrive();
		core->SetCurrentRoom(nullptr);
		RegisterSkipDrive(core, destination, remaining, randomTargets.FindRef(entry.name));
	}
}

void UForeverTrafficFrameworkComponent::DecrementSkipLegs(double stepGameSeconds) {
	for (TPair<FString, FSkipDrive>& pair : skipDrives) pair.Value.remainingSeconds -= stepGameSeconds;
}

bool UForeverTrafficFrameworkComponent::CompleteSkipLegs() {
	TArray<FString> due;
	for (const TPair<FString, FSkipDrive>& pair : skipDrives) {
		if (pair.Value.remainingSeconds <= 0.0) due.Add(pair.Key);
	}
	if (due.Num() == 0) return false;
	due.Sort(); // 稳定顺序
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	for (const FString& name : due) {
		FSkipDrive* found = skipDrives.Find(name);
		if (!found || found->remainingSeconds > 0.0) continue;
		const FSkipDrive drive = *found;
		skipDrives.Remove(name);
		Vehicle* core = drive.vehicle;
		if (framework) framework->SetSkipCreditSeconds(-drive.remainingSeconds);
		// 车停进预订的车位(没有预订=被玩家的车占了/已改派，兜底成到达这个房间)，位姿放到车位上。
		if (!core->ArriveAtReservedSlot()) core->SetCurrentRoom(drive.destination);
		FVector pose;
		float poseYaw;
		if (GetParkedPose(core, pose, poseYaw)) {
			core->SetTransform(pose.X, pose.Y, pose.Z, poseYaw);
			if (FCarEntry* entry = FindCarEntry(core)) {
				entry->location = pose;
				entry->yaw = poseYaw;
			}
		}
		navTargets.Remove(name);
		pendingNavResults.Add({ core, drive.destination, true, drive.bRandom });
		// 到达回调(乘客/任务衔接、vehicle_arrive)可能立刻发起下一段导航：当场处理，带着多出来的时间。
		for (int32 round = 0; round < 32; round++) {
			ProcessNavResults();
			if (pendingNavigations.Num() == 0) break;
			while (pendingNavigations.Num() > 0) {
				const FNavRequest request = pendingNavigations[0];
				pendingNavigations.RemoveAt(0);
				StartVehicleNavigation(request);
			}
			if (pendingNavResults.Num() == 0) break;
		}
	}
	if (framework) framework->SetSkipCreditSeconds(0.0);
	return true;
}

void UForeverTrafficFrameworkComponent::EndTimeSkip() {
	bTimeSkipping = false;
	if (!GetWorld()) { skipDrives.Empty(); return; }
	const double now = GetWorld()->GetTimeSeconds();
	TArray<FString> keys;
	skipDrives.GetKeys(keys);
	keys.Sort();
	for (const FString& name : keys) {
		const FSkipDrive drive = skipDrives.FindRef(name);
		skipDrives.Remove(name);
		Vehicle* core = drive.vehicle;
		const double fraction = drive.totalSeconds > 0.0 ? FMath::Clamp(1.0 - drive.remainingSeconds / drive.totalSeconds, 0.0, 1.0) : 1.0;
		// 沿路径走过fraction比例后的位置，之后的点接着开。
		const double target = DrivePathLength(drive.path) * fraction;
		std::vector<Vehicle::DrivePoint> rest;
		double walked = 0.0;
		for (size_t i = 1; i < drive.path.size(); i++) {
			const Vehicle::DrivePoint& a = drive.path[i - 1];
			const Vehicle::DrivePoint& b = drive.path[i];
			const double segment = FMath::Sqrt(FMath::Square(b.x - a.x) + FMath::Square(b.y - a.y) + FMath::Square(b.z - a.z));
			if (rest.empty() && walked + segment >= target) {
				const double t = segment > KINDA_SMALL_NUMBER ? (target - walked) / segment : 0.0;
				Vehicle::DrivePoint here = a;
				here.x = static_cast<float>(a.x + (b.x - a.x) * t);
				here.y = static_cast<float>(a.y + (b.y - a.y) * t);
				here.z = static_cast<float>(a.z + (b.z - a.z) * t);
				rest.push_back(here);
			}
			if (!rest.empty()) rest.push_back(b);
			walked += segment;
		}
		core->SetCurrentRoom(nullptr);
		if (rest.size() >= 2) core->BeginDrive(rest, drive.destination, now, GetDefault<AVehicleCommandController>()->maxSpeed * kUnspawnedDriveSpeedScale);
		if (!core->IsDriving()) {
			// 没有剩余路径(刚好到了)：当作到达，下一帧Tick处理结果。
			if (!core->ArriveAtReservedSlot()) core->SetCurrentRoom(drive.destination);
			navTargets.Remove(name);
			pendingNavResults.Add({ core, drive.destination, true, drive.bRandom });
		}
	}
}

void UForeverTrafficFrameworkComponent::TeleportVehicleToSlot(Vehicle* vehicle, Room* room, int32 slotIndex) {
	if (!vehicle || !room) return;
	const FString name = UTF8_TO_TCHAR(vehicle->GetName().c_str());
	// 还没处理的导航请求作废。
	pendingNavigations.RemoveAll([&](const FNavRequest& request) { return request.vehicleName == name; });
	if (FCarEntry* entry = FindCarEntry(vehicle)) {
		if (TWeakObjectPtr<AVehicleCommandController>* driver = carDrivers.Find(name)) {
			if (driver->IsValid()) driver->Get()->ClearCommands();
			carDrivers.Remove(name);
		}
		if (AVehicleElement* element = entry->element.Get()) {
			activeVehicles.Remove(name);
			entry->element = nullptr;
			const float remaining = minVehicleLifetime - static_cast<float>(GetWorld()->GetTimeSeconds() - entry->spawnedAt);
			if (remaining > 0.f) {
				element->SetActorHiddenInGame(true);
				element->SetActorEnableCollision(false);
				TWeakObjectPtr<AVehicleElement> weakElement(element);
				FTimerHandle handle;
				GetWorld()->GetTimerManager().SetTimer(handle, [weakElement]() {
					if (weakElement.IsValid()) weakElement->Destroy();
				}, remaining, false);
			}
			else {
				element->Destroy();
			}
		}
	}
	vehicle->EndDrive();
	navTargets.Remove(name);
	if (!vehicle->ReserveSlot(room, slotIndex) || !vehicle->ArriveAtReservedSlot()) {
		vehicle->SetVanished(room); // 车位被别的车占了：只好消失在这个房间
		return;
	}
	FVector pose;
	float poseYaw;
	if (GetParkedPose(vehicle, pose, poseYaw)) {
		vehicle->SetTransform(pose.X, pose.Y, pose.Z, poseYaw);
		if (FCarEntry* entry = FindCarEntry(vehicle)) {
			entry->location = pose;
			entry->yaw = poseYaw;
		}
	}
}

void UForeverTrafficFrameworkComponent::ProcessPendingNavigations() {
	if (!mapRef || !trafficRef) return; // 车辆还没登记（GenerateVehicles之前）：请求先留着
	// 按毫秒预算而不是按个数：规划一条路线的耗时差异很大(缓存命中几乎为0，冷启动整图搜索要几十毫秒)，
	// 每帧至少处理1个保证前进，之后用完预算就留到下一帧。
	constexpr double kNavBudgetSeconds = 0.008;
	constexpr int32 kNavMaxPerFrame = 64;
	const double start = FPlatformTime::Seconds();
	int32 processed = 0;
	while (pendingNavigations.Num() > 0 && processed < kNavMaxPerFrame) {
		if (processed > 0 && FPlatformTime::Seconds() - start >= kNavBudgetSeconds) break;
		const FNavRequest request = pendingNavigations[0];
		pendingNavigations.RemoveAt(0);
		StartVehicleNavigation(request);
		processed++;
	}
	const double elapsedMs = (FPlatformTime::Seconds() - start) * 1000.0;
	if (elapsedMs > 30.0) {
		UE_LOG(LogTemp, Warning, TEXT("ProcessPendingNavigations: 处理%d个导航请求耗时%.1f ms，剩余%d个排队"), processed, elapsedMs, pendingNavigations.Num());
	}
}

Room* UForeverTrafficFrameworkComponent::PickRandomVehicleRoom(Vehicle* core, int32& outSlotIndex) {
	outSlotIndex = -1;
	if (!mapRef || !core) return nullptr;
	const std::string& category = core->GetCategory();
	const Room* exclude = core->GetCurrentRoom();
	const auto& graph = mapRef->GetVehicleNavGraph();
	if (vehicleRoomCache.Num() == 0) {
		// 有车位且车位节点在车行图里的房间，地下室(layer<0)优先；没有地下室的就用全部。
		TArray<Room*> basement, other;
		for (const auto& [buildingName, building] : mapRef->GetBuildings()) {
			if (!building) continue;
			for (Room* room : building->GetRooms()) {
				if (!room) continue;
				bool bHasNode = false;
				for (const ParkingSlot* slot : room->GetParkingSlots()) {
					if (slot->navigationNode && graph.find(slot->navigationNode->GetId()) != graph.end()) { bHasNode = true; break; }
				}
				if (bHasNode) (room->GetLayer() < 0 ? basement : other).Add(room);
			}
		}
		vehicleRoomCache = basement.Num() > 0 ? basement : other;
		UE_LOG(LogTemp, Log, TEXT("UForeverTrafficFrameworkComponent: %d rooms with parking slots (basement %d)."),
			vehicleRoomCache.Num(), basement.Num());
	}
	if (vehicleRoomCache.Num() == 0) return nullptr;

	// 车在车位/房间里：只在从它出发沿车行图真正走得到的车位里挑（很多地下室互相不连通，乱挑大多规划不到）。
	Node* startNode = nullptr;
	if (ParkingSlot* parkedSlot = core->GetParkingSlot()) startNode = parkedSlot->navigationNode;
	if (!startNode && exclude) startNode = exclude->GetNavigationNode();
	const TSet<int32>* reachable = nullptr;
	if (startNode) {
		reachable = reachableCache.Find(startNode->GetId());
		if (!reachable) {
			TSet<int32> visited;
			TArray<int32> stack;
			visited.Add(startNode->GetId());
			stack.Add(startNode->GetId());
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
	}

	// 房间里第一个空闲、允许这个种类、(有起点时)走得到的车位下标。
	auto findSlot = [&](Room* room) -> int32 {
		const std::vector<ParkingSlot*>& slots = room->GetParkingSlots();
		for (size_t i = 0; i < slots.size(); i++) {
			const ParkingSlot* slot = slots[i];
			if (!slot->IsFree() || !slot->Accepts(category) || !slot->navigationNode) continue;
			if (reachable && !reachable->Contains(slot->navigationNode->GetId())) continue;
			return static_cast<int32>(i);
		}
		return -1;
	};

	TArray<TPair<Room*, int32>> candidates;
	for (Room* room : vehicleRoomCache) {
		if (room == exclude) continue;
		const int32 index = findSlot(room);
		if (index >= 0) candidates.Add(TPair<Room*, int32>(room, index));
	}
	if (candidates.Num() == 0) return nullptr;
	CORE_RANDOM_SCOPE("core.traffic");
	const TPair<Room*, int32>& picked = candidates[GetRandom(candidates.Num())];
	outSlotIndex = picked.Value;
	return picked.Key;
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
	const bool bPreset = request.presetRoom != nullptr;
	// 带预设车位的请求(通勤)：任何一步失败都要给结果，否则乘客永远等不到车。
	auto reportPresetFailure = [&]() {
		if (!bPreset || !trafficRef) return;
		Vehicle* failed = trafficRef->FindVehicleByName(TCHAR_TO_UTF8(*request.vehicleName));
		if (failed) {
			failed->ClearReservation();
			pendingNavResults.Add({ failed, request.presetRoom, false, false });
		}
	};
	if (!mapRef) { reportPresetFailure(); return; }
	// 线路车有自己的时刻表/循环指令，不接这条路。
	if (const FLineVehicleState* lineState = lineVehicleStates.Find(request.vehicleName)) {
		if (lineState->bRouteVehicle) { reportPresetFailure(); return; }
	}
	FCarEntry* entry = nullptr;
	for (FCarEntry& candidate : carEntries) {
		if (candidate.name == request.vehicleName) { entry = &candidate; break; }
	}
	if (!entry || !entry->vehicle) {
		UE_LOG(LogTemp, Warning, TEXT("vehicle_navigate: 找不到车辆 %s"), *request.vehicleName);
		reportPresetFailure();
		return;
	}
	Vehicle* core = entry->vehicle;
	AVehicleElement* element = entry->element.Get();
	if (element && element->IsPlayerControlled()) { reportPresetFailure(); return; } // 玩家在开

	// 新请求取代没走完的旧请求：旧的车位预定先放掉(预设车位的请求预定是调用方刚订好的，保留)。
	if (!bPreset) core->ClearReservation();

	// 车停在没有车行节点的位置（车位节点/房间节点都不在车行图里）必然规划失败，直接跳过。消失的车是瞬移，不用路网。
	const bool bVanished = core->GetParkState() == Vehicle::ParkState::Vanished;
	Room* parked = core->GetCurrentRoom();
	if (!bVanished && parked) {
		ParkingSlot* parkedSlot = core->GetParkingSlot();
		Node* parkedNode = parkedSlot ? parkedSlot->navigationNode : parked->GetNavigationNode();
		if (!parkedNode || mapRef->GetVehicleNavGraph().find(parkedNode->GetId()) == mapRef->GetVehicleNavGraph().end()) {
			static int32 skipped = 0;
			if (++skipped % 100 == 1) {
				UE_LOG(LogTemp, Warning, TEXT("vehicle_navigate: 车辆 %s 停在没有车行节点的房间，跳过（共跳过 %d 辆）"), *request.vehicleName, skipped);
			}
			reportPresetFailure();
			return;
		}
	}

	// 目的地必须有一个空闲且允许这辆车种类的车位：寻路开始就把它占上(预定)，不然多辆车会抢同一个车位。
	const bool bRandom = request.destination == TEXT("random");
	int32 slotIndex = -1;
	Room* destination = nullptr;
	if (bPreset) {
		destination = request.presetRoom;
		slotIndex = request.presetSlot;
	}
	else if (bRandom) {
		destination = PickRandomVehicleRoom(core, slotIndex);
	}
	else {
		destination = mapRef->LocateRoom(TCHAR_TO_UTF8(*request.destination));
		if (destination) slotIndex = destination->FindFreeSlot(core->GetCategory());
	}
	if (!destination) {
		UE_LOG(LogTemp, Warning, TEXT("vehicle_navigate: 车辆 %s 找不到目的地 %s"), *request.vehicleName, *request.destination);
		reportPresetFailure();
		return;
	}
	if (slotIndex < 0 || !core->ReserveSlot(destination, slotIndex)) {
		UE_LOG(LogTemp, Log, TEXT("vehicle_navigate: 车辆 %s 的目的地 %s 没有空闲车位，拒绝"), *request.vehicleName, UTF8_TO_TCHAR(destination->GetAddress().c_str()));
		pendingNavResults.Add({ core, destination, false, bRandom });
		return;
	}
	ParkingSlot* targetSlot = core->GetReservedSlot();
	Node* node = targetSlot ? targetSlot->navigationNode : nullptr;
	if (!node) {
		core->ClearReservation();
		UE_LOG(LogTemp, Warning, TEXT("vehicle_navigate: 车辆 %s 的目的车位没有导航节点 %s"), *request.vehicleName, UTF8_TO_TCHAR(destination->GetAddress().c_str()));
		reportPresetFailure();
		return;
	}
	navTargets.Add(request.vehicleName, destination);
	randomTargets.Add(request.vehicleName, bRandom);

	// 已经消失的车：下次寻路直接瞬移到目的车位，不规划路线。
	if (bVanished) {
		core->ArriveAtReservedSlot();
		FVector pose;
		float poseYaw;
		if (GetParkedPose(core, pose, poseYaw)) {
			core->SetTransform(pose.X, pose.Y, pose.Z, poseYaw);
			entry->location = pose;
			entry->yaw = poseYaw;
		}
		pendingNavResults.Add({ core, destination, true, bRandom });
		navTargets.Remove(request.vehicleName);
		return;
	}

	// 有Actor（在玩家附近）：交给Controller开。跳时间期间改走Core+虚拟时钟：先拆Actor，位置交给Core。
	if (element && bTimeSkipping) {
		const FVector location = element->GetActorLocation();
		core->SetTransform(location.X, location.Y, location.Z, element->GetActorRotation().Yaw);
		TeardownVehicleActor(entry, request.vehicleName);
	}
	else if (element) {
		AVehicleCommandController* controller = GetOrCreateCarDriver(request.vehicleName, element);
		if (!controller) {
			core->ClearReservation();
			navTargets.Remove(request.vehicleName);
			if (bPreset) pendingNavResults.Add({ core, destination, false, false });
			return;
		}
		controller->ClearCommands();
		controller->PushCommand(FVehicleCommand::MakeNavigate(node->GetId()));
		return;
	}

	// 没有Actor：和市民一样，Core按时间沿路线推算位置，进入生成距离才生成Actor（见TickComponent）。
	float x, y, z, yaw;
	core->GetTransform(x, y, z, yaw);
	const float yawRad = FMath::DegreesToRadians(yaw);
	Map::VehicleRoutePlan plan;
	const double planStart = FPlatformTime::Seconds();
	if (ParkingSlot* parkedSlot = core->GetParkingSlot()) {
		// 停在车位里：先倒出到与车位相连的connection另一端再远一个车长，再向前沿导航线拐弯。
		float sizeX, sizeY, sizeZ;
		core->GetSize(sizeX, sizeY, sizeZ);
		plan = mapRef->PlanVehicleRouteFromSlot(core->GetRoom(), parkedSlot, node->GetId(), sizeX / TRAFFIC_WORLD_SCALE);
	}
	else {
		plan = mapRef->PlanVehicleRouteToNode(core->GetCurrentRoom(),
			x / TRAFFIC_WORLD_SCALE, y / TRAFFIC_WORLD_SCALE, node->GetId(), true, FMath::Cos(yawRad), FMath::Sin(yawRad));
	}
	const double planMs = (FPlatformTime::Seconds() - planStart) * 1000.0;
	if (planMs > 20.0) UE_LOG(LogTemp, Warning, TEXT("vehicle_navigate: 车辆 %s 路线规划耗时 %.1f ms"), *request.vehicleName, planMs);
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
	if (plan.reverseFirstSegment && !path.empty()) path[0].reverse = true;
	core->ReleaseSlot();
	core->SetCurrentRoom(nullptr);
	if (bTimeSkipping) {
		RegisterSkipDrive(core, destination, path, bRandom);
		return;
	}
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
	ParkingSlot* targetSlot = core ? core->GetReservedSlot() : nullptr;
	Node* node = targetSlot ? targetSlot->navigationNode : (destination ? destination->GetNavigationNode() : nullptr);
	if (!element || !node) return;

	// 还在倒车出库：重新规划会丢掉倒车段，把剩余路径(含倒车标志)原样交给Controller沿着开。
	std::vector<Vehicle::DrivePoint> remaining;
	core->GetRemainingDrive(GetWorld()->GetTimeSeconds(), remaining);
	TArray<FVector> waypoints;
	const bool bReversing = !remaining.empty() && remaining[0].reverse;
	// 不论正向还是倒车，都把Core剩余路径原样交给Controller沿着开：重新规划(Navigate)可能选到相反的方向，车一出现就掉头。
	for (const Vehicle::DrivePoint& p : remaining) waypoints.Add(FVector(p.x, p.y, p.z - TRAFFIC_HEIGHT_EPSILON));
	core->EndDrive();
	if (AVehicleCommandController* controller = GetOrCreateCarDriver(entry.name, element)) {
		controller->ClearCommands();
		if (waypoints.Num() >= 2) controller->PushCommand(FVehicleCommand::MakeFollowPath(node->GetId(), waypoints, bReversing));
		else controller->PushCommand(FVehicleCommand::MakeNavigate(node->GetId()));
	}
}

bool UForeverTrafficFrameworkComponent::HandBackDriveToCore(FCarEntry& entry) {
	AVehicleElement* element = entry.element.Get();
	Room** destination = navTargets.Find(entry.name);
	TWeakObjectPtr<AVehicleCommandController>* driver = carDrivers.Find(entry.name);
	if (!element || !destination || !*destination || !driver || !driver->IsValid()) return false;
	TArray<FVector> remaining;
	bool bReverseFirst = false;
	if (!(*driver)->GetRemainingWaypoints(remaining, &bReverseFirst) || remaining.Num() < 2) return false;
	std::vector<Vehicle::DrivePoint> path;
	for (const FVector& point : remaining) path.push_back({ static_cast<float>(point.X), static_cast<float>(point.Y), static_cast<float>(point.Z) });
	if (bReverseFirst) path[0].reverse = true;
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
			result.vehicle->ClearReservation();
			int32& tries = randomRetries.FindOrAdd(failedName);
			if (result.bRandom && ++tries <= 5) {
				pendingNavigations.Add({ failedName, TEXT("random") });
				continue;
			}
			tries = 0;
			UE_LOG(LogTemp, Warning, TEXT("vehicle_navigate: 车辆 %s 规划不到 %s，放弃"),
				UTF8_TO_TCHAR(result.vehicle->GetName().c_str()), UTF8_TO_TCHAR(result.room->GetAddress().c_str()));
			if (OnVehicleNavigationFinished) OnVehicleNavigationFinished(result.vehicle, result.room, false);
			continue;
		}
		// 车现在停在预定的车位上：预定转成停靠(没有预定=车位被玩家的车占了、已改派/消失之后的兜底)，下一次规划就从车位出发。
		if (!result.vehicle->ArriveAtReservedSlot()) result.vehicle->SetCurrentRoom(result.room);
		if (OnVehicleNavigationFinished) OnVehicleNavigationFinished(result.vehicle, result.room, true);
		if (!storyFramework) continue;
		randomRetries.Remove(UTF8_TO_TCHAR(result.vehicle->GetName().c_str()));
		VehicleArriveEvent event(result.vehicle->GetName(), result.room->GetAddress());
		storyFramework->BroadcastToVehicle(event, result.vehicle);
	}
}

void UForeverTrafficFrameworkComponent::StopVehicleLineRun(const FString& vehicleName) {
	FLineVehicleState* state = lineVehicleStates.Find(vehicleName);
	if (!state) return;
	if (state->bRouteVehicle) {
		// 线路车：停止运行 = 退出时刻表，由 Controller 刹停并持有。先确保物理表示+Controller 存在。
		EnsureLinePhysicsElement(vehicleName);
		if (state->vehicle) state->vehicle->SetPhysicsDriven(true);
		state->bDetached = true;
	}
	if (AVehicleCommandController* controller = GetOrCreateLineController(*state)) {
		controller->StopNow();
	}
}
