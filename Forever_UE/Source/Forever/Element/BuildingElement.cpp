#include "Element/BuildingElement.h"

#include "Framework/CoreGuard.h"

#include "Framework/ForeverBuildingFrameworkComponent.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverStoryFrameworkComponent.h"
#include "Player/ForeverPlayerController.h"
#include "UI/MeetOptionWidget.h"

#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

#include "map/map.h"
#include "map/building.h"
#include "map/cabin.h"
#include "map/room.h"
#include "map/zone.h"
#include "map/general_geometry.h"
#include "player/player.h"
#include "story/event.h"

#include <algorithm>

#define BUILDING_WORLD_SCALE 1000.f
// 家具碰撞盒的最小高度(cm)：更矮的(地毯)不生成碰撞体，免得绊脚。
#define FURNITURE_COLLISION_MIN_HEIGHT 12.0
// Building没有真实高度数据，楼层cube统一从这个高度起算(相当于室外地坪grade)；它同时是地上1楼地板和地下1楼天花板共用的Z。
// 必须小于地形mesh自己的z-fighting偏移(HEIGHT_EPSILON 0.002地图单位 x BUILDING_WORLD_SCALE = 2.f)：否则地下室天花板会从地面里冒出来一截；
// 又必须大于0，与地形之间留1个世界单位间隙，避免共面z-fighting。这不是彻底修复：目前仍不采样building位置的真实地形高度，
// 只对"可建造的lot地形高度恒为0"成立，将来lot允许在非0高度地形上时需改成按位置采样SampleHeight()(见BuildingElement.md待办)。
#define BUILDING_HEIGHT_EPSILON 1.f
// 墙体厚度(地图单位)，照抄老工程BuildingBase.cpp::ConstructQuad里写死的0.01f。
#define BUILDING_WALL_THICKNESS 0.01f
// 天花板slab整体上移、地板slab整体下移的世界单位(cm)，避免slab顶/底面和墙顶/墙底共面z-fighting，见BuildFloorSection。
#define BUILDING_SLAB_Z_SHIFT 1.f
// UE标准立方体静态网格(/Game/Asset/Common/Mesh/Cube.Cube)的原生边长(cm)——AddCubeInstance/
// SpawnMesh用这个值把它缩放到目标尺寸，缩放系数=目标尺寸/这个原生边长。
#define BUILDING_CUBE_MESH_SIZE 100.f
// building碰撞盒在X/Y/Z三个方向各放大的量(地图单位)，和Room碰撞盒的-0.01配对，保证两者
// 贴合的边界不会因为完全重合而在Overlap判定上抖动。
#define BUILDING_COLLISION_MARGIN 0.01f
// Room碰撞盒在X/Y/Z三个方向各缩小的量(地图单位)，和上面BUILDING_COLLISION_MARGIN的+0.01配对
// (原来在ForeverRoomFrameworkComponent.cpp里叫ROOM_COLLISION_MARGIN，同一个值挪过来)。
#define ROOM_COLLISION_MARGIN 0.01f

using namespace std;

namespace {
	void BuildingAppendQuadDoubleSided(TArray<FVector>& vertices, TArray<int32>& triangles,
		const FVector& v00, const FVector& v10, const FVector& v11, const FVector& v01) {
		int32 base = vertices.Num();
		vertices.Add(v00); vertices.Add(v10); vertices.Add(v11); vertices.Add(v01);
		triangles.Add(base); triangles.Add(base + 2); triangles.Add(base + 1);
		triangles.Add(base); triangles.Add(base + 3); triangles.Add(base + 2);
		triangles.Add(base); triangles.Add(base + 1); triangles.Add(base + 2);
		triangles.Add(base); triangles.Add(base + 2); triangles.Add(base + 3);
	}

	// 按显式给出的中心/半尺寸(世界单位)/Z范围(世界单位)/rotation(弧度)画一个cube——远处LOD
	// (整栋一个box)专用。
	void BuildingAppendFlatBoxExplicit(TArray<FVector>& vertices, TArray<int32>& triangles,
		float cx, float cy, float hx, float hy, float zBottom, float zTop, float rotation) {
		float c = FMath::Cos(rotation);
		float s = FMath::Sin(rotation);

		auto RotatedCorner = [&](float relX, float relY, float z) {
			return FVector(cx + relX * c - relY * s, cy + relX * s + relY * c, z);
			};

		FVector v000 = RotatedCorner(-hx, -hy, zBottom);
		FVector v100 = RotatedCorner(hx, -hy, zBottom);
		FVector v110 = RotatedCorner(hx, hy, zBottom);
		FVector v010 = RotatedCorner(-hx, hy, zBottom);
		FVector v001 = RotatedCorner(-hx, -hy, zTop);
		FVector v101 = RotatedCorner(hx, -hy, zTop);
		FVector v111 = RotatedCorner(hx, hy, zTop);
		FVector v011 = RotatedCorner(-hx, hy, zTop);

		BuildingAppendQuadDoubleSided(vertices, triangles, v001, v101, v111, v011); // 顶
		BuildingAppendQuadDoubleSided(vertices, triangles, v010, v110, v100, v000); // 底
		BuildingAppendQuadDoubleSided(vertices, triangles, v000, v100, v101, v001);
		BuildingAppendQuadDoubleSided(vertices, triangles, v100, v110, v111, v101);
		BuildingAppendQuadDoubleSided(vertices, triangles, v110, v010, v011, v111);
		BuildingAppendQuadDoubleSided(vertices, triangles, v010, v000, v001, v011);
	}

	// 楼体子矩形的世界中心：building的GetPosX/PosY()是整个Quad的中心，楼体相对它有个未旋转局部坐标系下的偏移
	// (GetBodyOffsetX/Y())，要跟着building的rotation一起转。只给远处LOD(整栋一个box)用，楼层内任意点见ComputeWorldPosition。
	void ComputeBodyWorldCenter(const Building& building, float& outCx, float& outCy) {
		float cx = building.GetPosX() * BUILDING_WORLD_SCALE;
		float cy = building.GetPosY() * BUILDING_WORLD_SCALE;
		float rot = building.GetRotation();
		float c = FMath::Cos(rot), s = FMath::Sin(rot);
		float offX = building.GetBodyOffsetX() * BUILDING_WORLD_SCALE;
		float offY = building.GetBodyOffsetY() * BUILDING_WORLD_SCALE;
		outCx = cx + offX * c - offY * s;
		outCy = cy + offX * s + offY * c;
	}

	// 把Floor局部坐标(原点在楼体左下角，未旋转——Core侧Stair/Corridor/Room等InstanciateQuad()
	// 换算出来的坐标系，和Building::LocalToWorld同一套约定)转换成世界坐标(UE世界单位)——和
	// ComputeBodyWorldCenter是同一个公式，只是这里支持楼体内部任意一点，不止楼体中心。
	void ComputeWorldPosition(const Building& building, float localX, float localY,
		float& outWorldX, float& outWorldY) {
		float relX = localX - building.GetBodySizeX() * 0.5f + building.GetBodyOffsetX();
		float relY = localY - building.GetBodySizeY() * 0.5f + building.GetBodyOffsetY();
		float rot = building.GetRotation();
		float c = FMath::Cos(rot), s = FMath::Sin(rot);
		outWorldX = (building.GetPosX() + relX * c - relY * s) * BUILDING_WORLD_SCALE;
		outWorldY = (building.GetPosY() + relX * s + relY * c) * BUILDING_WORLD_SCALE;
	}

	// 整栋建筑的Z范围(世界单位)——最深地下室的底到最高楼层的顶，供远处LOD的单体cube使用。
	void ComputeFullZRange(const Building& building, float& outZBottom, float& outZTop) {
		const vector<float>& heights = building.GetFloorHeights();
		int32 basements = building.GetBasementCount();
		float totalBasementDepth = 0.f;
		for (int32 i = 0; i < basements; i++) totalBasementDepth += heights[i];
		float totalAboveHeight = 0.f;
		for (int32 i = basements; i < static_cast<int32>(heights.size()); i++) totalAboveHeight += heights[i];
		float grade = BUILDING_HEIGHT_EPSILON + building.GetGroundZ() * BUILDING_WORLD_SCALE; // 地坪世界高度(平台地块>0)
		outZBottom = grade - totalBasementDepth * BUILDING_WORLD_SCALE;
		outZTop = grade + totalAboveHeight * BUILDING_WORLD_SCALE;
	}

	// 一个开口(门/窗)在这一侧墙上的位置——x1/x2沿墙方向(局部坐标，相对这面墙起点horizBase的
	// 偏移量，不是绝对局部坐标)，y1/y2是离楼板的高度(世界单位)。门/窗对墙体开洞分段来说完全
	// 一样(都只是缺口)，这次不再摆窗户网格，不需要区分门/窗类型。
	struct FBuildingWallOpening {
		float x1 = 0.f, x2 = 0.f, y1 = 0.f, y2 = 0.f;
	};

	// Stair/Ramp网格朝向 = building世界旋转 + 自己的局部direction(0-3，NORTH=0°/EAST=90°/SOUTH=180°/WEST=270°，已在模板解析时按face预旋转)。
	// WEST/EAST要交换sizeX/sizeY：旋转90°/270°后局部X/Y与世界X/Y互换，缩放必须按局部轴给，否则楼梯长宽比错。
	void ComputeDirectionalMeshTransform(int direction, float sizeX, float sizeY, float buildingRotation,
		float& outSizeX, float& outSizeY, float& outRotation) {
		float localAngleDeg = 0.f;
		bool swapXY = false;
		switch (direction) {
		case FACE_WEST:  localAngleDeg = 270.f; swapXY = true; break;
		case FACE_EAST:  localAngleDeg = 90.f;  swapXY = true; break;
		case FACE_SOUTH: localAngleDeg = 180.f; break;
		case FACE_NORTH: default: localAngleDeg = 0.f; break;
		}
		outSizeX = swapXY ? sizeY : sizeX;
		outSizeY = swapXY ? sizeX : sizeY;
		outRotation = buildingRotation + FMath::DegreesToRadians(localAngleDeg);
	}

	// 电梯轿厢单步推进：状态(z, v)，v带符号(向上为正)，目标可随时改。每个子步：
	// 1) 速度背离目标：以加速度a减速，过零后自然反向加速；
	// 2) 速度朝向目标且剩余距离不够以a刹停：按v²/(2*剩余)精确减速刚好停在目标(所需减速度超过1.5a说明来不及，就按a刹车冲过再折返)；
	// 3) 否则以a加速到不超过cruiseSpeed。速度只在这里累积，所以中途改目标速度始终连续。返回是否已到站(z=targetZ、v=0)。
	bool StepCabin(float& z, float& v, float targetZ, float cruiseSpeed, float accel, float deltaSeconds) {
		constexpr float kArriveDistance = 0.5f;
		int32 steps = FMath::Max(1, FMath::CeilToInt(deltaSeconds * 60.f));
		float h = deltaSeconds / steps;
		for (int32 i = 0; i < steps; i++) {
			float rem = targetZ - z;
			float d = rem >= 0.f ? 1.f : -1.f;
			float remAbs = FMath::Abs(rem);
			if (remAbs <= kArriveDistance && FMath::Abs(v) <= accel * h * 3.f) {
				z = targetZ;
				v = 0.f;
				return true;
			}

			float toward = v * d;
			if (toward < 0.f) {
				v += d * accel * h;
			}
			else if (remAbs <= toward * toward / (2.f * accel)) {
				float need = toward * toward / (2.f * FMath::Max(remAbs, KINDA_SMALL_NUMBER));
				float decel = need <= accel * 1.5f ? need : accel;
				float newToward = toward - decel * h;
				v = d * FMath::Max(newToward, 0.f);
			}
			else {
				v = d * FMath::Min(toward + accel * h, cruiseSpeed);
			}

			float prevRem = rem;
			z += v * h;
			// 一步里正好穿过目标且速度已经很小：直接吸附到站，避免在目标两侧抖。
			if ((targetZ - z) * prevRem <= 0.f && FMath::Abs(v) <= accel * h * 3.f) {
				z = targetZ;
				v = 0.f;
				return true;
			}
		}
		return false;
	}

	// 电梯正面触发盒的深度(地图单位)：从井道开口平面往外1米。
	constexpr float kElevatorFrontDepth = 0.1f;
	// 电梯正面触发盒的宽(相对开口宽)/高(相对层高)比例，居中缩。
	constexpr float kElevatorMarkerScale = 0.8f;
	// 轿厢内触发盒的宽(相对轿厢宽)/高(相对层高)比例，底贴轿厢地板。
	constexpr float kElevatorInnerScale = 0.8f;
	// 轿厢内触发盒沿开口方向的深度(相对轿厢进深)，从轿厢门平面往里。
	constexpr float kElevatorInnerDepthScale = 0.5f;
}

ABuildingElement::ABuildingElement() {
	PrimaryActorTick.bCanEverTick = true;

	elementRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ElementRoot"));
	RootComponent = elementRoot;
	// 这个Actor整个生命周期都不会移动(纯粹是"这栋楼的组件挂载点")，见
	// AForeverFrameworkActor::sceneRoot同一条Mobility注释。
	elementRoot->SetMobility(EComponentMobility::Static);
}

void ABuildingElement::Init(Building* inBuilding, UForeverBuildingFrameworkComponent* inFramework) {
	building = inBuilding;
	framework = inFramework;
	if (!building) return;
	elementsByBuilding.Add(building, this);

	nearFloorCount = building->GetBasementCount() + building->GetLayerCount();
	nearComponentsByFloor.SetNum(nearFloorCount);

	// 名字快照在这里烘焙好，Overlap回调绝不解引用building——和EndPlay处理同一套安全原则。
	buildingNameOnly = building->GetName();
	zoneNameOnly = building->GetParentZone() ? building->GetParentZone()->GetName() : std::string();

	// 外壳(墙/地板/天花板)由Tick在世界生成完后排进队列建出来；进出建筑/房间的检测盒随近处LOD
	// 生成和销毁(见BuildTriggers/BuildFloorExtras/ClearNearSections)，Init里不建。
	// 0层建筑(只有自定义模型/植被，没有楼体)没有外壳，也没有进出检测盒(高度为0)，见BuildingElement.md"自定义模型"一节。
}

TMap<const Building*, TWeakObjectPtr<ABuildingElement>> ABuildingElement::elementsByBuilding;

Building* ABuildingElement::FindBuildingOfBox(UPrimitiveComponent* box) const {
	return (box && box == collisionBox.Get()) ? building : nullptr;
}

ABuildingElement* ABuildingElement::FindForBuilding(const Building* inBuilding) {
	const TWeakObjectPtr<ABuildingElement>* found = elementsByBuilding.Find(inBuilding);
	return found ? found->Get() : nullptr;
}

void ABuildingElement::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	if (building) {
		const TWeakObjectPtr<ABuildingElement>* found = elementsByBuilding.Find(building);
		if (found && found->Get() == this) elementsByBuilding.Remove(building);
	}
	building = nullptr;
	// Cabin*随Map一起失效——Overlap/Refresh回调都先判building。
	for (FCabin& cabin : cabins) cabin.cabin = nullptr;
	Super::EndPlay(EndPlayReason);
}

void ABuildingElement::BuildShell() {
	// 整栋楼所有楼层一次建完，实例都排进框架组件合并的ISM；全城最后一栋建完时框架一次性提交。
	for (int32 i = 0; i < nearFloorCount; i++) BuildFloorSection(i, /*shell=*/true, /*extras=*/false);
	if (framework.IsValid()) framework->NotifyShellBuilt();
}

void ABuildingElement::BuildCollisionBox() {
	if (!building) return;

	// 水平=body矩形(和远处LOD灰box同一个框)，垂直=整栋楼Z范围，三个方向各+0.01(地图单位)——
	// 和Room碰撞盒的-0.01配对，两者贴合的边界不会因为完全重合而在Overlap判定上抖动。
	float cx, cy;
	ComputeBodyWorldCenter(*building, cx, cy);
	float margin = BUILDING_COLLISION_MARGIN * BUILDING_WORLD_SCALE;
	float hx = building->GetBodySizeX() * 0.5f * BUILDING_WORLD_SCALE + margin;
	float hy = building->GetBodySizeY() * 0.5f * BUILDING_WORLD_SCALE + margin;
	float zBottom, zTop;
	ComputeFullZRange(*building, zBottom, zTop);
	zBottom -= margin;
	zTop += margin;
	float cz = (zBottom + zTop) * 0.5f;
	float hz = (zTop - zBottom) * 0.5f;

	UBoxComponent* box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
	box->SetBoxExtent(FVector(hx, hy, hz));
	box->SetCollisionProfileName(TEXT("Trigger"));
	box->SetupAttachment(elementRoot);
	box->SetMobility(EComponentMobility::Static);
	box->SetWorldLocation(FVector(cx, cy, cz));
	box->SetWorldRotation(FRotator(0.f, FMath::RadiansToDegrees(building->GetRotation()), 0.f));
	box->OnComponentBeginOverlap.AddDynamic(this, &ABuildingElement::OnOverlapBegin);
	box->OnComponentEndOverlap.AddDynamic(this, &ABuildingElement::OnOverlapEnd);
	box->RegisterComponent();

	collisionBox = box;
	box->UpdateOverlaps(); // 运行中才建的盒子：玩家已经站在里面时主动刷新一次重叠，补发BeginOverlap
}

namespace {
	// 家具碰撞盒的裁剪：床去掉床头板(只留床本身)，沙发/扶手椅去掉靠背(只留座位+扶手)，否则碰撞盒远高于/厚于视觉上能走近的部分。
	// 网格约定(make_furniture.py，米，-X=背面、+X=正面，XY已居中，原点在底面)：
	//   床：X长2.0~2.1，床头板在-X端厚0.07、高1.05，床身(含枕头)高到0.60；
	//   沙发/扶手椅：进深0.90、总高0.88，靠背在-X端厚0.20，扶手顶0.62。
	// 用占包围盒的比例表示，不依赖网格导入单位。boxMin/boxMax是网格局部系的包围盒(cm)。
	void TrimFurnitureCollisionBox(const FString& meshKey, FVector& boxMin, FVector& boxMax) {
		int32 slash = INDEX_NONE;
		meshKey.FindLastChar(TEXT('/'), slash);
		const FString name = meshKey.Mid(slash + 1);
		const FVector size = boxMax - boxMin;
		if (name.StartsWith(TEXT("Bed_"))) {
			boxMin.X += size.X * 0.035; // 床头板厚0.07/2.0~2.1
			boxMax.Z = boxMin.Z + size.Z * 0.571; // 0.60/1.05
		}
		else if (name.StartsWith(TEXT("Sofa")) || name.StartsWith(TEXT("Armchair"))) {
			boxMin.X += size.X * 0.222; // 靠背厚0.20/0.90
			boxMax.Z = boxMin.Z + size.Z * 0.705; // 0.62/0.88
		}
	}

	UForeverStoryFrameworkComponent* FindBuildingStoryFramework(UWorld* world) {
		AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
			UGameplayStatics::GetActorOfClass(world, AForeverFrameworkActor::StaticClass()));
		return framework ? framework->GetStoryFramework() : nullptr;
	}

	// 进出事件只发给这栋建筑自己的Script：按名字现查(碰撞盒回调里只烘焙了名字，不持有Building*)。
	Script* FindBuildingScript(UWorld* world, const std::string& buildingName) {
		AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
			UGameplayStatics::GetActorOfClass(world, AForeverFrameworkActor::StaticClass()));
		Map* map = framework ? framework->GetMap() : nullptr;
		Building* building = map ? map->GetBuilding(buildingName) : nullptr;
		return building ? building->GetScript() : nullptr;
	}
}

void ABuildingElement::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn) return;
	if (UForeverStoryFrameworkComponent* storyFramework = FindBuildingStoryFramework(GetWorld())) {
		EnterBuildingEvent event(zoneNameOnly, buildingNameOnly);
		storyFramework->BroadcastPlayerLocation(event, "building", buildingNameOnly, /*entering=*/true,
			FindBuildingScript(GetWorld(), buildingNameOnly));
	}
}

void ABuildingElement::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn) return;
	if (UForeverStoryFrameworkComponent* storyFramework = FindBuildingStoryFramework(GetWorld())) {
		LeaveBuildingEvent event(zoneNameOnly, buildingNameOnly);
		storyFramework->BroadcastPlayerLocation(event, "building", buildingNameOnly, /*entering=*/false,
			FindBuildingScript(GetWorld(), buildingNameOnly));
	}
}

void ABuildingElement::BuildRoomCollisionBoxes(int32 onlyLevel) {
	if (!building) return;

	// Room局部矩形按Building::LocalToWorld换算世界中心+building旋转，垂直范围是Room所在楼层的Z范围，
	// 三个方向各-0.01(地图单位)，与building碰撞盒的各+0.01配对，贴合边界不会因完全重合而在Overlap判定上抖动。
	const vector<float>& floorHeights = building->GetFloorHeights();
	int32 basements = building->GetBasementCount();
	float margin = ROOM_COLLISION_MARGIN * BUILDING_WORLD_SCALE;

	for (Room* room : building->GetRooms()) {
		if (!room) continue;

		int32 level = room->GetLayer();
		if (level != onlyLevel) continue;
		int32 idx = basements + level;
		if (idx < 0 || idx >= static_cast<int32>(floorHeights.size())) continue;
		float floorHeight = floorHeights[idx];

		float hx = room->GetSizeX() * 0.5f * BUILDING_WORLD_SCALE - margin;
		float hy = room->GetSizeY() * 0.5f * BUILDING_WORLD_SCALE - margin;
		float floorBaseZ = building->GetFloorWorldZ(level);
		float zBottom = BUILDING_HEIGHT_EPSILON + floorBaseZ * BUILDING_WORLD_SCALE + margin;
		float zTop = BUILDING_HEIGHT_EPSILON + (floorBaseZ + floorHeight) * BUILDING_WORLD_SCALE - margin;
		if (hx <= 0.f || hy <= 0.f || zTop <= zBottom) continue; // 房间/楼层太薄，缩小后变成非法尺寸

		float wx, wy;
		ComputeWorldPosition(*building, room->GetPosX(), room->GetPosY(), wx, wy);
		float cz = (zBottom + zTop) * 0.5f;
		float hz = (zTop - zBottom) * 0.5f;

		UBoxComponent* box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
		box->SetBoxExtent(FVector(hx, hy, hz));
		box->SetCollisionProfileName(TEXT("Trigger"));
		box->SetupAttachment(elementRoot);
		box->SetMobility(EComponentMobility::Static);
		box->SetWorldLocation(FVector(wx, wy, cz));
		box->SetWorldRotation(FRotator(0.f, FMath::RadiansToDegrees(building->GetRotation()), 0.f));
		box->OnComponentBeginOverlap.AddDynamic(this, &ABuildingElement::OnRoomOverlapBegin);
		box->OnComponentEndOverlap.AddDynamic(this, &ABuildingElement::OnRoomOverlapEnd);
		box->RegisterComponent();

		roomBoxLabels.Add(box, room);
		box->UpdateOverlaps(); // 同building盒：补发玩家已站在里面的BeginOverlap
	}
}

void ABuildingElement::OnRoomOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn) return;
	Room** roomPtr = roomBoxLabels.Find(OverlappedComponent);
	if (!roomPtr || !*roomPtr) return;
	Room* room = *roomPtr;

	// 真实物理位置——不是老工程的剧情/场景语义，见Source/Core/player/asset.md、
	// player.h"武器只挂肩膀"一节旁边的说明。假设房间之间不重叠(和BuildCollisionBox的
	// 两侧+/-0.01防抖动设计目的一致)，不做"离开A但还在B里"的重叠区域判断。
	if (AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()))) {
		if (Player* player = frameworkActor->GetPlayer()) {
			player->SetCurrentRoom(room);
		}
		if (UForeverStoryFrameworkComponent* storyFramework = frameworkActor->GetStoryFramework()) {
			EnterRoomEvent event(zoneNameOnly, buildingNameOnly, room->GetName());
			storyFramework->BroadcastPlayerLocation(event, "room", room->GetName(), /*entering=*/true, room->GetScript());
		}
	}
}

void ABuildingElement::OnRoomOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn) return;
	Room** roomPtr = roomBoxLabels.Find(OverlappedComponent);
	if (!roomPtr || !*roomPtr) return;
	Room* room = *roomPtr;

	if (AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()))) {
		if (Player* player = frameworkActor->GetPlayer()) {
			player->SetCurrentRoom(nullptr);
		}
		if (UForeverStoryFrameworkComponent* storyFramework = frameworkActor->GetStoryFramework()) {
			LeaveRoomEvent event(zoneNameOnly, buildingNameOnly, room->GetName());
			storyFramework->BroadcastPlayerLocation(event, "room", room->GetName(), /*entering=*/false, room->GetScript());
		}
	}
}

void ABuildingElement::AddCubeInstance(float centerX, float centerY, float centerZ,
	float sizeX, float sizeY, float sizeZ, float rotation, UMaterialInterface* material) {
	if (!framework.IsValid() || !building) return;
	// 不在这里建组件：实例交给框架组件全城合并的ISM，等全部建筑的外壳排完一次性提交。
	FTransform worldTransform(
		FRotator(0.f, FMath::RadiansToDegrees(rotation), 0.f),
		FVector(centerX, centerY, centerZ),
		FVector(sizeX / BUILDING_CUBE_MESH_SIZE, sizeY / BUILDING_CUBE_MESH_SIZE,
			sizeZ / BUILDING_CUBE_MESH_SIZE));
	framework->QueueShellCube(FVector2D(building->GetPosX(), building->GetPosY()), worldTransform,
		FMath::Cos(rotation), FMath::Sin(rotation), material);
}

void ABuildingElement::AddWallInstance(float centerX, float centerY, float centerZ,
	float sizeX, float sizeY, float sizeZ, float rotation, const FWallMaterialKey& key) {
	if (!framework.IsValid() || !building) return;
	// 同AddCubeInstance。Wall同样约定边长BUILDING_CUBE_MESH_SIZE。
	FTransform worldTransform(
		FRotator(0.f, FMath::RadiansToDegrees(rotation), 0.f),
		FVector(centerX, centerY, centerZ),
		FVector(sizeX / BUILDING_CUBE_MESH_SIZE, sizeY / BUILDING_CUBE_MESH_SIZE,
			sizeZ / BUILDING_CUBE_MESH_SIZE));
	framework->QueueShellWall(FVector2D(building->GetPosX(), building->GetPosY()), worldTransform,
		FMath::Cos(rotation), FMath::Sin(rotation), key);
}

UStaticMeshComponent* ABuildingElement::SpawnMesh(float centerX, float centerY, float centerZ,
	float sizeX, float sizeY, float sizeZ, float rotation, UStaticMesh* mesh, UMaterialInterface* material,
	bool isMovable) {
	if (!mesh) return nullptr;

	UStaticMeshComponent* comp = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	comp->SetStaticMesh(mesh);
	comp->SetupAttachment(elementRoot);
	// isMovable只有电梯轿厢会传true(每帧都要SetWorldLocation播动画)；楼梯/坡道摆好之后不再
	// 移动，必须是Static——和SpawnCube同一个理由，都要在RegisterComponent()之前设好
	// mobility+world transform。
	comp->SetMobility(isMovable ? EComponentMobility::Movable : EComponentMobility::Static);
	comp->SetGenerateOverlapEvents(false);
	comp->SetWorldLocation(FVector(centerX, centerY, centerZ));
	comp->SetWorldRotation(FRotator(0.f, FMath::RadiansToDegrees(rotation), 0.f));
	// 楼梯/坡道/轿厢资产约定包围盒是边长BUILDING_CUBE_MESH_SIZE(100，和SpawnCube的单位立方体
	// Cube.Cube同一个换算基准)的正方体——同一份"资产层面统一缩放"的约定，保证以后换成
	// 不同美术资产，只要包围盒同样是这个单位大小就能直接互相替换，不用改代码。
	comp->SetWorldScale3D(FVector(sizeX / BUILDING_CUBE_MESH_SIZE, sizeY / BUILDING_CUBE_MESH_SIZE,
		sizeZ / BUILDING_CUBE_MESH_SIZE));
	if (material) comp->SetMaterial(0, material);
	comp->RegisterComponent();
	AddInstanceComponent(comp);
	return comp;
}

void ABuildingElement::BuildWallsForElement(float floorBaseZ, float floorHeight,
	float elemCenterX, float elemCenterY, float elemSizeX, float elemSizeY,
	bool wallWest, bool wallEast, bool wallNorth, bool wallSouth,
	const unordered_map<int, vector<array<float, 8>>>& doors,
	const unordered_map<int, vector<array<float, 8>>>& windows,
	UMaterialInterface* wallMaterial, const WallStyle* styles) {
	// floorBaseZ是Building::GetFloorWorldZ(level)的值：地图单位、含地块平台高度(GetGroundZ)的楼层底部
	// 高度(basements为负、地上楼层为正，见building.h)，不含BUILDING_HEIGHT_EPSILON这个纯
	// 渲染层的"避免和地形共面z-fighting"偏移——这里换算世界Z时要把grade偏移加回来，和
	// ComputeFullZRange同一套约定。
	float rotation = building->GetRotation();
	float floorBottomWorldZ = BUILDING_HEIGHT_EPSILON + floorBaseZ * BUILDING_WORLD_SCALE;
	float floorTopWorldZ = BUILDING_HEIGHT_EPSILON + (floorBaseZ + floorHeight) * BUILDING_WORLD_SCALE;

	float halfX = elemSizeX * 0.5f, halfY = elemSizeY * 0.5f;
	struct FFaceInfo { bool enabled; int32 dir; float horizBase; float horizSpan; bool alongY; float acrossFixed; };
	FFaceInfo faces[4] = {
		{ wallWest,  FACE_WEST,  elemCenterY - halfY, elemSizeY, true,  elemCenterX - halfX },
		{ wallEast,  FACE_EAST,  elemCenterY - halfY, elemSizeY, true,  elemCenterX + halfX },
		{ wallNorth, FACE_NORTH, elemCenterX - halfX, elemSizeX, false, elemCenterY - halfY },
		{ wallSouth, FACE_SOUTH, elemCenterX - halfX, elemSizeX, false, elemCenterY + halfY },
	};

	for (const FFaceInfo& face : faces) {
		if (!face.enabled || face.horizSpan <= 0.f) continue;

		vector<FBuildingWallOpening> openings;
		auto collect = [&](const unordered_map<int, vector<array<float, 8>>>& holes) {
			auto it = holes.find(face.dir);
			if (it == holes.end()) return;
			for (const array<float, 8>& p : it->second) {
				FBuildingWallOpening opening;
				opening.x1 = p[0] * face.horizSpan + p[1];
				opening.x2 = p[4] * face.horizSpan + p[5];
				opening.y1 = p[2] * floorHeight + p[3];
				opening.y2 = p[6] * floorHeight + p[7];
				openings.push_back(opening);
			}
			};
		collect(doors);
		collect(windows);

		// 按沿墙方向的localX/localY(alongY决定跨墙坐标固定用acrossFixed，沿墙坐标用segCenter)
		// 生成一段墙体cube，zBot/zTop是世界坐标——照抄老工程BuildingBase.cpp::ConstructQuad
		// 的makePos/makeSize思路，只是这次每段都是独立组件，不是往同一个PMC里追加顶点。
		// 这面墙有标签对应的材质就走Wall(东南西北四个侧面各自的材质，没配的侧面退回这层墙材质，上下固定白色)，
		// Wall资产还不存在时退回普通Cube；整面墙共用同一个key，门窗洞口切出的每一段都继承它。
		FWallMaterialKey wallKey;
		bool useWallMesh = false;
		if (styles && styles[face.dir].HasMaterial()) {
			// Wall网格(框架缓存，没做出来就是nullptr，退回普通Cube)。
			UStaticMesh* wallMesh = framework->ResolveMesh("/Game/Asset/Building/Mesh/Wall.Wall", nullptr);
			if (wallMesh) {
				const WallStyle& style = styles[face.dir];
				// Wall槽位 0东 1南 2西 3北 对应WallStyle下标(FACE_DIRECTION) 1东 3南 0西 2北。
				static const int32 kSideOfSlot[4] = { FACE_EAST, FACE_SOUTH, FACE_WEST, FACE_NORTH };
				auto resolve = [&](const std::string& path) -> UMaterialInterface* {
					UMaterialInterface* found = framework->ResolveMaterial(path, nullptr);
					return found ? found : wallMaterial;
				};
				for (int32 slot = 0; slot < 4; slot++) {
					wallKey.slots[slot] = resolve(style.materials[kSideOfSlot[slot]]);
				}
				wallKey.slots[4] = resolve("/Game/Asset/Building/Material/White.White");
				useWallMesh = true;
			}
		}

		auto spawnWallBox = [&](float segStart, float segEnd, float zBot, float zTop) {
			if (segEnd <= segStart || zTop <= zBot) return;
			float segCenter = (segStart + segEnd) * 0.5f;
			float segLen = segEnd - segStart;
			// 西/北边的墙往+方向、东/南边的墙往-方向，即朝元素内部偏移。
			// 偏移量=1/4墙厚：相邻元素共用的边上两堵墙中心错开半个墙厚、部分重叠但不共面，材质不同时不会z-fighting。
			float inward = (face.dir == FACE_WEST || face.dir == FACE_NORTH ? 1.f : -1.f) * BUILDING_WALL_THICKNESS * 0.25f;
			float across = face.acrossFixed + inward;
			float localX = face.alongY ? across : segCenter;
			float localY = face.alongY ? segCenter : across;
			float wx, wy;
			ComputeWorldPosition(*building, localX, localY, wx, wy);
			float sizeXWorld = (face.alongY ? BUILDING_WALL_THICKNESS : segLen) * BUILDING_WORLD_SCALE;
			float sizeYWorld = (face.alongY ? segLen : BUILDING_WALL_THICKNESS) * BUILDING_WORLD_SCALE;
			float wz = (zBot + zTop) * 0.5f;
			float sizeZWorld = zTop - zBot;
			if (useWallMesh) AddWallInstance(wx, wy, wz, sizeXWorld, sizeYWorld, sizeZWorld, rotation, wallKey);
			else AddCubeInstance(wx, wy, wz, sizeXWorld, sizeYWorld, sizeZWorld, rotation, wallMaterial);
			};

		if (openings.empty()) {
			spawnWallBox(face.horizBase, face.horizBase + face.horizSpan, floorBottomWorldZ, floorTopWorldZ);
			continue;
		}

		sort(openings.begin(), openings.end(),
			[](const FBuildingWallOpening& a, const FBuildingWallOpening& b) { return a.x1 < b.x1; });

		float prevX = 0.f;
		for (const FBuildingWallOpening& opening : openings) {
			float segW = opening.x1 - prevX;
			if (segW > 0.f) {
				spawnWallBox(face.horizBase + prevX, face.horizBase + opening.x1, floorBottomWorldZ, floorTopWorldZ);
			}
			// y1/y2照抄老工程BuildingBase.cpp::ConstructQuad::processFace的约定——从天花板往下
			// 量，不是从地板往上量：y1是"开口顶到天花板"这段实心墙(过梁/门楣)的厚度，直接贴着
			// 天花板往下铺；y2是"开口底到天花板"的距离，(floorHeight-y2)才是"开口底到地板"这段
			// 实心墙(门槛/窗台)的厚度，贴着地板往上铺。
			if (opening.y1 > 0.f) {
				spawnWallBox(face.horizBase + opening.x1, face.horizBase + opening.x2,
					floorTopWorldZ - opening.y1 * BUILDING_WORLD_SCALE, floorTopWorldZ);
			}
			float thresholdH = floorHeight - opening.y2;
			if (thresholdH > 0.f) {
				spawnWallBox(face.horizBase + opening.x1, face.horizBase + opening.x2,
					floorBottomWorldZ, floorBottomWorldZ + thresholdH * BUILDING_WORLD_SCALE);
			}
			prevX = opening.x2;
		}
		float trailingW = face.horizSpan - prevX;
		if (trailingW > 0.f) {
			spawnWallBox(face.horizBase + prevX, face.horizBase + face.horizSpan, floorBottomWorldZ, floorTopWorldZ);
		}
		// 窗户这次不摆任何网格——窗户资产有问题，用户明确要求直接删掉窗户显示逻辑，只保留
		// 开洞几何(和门一样，只是墙上的一个缺口，不生成任何东西)。
	}
}

void ABuildingElement::BuildFloorSection(int32 floorIndex, bool shell, bool extras) {
	if (!building || !framework.IsValid()) return;
	if (floorIndex < 0 || floorIndex >= nearFloorCount) return;

	int32 level = floorIndex - building->GetBasementCount();
	const Floor* floor = building->GetFloor(level);
	if (!floor) return;

	// floorBaseZ(地图单位，含地块平台高度、不含BUILDING_HEIGHT_EPSILON这个纯渲染层偏移)直接
	// 用Core侧现成的GetFloorWorldZ()(=GetGroundZ()+GetFloorBaseZ())，不用像ComputeFullZRange那样另外反推。
	float floorBaseZ = building->GetFloorWorldZ(level);
	float floorHeight = building->GetFloorHeights()[floorIndex];

	// 材质/网格：按mod在AssignFloor里给这一层指定的FloorAssetSpec，留空用框架组件的默认值。
	UMaterialInstanceDynamic* wallMaterial = framework->GetDefaultWallMaterial();
	UMaterialInstanceDynamic* floorMaterial = framework->GetDefaultFloorMaterial();
	UMaterialInstanceDynamic* ceilingMaterial = framework->GetDefaultCeilingMaterial();
	UStaticMesh* stairMesh = framework->GetDefaultStairMesh();
	UStaticMesh* rampMesh = framework->GetDefaultRampMesh();
	// 这一层的FloorAssetSpec::buildShell==false(外观由AddDecoration的成品模型提供，比如Mohar)：
	// 不构造墙壁(含楼梯/坡道/电梯井道墙)、地板、天花板，楼梯/坡道网格照常摆，见
	// BuildingElement.md"buildShell"一节。
	bool buildShell = true;
	if (const FloorLayoutSpec* spec = building->GetFloorSpec(level)) {
		const FloorAssetSpec& assets = spec->assets;
		wallMaterial = framework->ResolveMaterial(assets.wallMaterial, wallMaterial);
		floorMaterial = framework->ResolveMaterial(assets.floorMaterial, floorMaterial);
		ceilingMaterial = framework->ResolveMaterial(assets.ceilingMaterial, ceilingMaterial);
		stairMesh = framework->ResolveMesh(assets.stairMeshPath, stairMesh);
		rampMesh = framework->ResolveMesh(assets.rampMeshPath, rampMesh);
		buildShell = assets.buildShell;
	}

	float rotation = building->GetRotation();
	auto& slot = nearComponentsByFloor[floorIndex];
	if (extras) BuildRoomCollisionBoxes(level);

	// 楼梯/坡道：井道墙体(和corridor/single/row同一套BuildWallsForElement，属于外壳)+按该层资产指定的
	// 3D网格摆一个实体(extras，只近处)；电梯只有井道墙体，不摆网格(轿厢单独由BuildElevatorCabinsForBuilding
	// 处理)。
	const bool buildWalls = shell && buildShell;
	for (const Stair& stair : floor->GetStairs()) {
		if (buildWalls) BuildWallsForElement(floorBaseZ, floorHeight, stair.GetPosX(), stair.GetPosY(),
			stair.GetSizeX(), stair.GetSizeY(),
			stair.GetWall(FACE_WEST), stair.GetWall(FACE_EAST), stair.GetWall(FACE_NORTH), stair.GetWall(FACE_SOUTH),
			{}, {}, wallMaterial, stair.GetWallStyles());
		float wx, wy;
		ComputeWorldPosition(*building, stair.GetPosX(), stair.GetPosY(), wx, wy);
		float wz = floorBaseZ * BUILDING_WORLD_SCALE + BUILDING_HEIGHT_EPSILON;
		// 楼梯网格按.layout模板里Stair实际声明的footprint尺寸缩放(X/Y)，Z缩放到整层高度
		// (楼梯本来就是连接上下相邻楼层的)——地图单位换算成世界单位后再传给SpawnMesh；朝向
		// 除了building自身的世界旋转，还要叠加这个楼梯自己的局部direction。
		if (!extras) continue;
		float meshSizeX, meshSizeY, meshRotation;
		ComputeDirectionalMeshTransform(stair.GetDirection(), stair.GetSizeX(), stair.GetSizeY(), rotation,
			meshSizeX, meshSizeY, meshRotation);
		if (UStaticMeshComponent* comp = SpawnMesh(wx, wy, wz,
			meshSizeX * BUILDING_WORLD_SCALE, meshSizeY * BUILDING_WORLD_SCALE,
			floorHeight * BUILDING_WORLD_SCALE, meshRotation, stairMesh, nullptr)) {
			slot.Add(comp);
		}
	}
	for (const Ramp& ramp : floor->GetRamps()) {
		if (buildWalls) BuildWallsForElement(floorBaseZ, floorHeight, ramp.GetPosX(), ramp.GetPosY(),
			ramp.GetSizeX(), ramp.GetSizeY(),
			ramp.GetWall(FACE_WEST), ramp.GetWall(FACE_EAST), ramp.GetWall(FACE_NORTH), ramp.GetWall(FACE_SOUTH),
			{}, {}, wallMaterial, ramp.GetWallStyles());
		float wx, wy;
		ComputeWorldPosition(*building, ramp.GetPosX(), ramp.GetPosY(), wx, wy);
		float wz = floorBaseZ * BUILDING_WORLD_SCALE + BUILDING_HEIGHT_EPSILON;
		// 同上：坡道网格按Ramp实际声明的footprint尺寸(X/Y)+整层高度(Z)缩放，朝向叠加自己的
		// 局部direction。
		if (!extras) continue;
		float meshSizeX, meshSizeY, meshRotation;
		ComputeDirectionalMeshTransform(ramp.GetDirection(), ramp.GetSizeX(), ramp.GetSizeY(), rotation,
			meshSizeX, meshSizeY, meshRotation);
		if (UStaticMeshComponent* comp = SpawnMesh(wx, wy, wz,
			meshSizeX * BUILDING_WORLD_SCALE, meshSizeY * BUILDING_WORLD_SCALE,
			floorHeight * BUILDING_WORLD_SCALE, meshRotation, rampMesh, nullptr)) {
			slot.Add(comp);
		}
	}
	for (const Elevator& elevator : floor->GetElevators()) {
		if (buildWalls) BuildWallsForElement(floorBaseZ, floorHeight, elevator.GetPosX(), elevator.GetPosY(),
			elevator.GetSizeX(), elevator.GetSizeY(),
			elevator.GetWall(FACE_WEST), elevator.GetWall(FACE_EAST), elevator.GetWall(FACE_NORTH), elevator.GetWall(FACE_SOUTH),
			{}, {}, wallMaterial, elevator.GetWallStyles());
	}
	for (const Corridor& corridor : floor->GetCorridors()) {
		if (buildWalls) BuildWallsForElement(floorBaseZ, floorHeight, corridor.GetPosX(), corridor.GetPosY(),
			corridor.GetSizeX(), corridor.GetSizeY(),
			corridor.GetWall(FACE_WEST), corridor.GetWall(FACE_EAST), corridor.GetWall(FACE_NORTH), corridor.GetWall(FACE_SOUTH),
			corridor.GetDoors(), corridor.GetWindows(), wallMaterial, corridor.GetWallStyles());
	}
	// 槽位实例化成Room后门/窗/朝向已转移到Room身上，槽位数据不再是最新的，且一个row槽位会被ArrangeRow切成多个Room，
	// 所以遍历building->GetRooms()按GetLayer()==level筛选，不直接用Floor::GetSingles()/GetRows()。
	for (Room* room : building->GetRooms()) {
		if (!room || room->GetLayer() != level) continue;
		if (buildWalls) BuildWallsForElement(floorBaseZ, floorHeight, room->GetPosX(), room->GetPosY(),
			room->GetSizeX(), room->GetSizeY(),
			true, true, true, true, // Single/Row隐含四面都有墙
			room->GetDoors(), room->GetWindows(), wallMaterial, room->GetWallStyles());
	}

	// 门不在这里建——门由ForeverDoorFrameworkComponent按玩家距离独立流式加载，和建筑LOD无关，
	// 见ForeverDoorFrameworkComponent.md。门洞本身仍然由上面的BuildWallsForElement照常挖出。

	// 地板/天花板：老工程两层独立薄slab的做法——地板贴地坪往上一点点，天花板贴楼层顶往下
	// 一点点，中间留一条很窄的缝，避免和上/下相邻楼层的对应slab共面z-fighting。
	constexpr float kSlabThickness = 0.02f; // 地图单位，照抄老工程Ceiling/Ground的0.02f厚度
	constexpr float kMinSlabSize = 1.f; // 世界单位(cm)：地板/天花板矩形任一边小于它就当作退化，不摆
	float floorBottomWorld = BUILDING_HEIGHT_EPSILON + floorBaseZ * BUILDING_WORLD_SCALE;
	float floorTopWorld = BUILDING_HEIGHT_EPSILON + (floorBaseZ + floorHeight) * BUILDING_WORLD_SCALE;
	if (buildWalls) for (const Ground& ground : floor->GetGrounds()) {
		float wx, wy;
		ComputeWorldPosition(*building, ground.GetPosX(), ground.GetPosY(), wx, wy);
		float wz = floorBottomWorld + kSlabThickness * 0.5f * BUILDING_WORLD_SCALE - BUILDING_SLAB_Z_SHIFT;
		float sizeX = ground.GetSizeX() * BUILDING_WORLD_SCALE, sizeY = ground.GetSizeY() * BUILDING_WORLD_SCALE;
		float sizeZ = kSlabThickness * BUILDING_WORLD_SCALE;
		if (sizeX <= kMinSlabSize || sizeY <= kMinSlabSize) continue; // 零宽/零高的矩形不摆：缩放为0的BlockAll cube会留下看不见却挡路的薄片
		// 带标签的地板用mod登记的材质实例，没有/加载失败退回这一层的地板材质(默认白色)。
		AddCubeInstance(wx, wy, wz, sizeX, sizeY, sizeZ, rotation, framework->ResolveMaterial(ground.GetMaterial(), floorMaterial));
	}
	if (buildWalls) for (const Ceiling& ceiling : floor->GetCeilings()) {
		float wx, wy;
		ComputeWorldPosition(*building, ceiling.GetPosX(), ceiling.GetPosY(), wx, wy);
		float wz = floorTopWorld - kSlabThickness * 0.5f * BUILDING_WORLD_SCALE + BUILDING_SLAB_Z_SHIFT;
		float sizeX = ceiling.GetSizeX() * BUILDING_WORLD_SCALE, sizeY = ceiling.GetSizeY() * BUILDING_WORLD_SCALE;
		float sizeZ = kSlabThickness * BUILDING_WORLD_SCALE;
		if (sizeX <= kMinSlabSize || sizeY <= kMinSlabSize) continue; // 同地板
		AddCubeInstance(wx, wy, wz, sizeX, sizeY, sizeZ, rotation, framework->ResolveMaterial(ceiling.GetMaterial(), ceilingMaterial));
	}

	// 1楼：把楼体(body)与building整块quad之间的回形空隙用4块cube铺上mod指定的材质，高度/厚度同1楼地面。
	const std::string& gapPath = building->GetGapMaterialPath();
	if (shell && level == 0 && !gapPath.empty()) {
		// 在building quad中心的未旋转局部系里：quad半宽半高、楼体矩形。
		float quadHalfX = building->GetSizeX() * 0.5f, quadHalfY = building->GetSizeY() * 0.5f;
		float bodyX0 = building->GetBodyOffsetX() - building->GetBodySizeX() * 0.5f;
		float bodyX1 = building->GetBodyOffsetX() + building->GetBodySizeX() * 0.5f;
		float bodyY0 = building->GetBodyOffsetY() - building->GetBodySizeY() * 0.5f;
		float bodyY1 = building->GetBodyOffsetY() + building->GetBodySizeY() * 0.5f;
		float c = FMath::Cos(rotation), s = FMath::Sin(rotation);
		// 高度/厚度与1楼地面完全一致(下表面0cm、上表面20cm)。
		float wz = floorBottomWorld + kSlabThickness * 0.5f * BUILDING_WORLD_SCALE - BUILDING_SLAB_Z_SHIFT;
		float sizeZ = kSlabThickness * BUILDING_WORLD_SCALE;
		UMaterialInterface* gapMaterial = framework->ResolveMaterial(gapPath, floorMaterial);
		// 局部矩形[x0,x1]x[y0,y1]一块cube；左右两条占满quad高度，上下两条只占楼体宽度，四块不重叠。
		auto addGapStrip = [&](float x0, float x1, float y0, float y1) {
			if (x1 - x0 <= 0.f || y1 - y0 <= 0.f) return;
			float lx = (x0 + x1) * 0.5f, ly = (y0 + y1) * 0.5f;
			float wx = (building->GetPosX() + lx * c - ly * s) * BUILDING_WORLD_SCALE;
			float wy = (building->GetPosY() + lx * s + ly * c) * BUILDING_WORLD_SCALE;
			AddCubeInstance(wx, wy, wz, (x1 - x0) * BUILDING_WORLD_SCALE, (y1 - y0) * BUILDING_WORLD_SCALE,
				sizeZ, rotation, gapMaterial);
		};
		addGapStrip(-quadHalfX, bodyX0, -quadHalfY, quadHalfY);
		addGapStrip(bodyX1, quadHalfX, -quadHalfY, quadHalfY);
		addGapStrip(bodyX0, bodyX1, -quadHalfY, bodyY0);
		addGapStrip(bodyX0, bodyX1, bodyY1, quadHalfY);
	}

}

void ABuildingElement::BuildElevatorCabinsForBuilding() {
	if (!building || !framework.IsValid()) return;
	float rotation = building->GetRotation();
	const vector<float>& floorHeights = building->GetFloorHeights();
	int32 basements = building->GetBasementCount();

	for (Cabin* coreCabin : building->GetCabinEntities()) {
		if (!coreCabin) continue;
		if (coreCabin->GetSpecIndex() < 0 || coreCabin->GetSpecIndex() >= static_cast<int32>(building->GetCabins().size())) continue;
		const ElevatorCabinSpec& spec = building->GetCabins()[coreCabin->GetSpecIndex()];
		int32 minIdx = basements + spec.minFloor;
		int32 floorCount = static_cast<int32>(floorHeights.size());
		if (minIdx < 0 || minIdx >= floorCount) continue;

		const Floor* floor = building->GetFloor(spec.minFloor);
		if (!floor) continue;
		const vector<Elevator>& elevators = floor->GetElevators();
		if (spec.shaftIndex < 0 || spec.shaftIndex >= static_cast<int32>(elevators.size())) continue;
		const Elevator& elevator = elevators[spec.shaftIndex];

		// 轿厢mesh和楼梯/坡道同一个约定([0,1]单位包围盒，按实际尺寸缩放+按方向旋转)，
		// 直接复用ComputeDirectionalMeshTransform；Z缩放用轿厢最低楼层(minFloor)的层高。
		float meshSizeX, meshSizeY, meshRotation;
		ComputeDirectionalMeshTransform(elevator.GetDirection(), elevator.GetSizeX(), elevator.GetSizeY(),
			rotation, meshSizeX, meshSizeY, meshRotation);

		float wx, wy;
		ComputeWorldPosition(*building, elevator.GetPosX(), elevator.GetPosY(), wx, wy);

		float minFloorHeight = floorHeights[minIdx];
		// 轿厢从Core记录的当前高度生成(LOD重建/运行中都不会跳回最低层)。
		float cabinZ = coreCabin->GetCurrentZ() * BUILDING_WORLD_SCALE + BUILDING_HEIGHT_EPSILON;

		UStaticMesh* cabinMesh = framework->ResolveMesh(spec.cabinMeshPath, framework->GetDefaultCabinMesh());
		// isMovable=true——轿厢要随行程SetWorldLocation，必须是Movable。
		// 水平方向缩到kCabinScale：轿厢的侧面和井道墙面重合会z-fighting，缩一点点错开。
		constexpr float kCabinScale = 0.95f;
		// 竖直方向不按层高比例缩，而是固定留kCabinFloorLift(UE单位，cm)：轿厢底面比楼层地板面高这么多，顶面比天花板低这么多，
		// 避免和地板/天花板共面z-fighting；层高很高时也只抬几厘米，玩家走得进去(原先按层高的5%抬，层高6米就抬30cm，迈不上去)。
		// 地板面本身已经含BUILDING_HEIGHT_EPSILON(1cm)，轿厢底面实际比地板面高kCabinFloorLift+1cm。
		constexpr float kCabinFloorLift = 2.f;
		float cabinMeshZOffset = kCabinFloorLift;
		float cabinMeshHeight = FMath::Max(minFloorHeight * BUILDING_WORLD_SCALE - 2.f * kCabinFloorLift, 1.f);
		UStaticMeshComponent* comp = SpawnMesh(wx, wy, cabinZ + cabinMeshZOffset,
			meshSizeX * BUILDING_WORLD_SCALE * kCabinScale, meshSizeY * BUILDING_WORLD_SCALE * kCabinScale,
			cabinMeshHeight,
			meshRotation, cabinMesh, nullptr, /*isMovable=*/true);
		if (!comp) continue;

		FCabin cabin;
		cabin.cabin = coreCabin;
		cabin.comp = comp;
		cabin.meshZOffset = cabinMeshZOffset;
		cabin.worldX = wx;
		cabin.worldY = wy;
		cabin.rotation = meshRotation;

		// 交互快照(Overlap回调只读这份)。
		cabin.interactNameOnly = UTF8_TO_TCHAR(coreCabin->GetInteractName().c_str());
		cabin.hasScript = coreCabin->GetScript() != nullptr;
		for (const std::string& option : coreCabin->GetOptions()) cabin.cachedOptions.Add(UTF8_TO_TCHAR(option.c_str()));

		// 轿厢内触发盒：宽和高取轿厢的kElevatorInnerScale，沿开口方向的深度取轿厢进深的
		// kElevatorInnerDepthScale并贴着轿厢门平面(和正面触发盒之间不留缝)——玩家走到轿厢最里面时不弹楼层选项，免得挡视线。在building局部坐标里按开口方向算，
		// 和正面触发盒同一套。跟着轿厢移动。
		float innerHeight = minFloorHeight * kElevatorInnerScale * BUILDING_WORLD_SCALE;
		cabin.innerHalfZ = innerHeight * 0.5f;
		{
			float innerLocalX = elevator.GetPosX(), innerLocalY = elevator.GetPosY();
			float innerSizeX = elevator.GetSizeX() * kElevatorInnerScale, innerSizeY = elevator.GetSizeY() * kElevatorInnerScale;
			switch (elevator.GetDirection()) {
			case FACE_WEST:  innerSizeX = elevator.GetSizeX() * kElevatorInnerDepthScale; innerLocalX -= (elevator.GetSizeX() - innerSizeX) * 0.5f; break;
			case FACE_EAST:  innerSizeX = elevator.GetSizeX() * kElevatorInnerDepthScale; innerLocalX += (elevator.GetSizeX() - innerSizeX) * 0.5f; break;
			case FACE_NORTH: innerSizeY = elevator.GetSizeY() * kElevatorInnerDepthScale; innerLocalY -= (elevator.GetSizeY() - innerSizeY) * 0.5f; break;
			default:         innerSizeY = elevator.GetSizeY() * kElevatorInnerDepthScale; innerLocalY += (elevator.GetSizeY() - innerSizeY) * 0.5f; break; // FACE_SOUTH
			}
			ComputeWorldPosition(*building, innerLocalX, innerLocalY, cabin.innerX, cabin.innerY);
			cabin.innerTrigger = SpawnCabinTrigger(cabin.innerX, cabin.innerY, cabinZ + cabin.innerHalfZ,
				innerSizeX * BUILDING_WORLD_SCALE, innerSizeY * BUILDING_WORLD_SCALE, innerHeight,
				rotation, /*isMovable=*/true);
		}

		// 各层电梯正面触发盒：这台电梯负责的每一层，用那一层模板自己的这口井定位，在开口
		// (Elevator::GetDirection()那一侧，模板里没有墙的一面)外放一块宽=开口宽×kElevatorMarkerScale、
		// 高=层高×kElevatorMarkerScale、深kElevatorFrontDepth(1米，贴着开口平面)的盒子。某层模板没有这口井就跳过。
		for (int32 level = spec.minFloor; level <= spec.maxFloor; level++) {
			int32 idx = basements + level;
			if (idx < 0 || idx >= floorCount) continue;
			const Floor* levelFloor = building->GetFloor(level);
			if (!levelFloor) continue;
			const vector<Elevator>& levelElevators = levelFloor->GetElevators();
			if (spec.shaftIndex < 0 || spec.shaftIndex >= static_cast<int32>(levelElevators.size())) continue;
			const Elevator& shaft = levelElevators[spec.shaftIndex];

			float halfX = shaft.GetSizeX() * 0.5f, halfY = shaft.GetSizeY() * 0.5f;
			float localX = shaft.GetPosX(), localY = shaft.GetPosY();
			float boxSizeX = shaft.GetSizeX() * kElevatorMarkerScale, boxSizeY = shaft.GetSizeY() * kElevatorMarkerScale;
			switch (shaft.GetDirection()) {
			case FACE_WEST:  localX -= halfX + kElevatorFrontDepth * 0.5f; boxSizeX = kElevatorFrontDepth; break;
			case FACE_EAST:  localX += halfX + kElevatorFrontDepth * 0.5f; boxSizeX = kElevatorFrontDepth; break;
			case FACE_NORTH: localY -= halfY + kElevatorFrontDepth * 0.5f; boxSizeY = kElevatorFrontDepth; break;
			default:         localY += halfY + kElevatorFrontDepth * 0.5f; boxSizeY = kElevatorFrontDepth; break; // FACE_SOUTH
			}
			float fx, fy;
			ComputeWorldPosition(*building, localX, localY, fx, fy);
			float levelHeight = floorHeights[idx];
			float fz = (building->GetFloorWorldZ(level) + levelHeight * 0.5f) * BUILDING_WORLD_SCALE + BUILDING_HEIGHT_EPSILON;
			if (UBoxComponent* trigger = SpawnCabinTrigger(fx, fy, fz,
				boxSizeX * BUILDING_WORLD_SCALE, boxSizeY * BUILDING_WORLD_SCALE,
				levelHeight * kElevatorMarkerScale * BUILDING_WORLD_SCALE, rotation, /*isMovable=*/false)) {
				cabin.frontTriggers.Add(trigger);
			}
		}

		cabins.Add(cabin);
	}
}

UBoxComponent* ABuildingElement::SpawnCabinTrigger(float centerX, float centerY, float centerZ,
	float sizeX, float sizeY, float sizeZ, float rotation, bool isMovable) {
	UBoxComponent* box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
	box->SetBoxExtent(FVector(sizeX * 0.5f, sizeY * 0.5f, sizeZ * 0.5f));
	box->SetCollisionProfileName(TEXT("Trigger"));
	box->SetupAttachment(elementRoot);
	// 轿厢内的触发盒跟着轿厢SetWorldLocation，必须是Movable(移动时UE会自动更新Overlap)。
	box->SetMobility(isMovable ? EComponentMobility::Movable : EComponentMobility::Static);
	box->SetWorldLocation(FVector(centerX, centerY, centerZ));
	box->SetWorldRotation(FRotator(0.f, FMath::RadiansToDegrees(rotation), 0.f));
	box->OnComponentBeginOverlap.AddDynamic(this, &ABuildingElement::OnCabinOverlapBegin);
	box->OnComponentEndOverlap.AddDynamic(this, &ABuildingElement::OnCabinOverlapEnd);
	box->RegisterComponent();
	AddInstanceComponent(box);
	return box;
}

ABuildingElement::FCabin* ABuildingElement::FindCabinByTrigger(const UPrimitiveComponent* trigger) {
	if (!trigger) return nullptr;
	for (FCabin& cabin : cabins) {
		if (cabin.innerTrigger == trigger) return &cabin;
		for (const UBoxComponent* front : cabin.frontTriggers) {
			if (front == trigger) return &cabin;
		}
	}
	return nullptr;
}

void ABuildingElement::ShowCabinOptions(const FCabin& cabin) {
	if (!cabin.hasScript || cabin.cachedOptions.Num() == 0) return;
	AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	UMeetOptionWidget* meetOption = playerController ? playerController->GetMeetOptionWidget() : nullptr;
	if (!meetOption) return;
	AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
	for (int32 idx = 0; idx < cabin.cachedOptions.Num(); idx++) {
		meetOption->AddOption(frameworkActor, cabin.cachedOptions[idx], cabin.interactNameOnly, idx);
	}
}

void ABuildingElement::HideCabinOptions(const FCabin& cabin) {
	AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (UMeetOptionWidget* meetOption = playerController ? playerController->GetMeetOptionWidget() : nullptr) {
		meetOption->RemoveName(cabin.interactNameOnly);
	}
}

void ABuildingElement::OnCabinOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	if (!building || !OtherActor || OtherActor != UGameplayStatics::GetPlayerPawn(GetWorld(), 0)) return;
	FCabin* cabin = FindCabinByTrigger(OverlappedComponent);
	if (!cabin) return;
	// 门口和轿厢内的触发盒可能同时压到，只在第一个进入时弹出选项，避免同一组选项出现两遍。
	if (cabin->playerOverlaps++ == 0) ShowCabinOptions(*cabin);
}

void ABuildingElement::OnCabinOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
	if (!OtherActor || OtherActor != UGameplayStatics::GetPlayerPawn(GetWorld(), 0)) return;
	FCabin* cabin = FindCabinByTrigger(OverlappedComponent);
	if (!cabin || cabin->playerOverlaps <= 0) return;
	if (--cabin->playerOverlaps == 0) HideCabinOptions(*cabin);
}

void ABuildingElement::RefreshCabinOptions(const Cabin* coreCabin) {
	if (!building || !coreCabin) return;
	for (FCabin& cabin : cabins) {
		if (cabin.cabin != coreCabin) continue;
		cabin.hasScript = coreCabin->GetScript() != nullptr;
		cabin.cachedOptions.Reset();
		for (const std::string& option : coreCabin->GetOptions()) cabin.cachedOptions.Add(UTF8_TO_TCHAR(option.c_str()));
		// 玩家正压着触发盒：整组换掉(原来没有选项时屏幕上没有这个名字，ReplaceOptions会直接跳过，
		// 所以这里先收起再重新弹出)。
		if (cabin.playerOverlaps > 0) {
			HideCabinOptions(cabin);
			ShowCabinOptions(cabin);
		}
	}
}

void ABuildingElement::ClearNearSections() {
	// 进出建筑/房间的检测盒：销毁时UE补发EndOverlap(玩家还在里面时发出离开事件)，回调只读烘焙好的名字/roomBoxLabels，
	// 所以先销毁盒子再清roomBoxLabels。
	for (auto& [box, room] : roomBoxLabels) {
		if (!box) continue;
		RemoveInstanceComponent(box);
		box->DestroyComponent();
	}
	roomBoxLabels.Empty();
	if (collisionBox) {
		RemoveInstanceComponent(collisionBox);
		collisionBox->DestroyComponent();
		collisionBox = nullptr;
	}
	// 墙体/地板/天花板(外壳)是全城合并的ISM，常驻不清；这里只清近处专有的楼梯/坡道网格、电梯轿厢、自定义模型。
	// 楼梯/坡道网格(SpawnMesh产出)不在ISM改造范围内，逐个销毁。
	for (TArray<TObjectPtr<UStaticMeshComponent>>& floorComponents : nearComponentsByFloor) {
		for (UStaticMeshComponent* comp : floorComponents) {
			if (!comp) continue;
			RemoveInstanceComponent(comp);
			comp->DestroyComponent();
		}
		floorComponents.Empty();
	}
	for (FCabin& cabin : cabins) {
		// 先解绑Overlap回调再销毁(销毁时UE会补发EndOverlap，那时cabins正在清空)；玩家还压着
		// 触发盒的话在这里显式收起选项。
		if (cabin.playerOverlaps > 0) HideCabinOptions(cabin);
		auto destroy = [this](UPrimitiveComponent* comp) {
			if (!comp) return;
			comp->OnComponentBeginOverlap.RemoveAll(this);
			comp->OnComponentEndOverlap.RemoveAll(this);
			RemoveInstanceComponent(comp);
			comp->DestroyComponent();
			};
		destroy(cabin.comp);
		destroy(cabin.innerTrigger);
		for (UBoxComponent* trigger : cabin.frontTriggers) destroy(trigger);
	}
	cabins.Empty();
	for (UStaticMeshComponent* comp : nearDecorations) {
		if (!comp) continue;
		RemoveInstanceComponent(comp);
		comp->DestroyComponent();
	}
	nearDecorations.Empty();
	for (auto& pair : nearFurnitureISM) {
		if (UInstancedStaticMeshComponent* ism = pair.Value) {
			RemoveInstanceComponent(ism);
			ism->DestroyComponent();
		}
	}
	nearFurnitureISM.Empty();
	if (nearFurnitureCollisionISM) {
		RemoveInstanceComponent(nearFurnitureCollisionISM);
		nearFurnitureCollisionISM->DestroyComponent();
		nearFurnitureCollisionISM = nullptr;
	}
}

void ABuildingElement::BuildRoomFurniture(int32 floorIndex) {
	if (!building || !framework.IsValid()) return;
	const int32 level = floorIndex - building->GetBasementCount();

	// 先按mesh聚合这一层所有房间的家具，再每种mesh一次AddInstances(批量，只重建一次树)。
	TMap<FString, TArray<FTransform>> transformsByMesh;
	for (Room* room : building->GetRooms()) {
		if (!room || room->GetLayer() != level) continue;
		for (const DecorationInstance& item : room->GetFurniture()) {
			transformsByMesh.FindOrAdd(UTF8_TO_TCHAR(item.meshPath.c_str())).Add(FTransform(
				FRotator(item.pitch, FMath::RadiansToDegrees(item.yaw), item.roll),
				FVector(item.x * BUILDING_WORLD_SCALE, item.y * BUILDING_WORLD_SCALE,
					BUILDING_HEIGHT_EPSILON + item.z * BUILDING_WORLD_SCALE),
				FVector(item.scaleX, item.scaleY, item.scaleZ)));
		}
	}

	TArray<FTransform> collisionBoxes; // 这一层所有家具的包围盒(世界变换，单位立方体缩放)
	for (auto& pair : transformsByMesh) {
		TObjectPtr<UInstancedStaticMeshComponent>* found = nearFurnitureISM.Find(pair.Key);
		UInstancedStaticMeshComponent* ism = found ? found->Get() : nullptr;
		if (!ism) {
			UStaticMesh* mesh = framework->ResolveMesh(TCHAR_TO_UTF8(*pair.Key), nullptr);
			if (!mesh) {
				UE_LOG(LogTemp, Warning, TEXT("ABuildingElement::BuildRoomFurniture: failed to load mesh %s"), *pair.Key);
				continue;
			}
			ism = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
			ism->SetStaticMesh(mesh);
			ism->SetupAttachment(elementRoot);
			ism->SetMobility(EComponentMobility::Static);
			ism->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 渲染用ISM本身不碰撞，碰撞走下面的包围盒ISM(nearFurnitureCollisionISM)
			ism->SetGenerateOverlapEvents(false);
			ism->SetCastShadow(false); // 室内小件，省阴影开销
			ism->RegisterComponent();
			AddInstanceComponent(ism);
			nearFurnitureISM.Add(pair.Key, ism);
		}
		ism->AddInstances(pair.Value, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true);

		// 碰撞：每件家具一个包围盒(取网格自己的包围盒，实例变换同家具)。地毯这类薄片(高度<FURNITURE_COLLISION_MIN_HEIGHT)不挡路。
		const FBoxSphereBounds meshBounds = ism->GetStaticMesh()->GetBounds();
		FVector boxMin = meshBounds.Origin - meshBounds.BoxExtent, boxMax = meshBounds.Origin + meshBounds.BoxExtent;
		TrimFurnitureCollisionBox(pair.Key, boxMin, boxMax);
		const FVector boxCenter = (boxMin + boxMax) * 0.5, boxExtent = (boxMax - boxMin) * 0.5;
		for (const FTransform& instance : pair.Value) {
			const FVector scale = instance.GetScale3D();
			if (boxExtent.Z * 2.0 * scale.Z < FURNITURE_COLLISION_MIN_HEIGHT) continue;
			collisionBoxes.Add(FTransform(
				instance.GetRotation(),
				instance.TransformPosition(boxCenter),
				scale * (boxExtent * 2.0 / 100.0))); // 引擎Cube边长100cm
		}
	}

	if (collisionBoxes.Num() > 0) {
		if (!nearFurnitureCollisionISM) {
			UStaticMesh* cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			if (cube) {
				UInstancedStaticMeshComponent* comp = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
				comp->SetStaticMesh(cube);
				comp->SetupAttachment(elementRoot);
				comp->SetMobility(EComponentMobility::Static);
				comp->SetVisibility(false);
				comp->SetHiddenInGame(true);
				comp->SetCastShadow(false);
				comp->SetGenerateOverlapEvents(false);
				// 只挡人/物理物体/射线(玩家和市民是Pawn通道；本项目Pawn预设不理会Visibility，武器射线用ECC_Pawn)，其余全忽略。
				comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				comp->SetCollisionObjectType(ECC_WorldStatic);
				comp->SetCollisionResponseToAllChannels(ECR_Ignore);
				comp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
				comp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
				// 例外：必须Block ECC_Visibility。Mannequin的脚部IK控制绑定(CR_Mannequin_FootIK)用Visibility球形射线找地面，
				// 盒子若忽略它，玩家站上盒顶时射线穿过盒子打到地板，IK把脚和骨盆往下拉到地板——胶囊在盒顶、小白人却插在家具里。
				comp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
				comp->RegisterComponent();
				AddInstanceComponent(comp);
				nearFurnitureCollisionISM = comp;
			}
		}
		if (nearFurnitureCollisionISM) {
			nearFurnitureCollisionISM->AddInstances(collisionBoxes, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true);
		}
	}
}

void ABuildingElement::BuildDecorations() {
	if (!building || !framework.IsValid()) return;
	for (const DecorationInstance& decoration : building->GetDecorations()) {
		UStaticMesh* mesh = framework->ResolveMesh(decoration.meshPath, nullptr);
		if (!mesh) {
			UE_LOG(LogTemp, Warning, TEXT("ABuildingElement::BuildDecorations: failed to load mesh %s"),
				UTF8_TO_TCHAR(decoration.meshPath.c_str()));
			continue;
		}
		// 世界摆位Core已经按建筑朝向换算好；Z和楼层同一个地面基准(BUILDING_HEIGHT_EPSILON)。
		// 缩放是资产原尺寸的倍数，不像楼梯那样按单位立方体拟合(SpawnMesh的约定不适用)。
		UStaticMeshComponent* comp = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		comp->SetStaticMesh(mesh);
		comp->SetupAttachment(elementRoot);
		comp->SetMobility(EComponentMobility::Static);
		comp->SetGenerateOverlapEvents(false);
		comp->SetWorldLocation(FVector(decoration.x * BUILDING_WORLD_SCALE, decoration.y * BUILDING_WORLD_SCALE,
			BUILDING_HEIGHT_EPSILON + decoration.z * BUILDING_WORLD_SCALE));
		comp->SetWorldRotation(FRotator(decoration.pitch, FMath::RadiansToDegrees(decoration.yaw), decoration.roll));
		comp->SetWorldScale3D(FVector(decoration.scaleX, decoration.scaleY, decoration.scaleZ));
		comp->RegisterComponent();
		AddInstanceComponent(comp);
		nearDecorations.Add(comp);
	}
}

void ABuildingElement::ExecuteLodOp(const FLodOp& op) {
	switch (op.type) {
	case ELodOpType::BuildShell:
		BuildShell();
		transitionPending = false;
		if (framework.IsValid()) framework->NotifyLodTransitionFinished();
		break;
	case ELodOpType::BuildTriggers:
		if (nearFloorCount > 0) BuildCollisionBox();
		break;
	case ELodOpType::DeleteAllNearMeshes:
		ClearNearSections();
		currentLod = ELod::Far;
		transitionPending = false;
		if (framework.IsValid()) framework->NotifyLodTransitionFinished();
		break;
	case ELodOpType::BuildFloorExtras:
		BuildFloorSection(op.floorIndex, /*shell=*/false, /*extras=*/true);
		break;
	case ELodOpType::BuildElevatorCabins:
		BuildElevatorCabinsForBuilding();
		break;
	case ELodOpType::FinishNear:
		currentLod = ELod::Near;
		transitionPending = false;
		if (framework.IsValid()) framework->NotifyLodTransitionFinished();
		break;
	case ELodOpType::BuildDecorations:
		BuildDecorations();
		break;
	case ELodOpType::BuildRoomFurniture:
		BuildRoomFurniture(op.floorIndex);
		break;
	}
}

void ABuildingElement::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
	CORE_GUARD_BEGIN("ABuildingElement::Tick")

	if (!building || !framework.IsValid()) return;

	// 世界还在分阶段生成：玩家只是过渡用的DefaultPawn，先不做LOD切换(见AForeverFrameworkActor::BeginPlay)。
	if (!framework->IsWorldGenerationDone()) return;

	// 电梯行程放在最前面：不依赖玩家Pawn，也不管LOD(远景下电梯照样到站)。
	UpdateCabins(DeltaTime);

	// 外壳(整栋楼的墙/地板/天花板)：世界生成完后每栋建一次，常驻不拆，所以远处LOD不再随距离加载/卸载任何东西。
	// 排进队列期间算"转场中"(世界冻结/IsLodSettled据此等全城外壳建完)，不做近远判定。
	if (!bShellQueued) {
		bShellQueued = true;
		if (nearFloorCount > 0) {
			floorTierOpQueue.Enqueue({ ELodOpType::BuildShell, -1 });
			transitionPending = true;
			framework->NotifyLodTransitionStarted();
		}
	}

	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);

	if (!transitionPending && pawn) {
		FVector pawnMapPos = pawn->GetActorLocation() / BUILDING_WORLD_SCALE;
		float dist = FVector2D::Distance(FVector2D(pawnMapPos.X, pawnMapPos.Y),
			FVector2D(building->GetPosX(), building->GetPosY()));

		// 两级LOD(外壳/近处)，迟滞区间：远->近阈值小(默认16)，近->远阈值大(默认32)，防止玩家在临界距离
		// 附近小范围来回走动时反复建/删楼梯/电梯/模型的组件。距离是水平(XY)平面距离，忽略高度差。
		if (currentLod == ELod::Far && dist <= framework->GetLodNearEnterDistance()) {
			// 先建这栋楼的进出检测盒。
			floorTierOpQueue.Enqueue({ ELodOpType::BuildTriggers, -1 });
			// 楼梯/坡道网格和房间检测盒按层建，只排真有楼梯/坡道/房间的楼层(外壳的墙已经在了)。
			TSet<int32> levelsWithRooms;
			for (Room* room : building->GetRooms()) if (room) levelsWithRooms.Add(room->GetLayer());
			for (int32 i = 0; i < nearFloorCount; i++) {
				const int32 level = i - building->GetBasementCount();
				const Floor* floor = building->GetFloor(level);
				if (!floor) continue;
				if (floor->GetStairs().empty() && floor->GetRamps().empty() && !levelsWithRooms.Contains(level)) continue;
				floorTierOpQueue.Enqueue({ ELodOpType::BuildFloorExtras, i });
			}
			// 自定义模型单独一条操作，不跟楼层/门一起建，但和楼层同一个优先级(楼层级队列)。
			int32 decorationOps = building->GetDecorations().empty() ? 0 : 1;
			if (decorationOps > 0) floorTierOpQueue.Enqueue({ ELodOpType::BuildDecorations, -1 });
			// 房间家具按层建(每层一条，只排有家具的楼层)。
			TSet<int32> levelsWithFurniture;
			for (Room* room : building->GetRooms()) if (room && !room->GetFurniture().empty()) levelsWithFurniture.Add(room->GetLayer());
			for (int32 i = 0; i < nearFloorCount; i++) {
				if (levelsWithFurniture.Contains(i - building->GetBasementCount())) {
					floorTierOpQueue.Enqueue({ ELodOpType::BuildRoomFurniture, i });
				}
			}
			floorTierOpQueue.Enqueue({ ELodOpType::BuildElevatorCabins, -1 });
			floorTierOpQueue.Enqueue({ ELodOpType::FinishNear, -1 });

			transitionPending = true;
			framework->NotifyLodTransitionStarted();
		}
		else if (currentLod == ELod::Near && dist > framework->GetLodFarExitDistance()) {
			floorTierOpQueue.Enqueue({ ELodOpType::DeleteAllNearMeshes, -1 });
			transitionPending = true;
			framework->NotifyLodTransitionStarted();
		}
	}

	// 每个操作都要先问框架组件要一份全地图共享的预算，耗尽就下一帧再继续。必须先判断队列是否为空再消耗预算，
	// 否则"这一帧无事可做"的building也会白吃预算。
	// 外壳操作走自己的预算，其余近处操作走"每帧1条"的LOD预算。
	FLodOp next;
	while (floorTierOpQueue.Peek(next)) {
		const bool granted = next.type == ELodOpType::BuildShell ? framework->TryConsumeShellBudget() : framework->TryConsumeLodOpBudget();
		if (!granted) break;
		FLodOp op;
		floorTierOpQueue.Dequeue(op);
		ExecuteLodOp(op);
	}
	CORE_GUARD_END
}

void ABuildingElement::UpdateCabins(float deltaSeconds) {
	// Core的Cabin记录当前高度/速度/目标楼层，这里每帧用StepCabin积分并写回，到站时把currentLevel
	// 更新成目标楼层。高度在Core里是地图单位，这里换算成UE单位积分再换回去。
	for (Cabin* cabin : building->GetCabinEntities()) {
		if (!cabin || !cabin->IsMoving()) continue;
		cabin->Advance(deltaSeconds);
		float targetZ = building->GetFloorWorldZ(cabin->GetTargetLevel());
		float cruise = framework->GetCabinCruiseSpeed();
		float accel = cruise / FMath::Max(framework->GetCabinEaseSeconds(), 0.01f);
		float worldZ = cabin->GetCurrentZ() * BUILDING_WORLD_SCALE;
		float worldV = cabin->GetVelocity() * BUILDING_WORLD_SCALE;
		bool arrived = cruise <= 0.f || StepCabin(worldZ, worldV, targetZ * BUILDING_WORLD_SCALE, cruise, accel, deltaSeconds);
		if (arrived) cabin->Arrive(targetZ);
		else {
			cabin->SetCurrentZ(worldZ / BUILDING_WORLD_SCALE);
			cabin->SetVelocity(worldV / BUILDING_WORLD_SCALE);
		}
	}

	if (currentLod != ELod::Near) return;
	for (FCabin& cabin : cabins) {
		if (!cabin.cabin || !cabin.comp) continue;
		float z = cabin.cabin->GetCurrentZ() * BUILDING_WORLD_SCALE + BUILDING_HEIGHT_EPSILON;
		if (FMath::IsNearlyEqual(z, cabin.lastSyncedZ, 0.01f)) continue; // 没动就不碰组件，见FCabin::lastSyncedZ
		cabin.lastSyncedZ = z;
		cabin.comp->SetWorldLocation(FVector(cabin.worldX, cabin.worldY, z + cabin.meshZOffset));
		FVector innerLocation(cabin.innerX, cabin.innerY, z + cabin.innerHalfZ);
		if (cabin.innerTrigger) cabin.innerTrigger->SetWorldLocation(innerLocation);
	}

}
