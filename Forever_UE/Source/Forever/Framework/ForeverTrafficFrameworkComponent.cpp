#include "Framework/ForeverTrafficFrameworkComponent.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Element/VehicleElement.h"
#include "Element/TransitVehicleElement.h"
#include "Element/CitizenElement.h"
#include "traffic/traffic.h"
#include "traffic/vehicle.h"
#include "traffic/route.h"
#include "traffic/station.h"
#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "common/utility.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

using namespace std;

// 和BuildingElement.cpp同名常量/同一个公式——这个项目的既有约定是每个需要room-local→世界
// 坐标换算的.cpp文件各自抄一份(ForeverPopulaceFrameworkComponent.cpp也是这么做的)，不额外
// 抽公共函数，见Source/Forever/Framework/ForeverPopulaceFrameworkComponent.cpp顶部注释。
#define TRAFFIC_WORLD_SCALE 1000.f

// 载具用的骨骼网格是bSimulatePhysics=true的真实刚体(见VehicleElement.cpp构造函数)，和市民用的
// 胶囊体(POPULACE_HEIGHT_EPSILON=10就够，CMC每帧温和地解算穿插)不是一回事——physics引擎对
// 刚体生成时的穿插极其敏感，哪怕只嵌进地板1个单位，Chaos也会在后面几帧用很大的分离冲量把车
// 弹开，实测反馈"一开局就有些车被弹飞到天上"。生成点要离地板明显有余量，让车自己靠重力自然
// 落地，而不是生成时就蹭着地板，50个单位(0.5米)是比车轮半径大的一个保守值。
#define TRAFFIC_HEIGHT_EPSILON 50.f

// 公交/火车/飞机线路调试画线用的尺寸(UE单位)，纯debug标记，照抄
// ForeverRoadnetFrameworkComponent.cpp导航图debug可视化的经验值(NAV_DEBUG_*)。
#define ROUTE_DEBUG_EDGE_HALF_WIDTH 8.f
#define ROUTE_DEBUG_NODE_HALF_SIZE 40.f
#define ROUTE_DEBUG_HEIGHT 50.f
#define ROUTE_DEBUG_ARROW_LENGTH 120.f
#define ROUTE_DEBUG_ARROW_HALF_WIDTH 40.f
// 每隔多少地图单位采一个样，至少16段——照抄public_transport_plan.md"调试画线"一节的数值。
#define ROUTE_DEBUG_SAMPLE_STEP 0.25f
#define ROUTE_DEBUG_MIN_STEPS 16

namespace {
	void ComputeWorldPosition(const Building& building, float localX, float localY,
		float& outWorldX, float& outWorldY) {
		float relX = localX - building.GetBodySizeX() * 0.5f + building.GetBodyOffsetX();
		float relY = localY - building.GetBodySizeY() * 0.5f + building.GetBodyOffsetY();
		float rot = building.GetRotation();
		float c = FMath::Cos(rot), s = FMath::Sin(rot);
		outWorldX = (building.GetPosX() + relX * c - relY * s) * TRAFFIC_WORLD_SCALE;
		outWorldY = (building.GetPosY() + relX * s + relY * c) * TRAFFIC_WORLD_SCALE;
	}

	// 下车点能不能用：既要终点本身站得下人，也要车到终点这一路没有东西挡着（车贴墙时，
	// exitOffsetY算出来的点可能已经在墙的另一侧——那个点自己是空的，但路上隔着一堵墙，人
	// 不应该直接穿墙过去）。从车身中心sweep到候选下车点，一次查询同时覆盖"终点被占用"和
	// "半路被挡"两种情况。
	//
	// 不能用OverlapAnyTestByChannel/OverlapMulti这类"重叠类"查询——建筑/房间/园区的边界盒
	// (ABuildingElement::BuildCollisionBox/BuildRoomCollisionBoxes、
	// UForeverZoneFrameworkComponent的box，还有市民/车辆自己的proximity box)全都是"Trigger"
	// 碰撞档案(QueryOnly+对所有通道都是Overlap，用来触发进出提示/MeetOption，不是真的拿来挡
	// 人的)，重叠类查询对"Block"和"Overlap"响应一视同仁，只要没设成Ignore就算命中，车只要停在
	// 建筑/房间/园区范围内就会被误判"被挡住"。改用SweepTestByChannel——扫掠类查询只认"Block"
	// 响应，Trigger档案的Overlap响应天然被忽略，真正会挡人的墙体/地板/载具车身(Vehicle档案
	// 对Pawn是Block)才会被测到。
	bool IsExitPathBlocked(UWorld* world, const FVector& from, const FVector& to, const FCollisionQueryParams& queryParams) {
		return world->SweepTestByChannel(
			from, to, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(42.f, 96.f), queryParams);
	}

	// 双面四边形——和ForeverRoadnetFrameworkComponent.cpp的RoadnetAppendQuadDoubleSided同一个
	// 技巧，这个项目的既有约定是每个需要它的.cpp文件各自抄一份，不额外抽公共函数。
	void RouteAppendQuadDoubleSided(TArray<FVector>& vertices, TArray<int32>& triangles,
		const FVector& v00, const FVector& v10, const FVector& v11, const FVector& v01) {
		int32 base = vertices.Num();
		vertices.Add(v00); vertices.Add(v10); vertices.Add(v11); vertices.Add(v01);
		triangles.Add(base); triangles.Add(base + 2); triangles.Add(base + 1);
		triangles.Add(base); triangles.Add(base + 3); triangles.Add(base + 2);
		triangles.Add(base); triangles.Add(base + 1); triangles.Add(base + 2);
		triangles.Add(base); triangles.Add(base + 2); triangles.Add(base + 3);
	}

	// 站点接口用的小box，照抄AppendNavBox。
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

	// 两点间的细ribbon，照抄AppendNavEdgeRibbon。
	void AppendRouteEdgeRibbon(TArray<FVector>& vertices, TArray<int32>& triangles,
		const FVector& from, const FVector& to, float halfWidth) {
		FVector2D dir2D(to.X - from.X, to.Y - from.Y);
		float len = dir2D.Size();
		if (len < 1e-3f) return;
		dir2D /= len;
		FVector offset(-dir2D.Y * halfWidth, dir2D.X * halfWidth, 0.f);

		RouteAppendQuadDoubleSided(vertices, triangles, from - offset, to - offset, to + offset, from + offset);
	}

	// 一条edge(可能是多段segments拼起来的)按弧长采样成世界坐标(UE单位)折线——不管edge->reversed
	// (那个标志只影响Route::Update()驱动载具时的遍历方向/切线符号，画出来的曲线形状本身和
	// 方向无关，正向反向用的是同一组segments)。相邻两段共享的端点只采一次，避免重复顶点。
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

	// 在折线中点画一个小箭头指示行驶方向(edge->reversed时指向相反)——两个三角形拼成箭头形状，
	// 双面绘制，从任意角度看都不会因为背面剔除而消失。
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
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> pureFinder(
		TEXT("/Game/Asset/Materials/Pure.Pure"));
	if (pureFinder.Succeeded()) {
		pureBaseMaterial = pureFinder.Object;
	}
}

void UForeverTrafficFrameworkComponent::GenerateVehicles(Map* map, Traffic* traffic) {
	if (!map || !traffic) return;

	for (auto& [name, vehicle] : traffic->GetVehicles()) {
		if (!vehicle) continue;

		if (vehicle->GetRoute()) {
			// 公共交通车辆(category!=car)：不停车位，不需要换算世界坐标——Tick里直接读
			// vehicle->GetTransform()驱动，第一帧Tick就会跳到Route算出来的真实位置，生成时
			// 给个原点占位无所谓。
			UClass* transitClass = ATransitVehicleElement::StaticClass();
			const string& blueprintPath = vehicle->GetBlueprintPath();
			if (!blueprintPath.empty()) {
				FString path = UTF8_TO_TCHAR(blueprintPath.data());
				if (UClass* loaded = LoadClass<ATransitVehicleElement>(nullptr, *path)) {
					transitClass = loaded;
				}
			}

			ATransitVehicleElement* transitElement = GetWorld()->SpawnActor<ATransitVehicleElement>(
				transitClass, FVector::ZeroVector, FRotator::ZeroRotator);
			if (!transitElement) continue;

			transitElement->Init(vehicle);
			activeTransitVehicles.Add(UTF8_TO_TCHAR(vehicle->GetName().c_str()), transitElement);
			continue;
		}

		Room* room = vehicle->GetRoom();
		Building* building = room ? room->GetParentBuilding() : nullptr;
		if (!room || !building) continue; // 不是预置在停车位里的车辆(理论上这次Traffic::Init
										   // 生成的车辆都会有room，防御性写法)

		float localX, localY;
		vehicle->GetParkingLocalPosition(localX, localY);

		float worldX, worldY;
		ComputeWorldPosition(*building, room->GetPosX() + localX, room->GetPosY() + localY, worldX, worldY);
		float worldZ = TRAFFIC_HEIGHT_EPSILON + building->GetFloorBaseZ(room->GetLayer()) * TRAFFIC_WORLD_SCALE;
		float worldYaw = FMath::RadiansToDegrees(building->GetRotation()) + vehicle->GetParkingRotationDegrees();

		FString blueprintPath = UTF8_TO_TCHAR(vehicle->GetBlueprintPath().data());
		UClass* vehicleClass = LoadClass<AVehicleElement>(nullptr, *blueprintPath);
		if (!vehicleClass) {
			UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::GenerateVehicles: 加载车辆蓝图失败，路径=%s，车辆=%s"),
				*blueprintPath, UTF8_TO_TCHAR(vehicle->GetName().c_str()));
			continue;
		}

		// carMesh是有真实碰撞、参与物理模拟的骨骼网格(见VehicleElement.cpp构造函数)，停在
		// 房间里这个点离墙/地板的碰撞体很容易贴得很近，默认的生成时碰撞检测会因为"生成点被
		// 占用"直接拒绝生成(SpawnActor返回nullptr)——照抄原ToggleVehicle MOUNT分支的做法，
		// 用AlwaysSpawn跳过这次检测(实测：没加这个之前，PIE日志里每一次SpawnActor都报
		// "SpawnActor failed because of collision at the spawn location"，车辆一辆都没真正
		// 生成出来，见排查记录)。
		FActorSpawnParameters spawnParams;
		spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AVehicleElement* vehicleElement = GetWorld()->SpawnActor<AVehicleElement>(
			vehicleClass, FVector(worldX, worldY, worldZ), FRotator(0.f, worldYaw, 0.f), spawnParams);
		if (!vehicleElement) continue;

		vehicleElement->Init(vehicle, nullptr); // 这时候还没人上车，previousPawn先留空
		activeVehicles.Add(UTF8_TO_TCHAR(vehicle->GetName().c_str()), vehicleElement);
	}
}

void UForeverTrafficFrameworkComponent::ApplyEnterVehicle(const FString& vehicleName, APlayerController* controller) {
	if (!controller) return;

	TObjectPtr<AVehicleElement>* found = activeVehicles.Find(vehicleName);
	AVehicleElement* vehicleElement = found ? found->Get() : nullptr;
	if (!vehicleElement) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::ApplyEnterVehicle: 找不到车辆 %s"), *vehicleName);
		return;
	}

	APawn* currentPawn = controller->GetPawn();
	if (!currentPawn || currentPawn == vehicleElement) return;

	// 隐藏之后这个pawn会一直停在原地，但UForeverPopulaceFrameworkComponent::TickComponent的
	// 距离流式销毁只看"离当前玩家pawn多远"——玩家开车远离之后这个停在原地的市民会被判定为
	// "走远的市民"直接Destroy掉，previousPawn变成悬空指针，下车时车没了但人也出不来、操控
	// 彻底失灵(照抄原ToggleVehicle MOUNT分支踩过的坑)。上车期间标记豁免，下车时解除。
	if (ACitizenElement* citizenElement = Cast<ACitizenElement>(currentPawn)) {
		citizenElement->SetDespawnExempt(true);
	}

	// 上车后"走远一步才能再上车"是已知的小瑕疵：SetActorEnableCollision(false)不保证同步清掉
	// 陈旧的重叠记录，试过"调UpdateOverlaps()强制清一次"(没用)、"先挪到远处制造一次干净的
	// End再隐藏"(这一步会连带触发离开当前房间/建筑的边界盒，弹出不该出现的"离开XX"提示)，
	// 两条路都有副作用，先回退到最朴素的写法——有这个小瑕疵，但不会有额外的误报提示。
	currentPawn->SetActorHiddenInGame(true);
	currentPawn->SetActorEnableCollision(false);
	vehicleElement->SetPreviousPawn(currentPawn);
	controller->Possess(vehicleElement);
}

void UForeverTrafficFrameworkComponent::ExitVehicle(APlayerController* controller) {
	if (!controller) return;

	AVehicleElement* vehicleElement = Cast<AVehicleElement>(controller->GetPawn());
	if (!vehicleElement) return;

	Vehicle* vehicle = vehicleElement->GetVehicle();
	if (!vehicle) return;

	// 下车点——车身局部坐标系下的偏移(VehicleMod指定，默认左侧)，按车辆当前transform(位置+
	// 完整旋转，侧翻/颠簸时也一起考虑)变换成候选世界坐标。
	FVector localOffset(vehicle->GetExitOffsetX(), vehicle->GetExitOffsetY(), vehicle->GetExitOffsetZ());
	FVector candidateLocation = vehicleElement->GetActorTransform().TransformPositionNoScale(localOffset);

	// 检测这个位置站不站得下人——用ECC_Pawn通道，不能用ECC_Visibility(这个项目的Pawn预设
	// 碰撞档案会忽略ECC_Visibility，见[[memory:pawn_preset_ignores_visibility]])。胶囊体
	// 尺寸照抄AForeverCharacter构造函数里InitCapsuleSize(42.f, 96.0f)的实际数值。
	//
	// 必须忽略载具自己：carMesh用的是引擎自带的"Vehicle"碰撞档案，对ECC_Pawn默认是Block，
	// 下车点只在车身旁边200个单位，不排除车身自己的话，这次重叠检测测到的其实是"车身挡住
	// 了自己旁边的下车点"——不管车四周多空旷都会报true，之前"任何时候都无法下车"就是这个根因。
	FCollisionQueryParams queryParams;
	queryParams.AddIgnoredActor(vehicleElement);

	// 起点用车身中心(Z对齐到下车点的高度)——车辆自己已经被queryParams忽略，这段sweep只会测到
	// 车和下车点之间真正的外部遮挡物(典型情况：车贴墙停，exitOffsetY算出来的点落在墙的另一
	// 侧，起点到终点这一路会先穿过墙体，sweep在墙上就会报true)。
	FVector pathStart = vehicleElement->GetActorLocation();
	pathStart.Z = candidateLocation.Z;

	if (IsExitPathBlocked(GetWorld(), pathStart, candidateLocation, queryParams)) {
		if (GEngine) {
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("下车位置被挡住，无法下车"));
		}
		return;
	}

	float dismountYaw = vehicleElement->GetActorRotation().Yaw;
	APawn* previousPawn = vehicleElement->GetPreviousPawn().Get();
	if (!previousPawn) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::ExitVehicle: previousPawn已失效，玩家将没有可控对象"));
		return;
	}

	// 解除上车时加的距离销毁豁免，恢复正常的流式生成/销毁管理。
	if (ACitizenElement* citizenElement = Cast<ACitizenElement>(previousPawn)) {
		citizenElement->SetDespawnExempt(false);
	}

	// 先恢复显示/碰撞+重新Possess，最后才移动位置——顺序很重要，两层坑：
	// 1. 移动时这个pawn的碰撞如果还是disabled，移动不会触发UpdateOverlaps()（没碰撞的组件
	//    直接跳过重叠检测），车辆的proximity box就感知不到人已经站回来了，"上车"选项再也
	//    不会弹出来(下车后无法重新上车的根因)。必须先启用碰撞再移动。
	// 2. 更隐蔽的一点：OnOverlapBegin里用UGameplayStatics::GetPlayerPawn(0)来判断"靠近的是不是
	//    玩家自己"——如果Possess放在移动之后，那么移动触发UpdateOverlaps、同步派发
	//    OnOverlapBegin的那一刻，controller名义上还占有着车(Possess还没执行)，GetPlayerPawn()
	//    返回的还是载具，proximityBox那边"OtherActor==pawn"直接判不通过，这次本该触发的
	//    "进入了下车点所在的那个box"事件就被白白丢掉了——而这个box后续也不会再有新的重叠变化
	//    (人已经站在里面不会再动)，所以选项一直不出现，必须走远再走近制造一次新的
	//    Begin/End转换才能刷出来，这正是实测反馈的现象(不管下车点落在当前车还是旁边另一辆车
	//    的box里都一样)。Possess提到移动之前，派发事件那一刻GetPlayerPawn()就已经正确指向
	//    previousPawn了。
	previousPawn->SetActorHiddenInGame(false);
	previousPawn->SetActorEnableCollision(true);
	controller->Possess(previousPawn);
	// 只取车身的Yaw站直——侧翻/颠簸之后Pitch/Roll可能是任意角度，人下车应该始终站直，不应该
	// 继承车身的翻滚姿态，照抄原ToggleVehicle DISMOUNT分支的说明。
	previousPawn->SetActorLocationAndRotation(candidateLocation, FRotator(0.f, dismountYaw, 0.f));
	controller->SetControlRotation(FRotator(0.f, dismountYaw, 0.f));

	// 车辆本身不销毁——这次是预置在停车位的真实物件，不是一次性测试对象，下车后原地留着
	// 供下次再上车。
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

	// 四个类别各一个UProceduralMeshComponent，照抄
	// ForeverRoadnetFrameworkComponent::BuildNavigationDebugMesh的组件创建方式。
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

void UForeverTrafficFrameworkComponent::BuildTracks(Traffic* traffic) {
	if (!traffic) return;

	for (auto& [routeName, route] : traffic->GetRoutes()) {
		if (!route || !route->ShouldDrawPath()) continue;

		if (route->GetTrackMesh().empty()) {
			// 这次三种内置线路里唯一drawPath=true的是TrainRoute，trackMesh留空——只打一条Log，
			// 真正沿弧长铺InstancedStaticMeshComponent的逻辑(思路同
			// ForeverRoadnetFrameworkComponent::BuildRoadInstances)留给以后有真实轨道资产的
			// 内容类型实现，见public_transport_plan.md"铺轨"一节。
			debugf("UForeverTrafficFrameworkComponent::BuildTracks: route %s has empty trackMesh, skip laying tracks.\n",
				route->GetName().c_str());
		}
	}
}
