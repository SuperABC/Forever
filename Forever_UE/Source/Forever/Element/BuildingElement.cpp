#include "Element/BuildingElement.h"

#include "Element/DoorComponent.h"
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
#include "map/door.h"
#include "map/cabin.h"
#include "map/room.h"
#include "map/zone.h"
#include "map/general_geometry.h"
#include "player/player.h"
#include "story/event.h"

#include <algorithm>

#define BUILDING_WORLD_SCALE 1000.f
// Building没有真实高度数据，楼层cube统一从这个高度起算(相当于室外地坪grade)，这个grade
// 同时是地上1楼的地板/basements最上面一层(地下1楼)天花板共用的同一个Z——0楼层的
// GetFloorBaseZ(0)恒等于0，这个值只是纯渲染层"避开z-fighting"的偏移，加不加、加多少和
// Core侧楼层数据无关。
//
// 数值这次从10.f改成1.f：老数值10.f比地形网格自己的z-fighting偏移
// (ForeverTerrainFrameworkComponent.cpp的HEIGHT_EPSILON=0.002地图单位，乘
// BUILDING_WORLD_SCALE换算成世界单位=2.f)还大，导致地下室天花板(和地上1楼地板共用同一个
// grade)整体比地形mesh表面高8个世界单位，从外面能看见地下室天花板从地面里冒出来一截
// (用户报告的bug)。改成比地形的2.f更小的1.f，让这个共用grade稳定沉到地形表面以下——
// 地下室天花板朝上的一面因此低于地形，满足"看不出地下室"的要求；地上1楼的地板(同一个Z)
// 跟着一起沉了1个世界单位(约1cm)，用户已确认这个副作用可以接受("地上一层的地板下表面
// 可以比地形mesh低，毕竟玩家在室外的时候一直都在地面上")。1.f仍然大于0，和地形的2.f之间
// 留了1个世界单位的间隙，避免两个网格恰好共面出现z-fighting。
//
// 这不是彻底修复：Building这次仍然不采样所在位置的真实地形高度(TODO，见
// BuildingElement.md)，只是把这个全局常量和地形自己的常量放在同一个参考系里对齐大小关系，
// 对今天"可建造的lot地形高度恒为0"这个前提成立；将来lot允许出现在非0高度地形上时，这个
// 全局常量会需要换成真正按building位置采样SampleHeight()的方案。
#define BUILDING_HEIGHT_EPSILON 1.f
// 墙体厚度(地图单位)，照抄老工程BuildingBase.cpp::ConstructQuad里写死的0.01f。
#define BUILDING_WALL_THICKNESS 0.01f
// UE标准立方体静态网格(/Game/Asset/Meshes/Cube.Cube)的原生边长(cm)——AddCubeInstance/
// SpawnMesh用这个值把它缩放到目标尺寸，缩放系数=目标尺寸/这个原生边长。
#define BUILDING_CUBE_MESH_SIZE 100.f
// building碰撞盒在X/Y/Z三个方向各放大的量(地图单位)，和Room碰撞盒的-0.01配对，保证两者
// 贴合的边界不会因为完全重合而在Overlap判定上抖动。
#define BUILDING_COLLISION_MARGIN 0.01f
// Room碰撞盒在X/Y/Z三个方向各缩小的量(地图单位)，和上面BUILDING_COLLISION_MARGIN的+0.01配对
// (原来在ForeverRoomFrameworkComponent.cpp里叫ROOM_COLLISION_MARGIN，同一个值挪过来)。
#define ROOM_COLLISION_MARGIN 0.01f
// 远处LOD灰box固定用section 0——每栋building现在都有自己专属的farMesh，不再像之前共用一个
// PMC那样需要按building分配不同的section索引。
#define BUILDING_FAR_SECTION_INDEX 0

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

	// 楼体子矩形的世界中心(地图单位换算成世界单位)——building自身的GetPosX/PosY()是Building
	// 整个Quad的中心(已经是世界坐标)，楼体中心相对这个点有一个未旋转局部坐标系下的偏移
	// (GetBodyOffsetX/Y())，这个偏移要跟着building自己的rotation一起转，才能得到楼体真正的
	// 世界中心。只给远处LOD(整栋一个box)用；楼层内部任意一点的转换见下面更通用的
	// ComputeWorldPosition。
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
		float grade = BUILDING_HEIGHT_EPSILON;
		outZBottom = grade - totalBasementDepth * BUILDING_WORLD_SCALE;
		outZTop = grade + totalAboveHeight * BUILDING_WORLD_SCALE;
	}

	// 一个开口(门/窗)在这一侧墙上的位置——x1/x2沿墙方向(局部坐标，相对这面墙起点horizBase的
	// 偏移量，不是绝对局部坐标)，y1/y2是离楼板的高度(世界单位)。门/窗对墙体开洞分段来说完全
	// 一样(都只是缺口)，这次不再摆窗户网格，不需要区分门/窗类型。
	struct FBuildingWallOpening {
		float x1 = 0.f, x2 = 0.f, y1 = 0.f, y2 = 0.f;
	};

	// Stair/Ramp网格的朝向由三部分组成：building自身的世界旋转(rotation，跟着lot走，和墙体/
	// 地板slab一致)、加上这个楼梯/坡道自己的局部direction(0-3，模板解析时已经按AssignFloor
	// 声明的face预旋转过，见BuildingLayoutLibrary)——照抄老工程ABuildingBase::ConstructBuilding
	// 里"GetRotation(stair.GetDirection())"这段：NORTH=0°、EAST=90°、SOUTH=180°、WEST=270°，
	// 在老工程里这是相对building自己Actor(已经摆好世界旋转)的局部附加旋转，这次没有per-building
	// 的Actor，要显式把这段角度和building世界旋转相加才能等价；同时WEST/EAST方向要交换
	// sizeX/sizeY——网格局部坐标系X轴默认对齐NORTH朝向下的世界X，旋转90°/270°之后局部X/Y
	// 和世界X/Y互换，缩放必须按局部轴给，不然90°/270°朝向的楼梯会被拉伸成错误的长宽比。
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

	// 电梯轿厢单步推进：状态是(z, v)，v带符号(向上为正)，目标可以随时改。每个子步先判断：
	// 1) 速度背离目标(含刚反向改目标)：以加速度a减速，过零后自然反向加速；
	// 2) 速度朝向目标且剩余距离不够以a刹停：按v²/(2*剩余)精确减速刚好停在目标(需要的减速度超过1.5a说明
	//    已经来不及，就按a刹车冲过目标再折返)；
	// 3) 否则以a加速到不超过cruiseSpeed(同向改目标时速度不变，继续巡航)。
	// 速度只在这里累积，所以中途改目标速度始终连续。返回是否已到站(此时z=targetZ、v=0)。
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
	nearDoorsByFloor.SetNum(nearFloorCount);

	farMesh = NewObject<UProceduralMeshComponent>(this, TEXT("FarMesh"));
	farMesh->SetupAttachment(elementRoot);
	farMesh->SetMobility(EComponentMobility::Static); // 整个组件自己不会移动(section增删不算"移动")
	farMesh->RegisterComponent();

	// 0层建筑(只有自定义模型/植被，没有楼体)：远处方块和进出触发盒高度都是0，没有意义，不建——
	// 远处LOD什么都不显示，自定义模型只在近处出现，见BuildingElement.md"自定义模型"一节。
	if (nearFloorCount == 0) return;

	BuildFarSection();
	// building自己的进入/离开碰撞盒 + 这栋楼所有Room各自的碰撞盒——都常驻到这个Element被
	// 销毁，不随近/远LOD切换增删。之前排查"碰撞盒常驻数量拖慢注册"时临时禁用过，结论已经
	// 明确是"全地图共用一个owner Actor"的问题，现在两者都attach到这栋楼专属的Actor上，
	// 不会再互相拖累。
	BuildCollisionBox();
	BuildRoomCollisionBoxes();
}

TMap<const Building*, TWeakObjectPtr<ABuildingElement>> ABuildingElement::elementsByBuilding;

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

void ABuildingElement::BuildFarSection() {
	// buildShell只管近处的墙壁/地板/天花板，远处灰色方块照常画(成品模型只在近处显示，远处靠它)。
	if (!farMesh || !building || !framework.IsValid() || nearFloorCount == 0) return;

	float cx, cy;
	ComputeBodyWorldCenter(*building, cx, cy);
	float hx = building->GetBodySizeX() * 0.5f * BUILDING_WORLD_SCALE;
	float hy = building->GetBodySizeY() * 0.5f * BUILDING_WORLD_SCALE;
	float zBottom, zTop;
	ComputeFullZRange(*building, zBottom, zTop);

	TArray<FVector> vertices;
	TArray<int32> triangles;
	BuildingAppendFlatBoxExplicit(vertices, triangles, cx, cy, hx, hy, zBottom, zTop, building->GetRotation());

	farMesh->CreateMeshSection(BUILDING_FAR_SECTION_INDEX, vertices, triangles,
		TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);
	UMaterialInstanceDynamic* material = framework->ResolveLodMaterial(building);
	if (material) farMesh->SetMaterial(BUILDING_FAR_SECTION_INDEX, material);
}

void ABuildingElement::ClearFarSection() {
	if (!farMesh) return;
	farMesh->ClearMeshSection(BUILDING_FAR_SECTION_INDEX);
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

	// 名字快照在这里烘焙好，Overlap回调绝不解引用building——和EndPlay处理同一套安全原则。
	buildingNameOnly = building->GetName();
	zoneNameOnly = building->GetParentZone() ? building->GetParentZone()->GetName() : std::string();
	collisionBox = box;
}

namespace {
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

void ABuildingElement::BuildRoomCollisionBoxes() {
	if (!building) return;

	// 原来在UForeverRoomFrameworkComponent里，全地图所有Room共用那个组件所在的单例owner——
	// 现在挪进这里，每栋楼自己的Room碰撞盒跟着这栋楼的其它组件一起attach到elementRoot，
	// 用的还是原来那套换算(Room自己的局部矩形按Building::LocalToWorld换算世界中心+
	// building自身旋转，垂直范围是这个Room所在楼层的Z范围)，三个方向各-0.01(地图单位)，
	// 和building碰撞盒的各+0.01配对，两者贴合的边界不会因为完全重合而在Overlap判定上抖动。
	const vector<float>& floorHeights = building->GetFloorHeights();
	int32 basements = building->GetBasementCount();
	float margin = ROOM_COLLISION_MARGIN * BUILDING_WORLD_SCALE;

	for (Room* room : building->GetRooms()) {
		if (!room) continue;

		int32 level = room->GetLayer();
		int32 idx = basements + level;
		if (idx < 0 || idx >= static_cast<int32>(floorHeights.size())) continue;
		float floorHeight = floorHeights[idx];

		float hx = room->GetSizeX() * 0.5f * BUILDING_WORLD_SCALE - margin;
		float hy = room->GetSizeY() * 0.5f * BUILDING_WORLD_SCALE - margin;
		float floorBaseZ = building->GetFloorBaseZ(level);
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

UInstancedStaticMeshComponent* ABuildingElement::GetOrCreateCubeInstancePool(UMaterialInterface* material) {
	if (TObjectPtr<UInstancedStaticMeshComponent>* found = cubeInstancePools.Find(material)) {
		return *found;
	}
	if (!framework.IsValid()) return nullptr;
	UStaticMesh* cubeMesh = framework->GetCubeMesh();
	if (!cubeMesh) return nullptr;

	UInstancedStaticMeshComponent* ism = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
	ism->SetStaticMesh(cubeMesh);
	ism->SetupAttachment(elementRoot);
	// 常驻组件，不随LOD切换销毁/重建——只在Init()之后首次用到某个材质时创建一次，见
	// AddCubeInstance/类头文件cubeInstancePools注释。
	ism->SetMobility(EComponentMobility::Static);
	// 显式设置成BlockAll——不能依赖"ISM默认碰撞和原来UStaticMeshComponent默认一致"这个
	// 未经验证的假设，墙体/地板/天花板只需要纯阻挡碰撞，和原SpawnCube的效果一致。
	ism->SetCollisionProfileName(TEXT("BlockAll"));
	ism->SetGenerateOverlapEvents(false);
	if (material) ism->SetMaterial(0, material);
	ism->RegisterComponent();
	AddInstanceComponent(ism);
	cubeInstancePools.Add(material, ism);
	return ism;
}

void ABuildingElement::AddCubeInstance(float centerX, float centerY, float centerZ,
	float sizeX, float sizeY, float sizeZ, float rotation, UMaterialInterface* material) {
	// 不在这里直接调ism->AddInstance()——按材质暂存，攒够一层楼的全部实例之后由
	// FlushCubeInstances()一次性批量提交，见pendingCubeTransforms声明处注释(逐个单实例提交
	// 实测每次约0.1~0.4ms，300+段的楼层能堆到单帧100ms+的卡顿)。
	FTransform worldTransform(
		FRotator(0.f, FMath::RadiansToDegrees(rotation), 0.f),
		FVector(centerX, centerY, centerZ),
		FVector(sizeX / BUILDING_CUBE_MESH_SIZE, sizeY / BUILDING_CUBE_MESH_SIZE,
			sizeZ / BUILDING_CUBE_MESH_SIZE));
	pendingCubeTransforms.FindOrAdd(material).Add(worldTransform);
}

void ABuildingElement::FlushCubeInstances() {
	for (auto& [material, transforms] : pendingCubeTransforms) {
		if (transforms.IsEmpty()) continue;
		UInstancedStaticMeshComponent* ism = GetOrCreateCubeInstancePool(material);
		if (!ism) continue;
		ism->AddInstances(transforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true);
	}
	pendingCubeTransforms.Empty();
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
	UMaterialInterface* wallMaterial) {
	// floorBaseZ是Building::GetFloorBaseZ(level)的值：地图单位、相对地坪(grade)的楼层底部
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
		auto spawnWallBox = [&](float segStart, float segEnd, float zBot, float zTop) {
			if (segEnd <= segStart || zTop <= zBot) return;
			float segCenter = (segStart + segEnd) * 0.5f;
			float segLen = segEnd - segStart;
			float localX = face.alongY ? face.acrossFixed : segCenter;
			float localY = face.alongY ? segCenter : face.acrossFixed;
			float wx, wy;
			ComputeWorldPosition(*building, localX, localY, wx, wy);
			float sizeXWorld = (face.alongY ? BUILDING_WALL_THICKNESS : segLen) * BUILDING_WORLD_SCALE;
			float sizeYWorld = (face.alongY ? segLen : BUILDING_WALL_THICKNESS) * BUILDING_WORLD_SCALE;
			float wz = (zBot + zTop) * 0.5f;
			float sizeZWorld = zTop - zBot;
			AddCubeInstance(wx, wy, wz, sizeXWorld, sizeYWorld, sizeZWorld, rotation, wallMaterial);
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

void ABuildingElement::BuildFloorSection(int32 floorIndex) {
	if (!building || !framework.IsValid()) return;
	if (floorIndex < 0 || floorIndex >= nearFloorCount) return;

	int32 level = floorIndex - building->GetBasementCount();
	const Floor* floor = building->GetFloor(level);
	if (!floor) return;

	// floorBaseZ(地图单位，相对地坪grade、不含BUILDING_HEIGHT_EPSILON这个纯渲染层偏移)直接
	// 用Core侧现成的GetFloorBaseZ()，不用像ComputeFullZRange那样另外反推——两者算的是同一个
	// 值(grade-相对的楼层底部高度)，见Building::GetFloorBaseZ()注释。
	float floorBaseZ = building->GetFloorBaseZ(level);
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

	// 楼梯/坡道：井道墙体(和corridor/single/row同一套BuildWallsForElement)+按该层资产指定的
	// 3D网格摆一个实体；电梯只有井道墙体，不摆网格(轿厢单独由BuildElevatorCabinsForBuilding
	// 处理)。
	for (const Stair& stair : floor->GetStairs()) {
		if (buildShell) BuildWallsForElement(floorBaseZ, floorHeight, stair.GetPosX(), stair.GetPosY(),
			stair.GetSizeX(), stair.GetSizeY(),
			stair.GetWall(FACE_WEST), stair.GetWall(FACE_EAST), stair.GetWall(FACE_NORTH), stair.GetWall(FACE_SOUTH),
			{}, {}, wallMaterial);
		float wx, wy;
		ComputeWorldPosition(*building, stair.GetPosX(), stair.GetPosY(), wx, wy);
		float wz = floorBaseZ * BUILDING_WORLD_SCALE + BUILDING_HEIGHT_EPSILON;
		// 楼梯网格按.layout模板里Stair实际声明的footprint尺寸缩放(X/Y)，Z缩放到整层高度
		// (楼梯本来就是连接上下相邻楼层的)——地图单位换算成世界单位后再传给SpawnMesh；朝向
		// 除了building自身的世界旋转，还要叠加这个楼梯自己的局部direction。
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
		if (buildShell) BuildWallsForElement(floorBaseZ, floorHeight, ramp.GetPosX(), ramp.GetPosY(),
			ramp.GetSizeX(), ramp.GetSizeY(),
			ramp.GetWall(FACE_WEST), ramp.GetWall(FACE_EAST), ramp.GetWall(FACE_NORTH), ramp.GetWall(FACE_SOUTH),
			{}, {}, wallMaterial);
		float wx, wy;
		ComputeWorldPosition(*building, ramp.GetPosX(), ramp.GetPosY(), wx, wy);
		float wz = floorBaseZ * BUILDING_WORLD_SCALE + BUILDING_HEIGHT_EPSILON;
		// 同上：坡道网格按Ramp实际声明的footprint尺寸(X/Y)+整层高度(Z)缩放，朝向叠加自己的
		// 局部direction。
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
		if (buildShell) BuildWallsForElement(floorBaseZ, floorHeight, elevator.GetPosX(), elevator.GetPosY(),
			elevator.GetSizeX(), elevator.GetSizeY(),
			elevator.GetWall(FACE_WEST), elevator.GetWall(FACE_EAST), elevator.GetWall(FACE_NORTH), elevator.GetWall(FACE_SOUTH),
			{}, {}, wallMaterial);
	}
	for (const Corridor& corridor : floor->GetCorridors()) {
		if (buildShell) BuildWallsForElement(floorBaseZ, floorHeight, corridor.GetPosX(), corridor.GetPosY(),
			corridor.GetSizeX(), corridor.GetSizeY(),
			corridor.GetWall(FACE_WEST), corridor.GetWall(FACE_EAST), corridor.GetWall(FACE_NORTH), corridor.GetWall(FACE_SOUTH),
			corridor.GetDoors(), corridor.GetWindows(), wallMaterial);
	}
	// Single/Row槽位隐含四面都有墙——但槽位本身在实例化成真正的Room之后，门/窗/朝向都已经
	// 转移到Room身上(见Building::AssignRoom/ArrangeRow)，槽位自己的门窗数据不再是最新的，
	// 这里改成遍历building->GetRooms()按GetLayer()==level筛选，画每个真正Room的墙体，
	// 不直接用Floor::GetSingles()/GetRows()（那两个列表只是"模板槽位"，槽位数量/大小和实际
	// 生成的Room数量不是一一对应——ArrangeRow会把一个row槽位切成好几个Room）。
	for (Room* room : building->GetRooms()) {
		if (!room || room->GetLayer() != level) continue;
		if (buildShell) BuildWallsForElement(floorBaseZ, floorHeight, room->GetPosX(), room->GetPosY(),
			room->GetSizeX(), room->GetSizeY(),
			true, true, true, true, // Single/Row隐含四面都有墙
			room->GetDoors(), room->GetWindows(), wallMaterial);
	}

	// 门不在这里建——拆到GatherPendingDoors/BuildDoorBatch里单独按批次处理，见
	// BuildingElement.md"门加载优先级"一节。门洞本身仍然由上面的BuildWallsForElement
	// 照常挖出，和是否建门视觉组件无关。

	// 地板/天花板：老工程两层独立薄slab的做法——地板贴地坪往上一点点，天花板贴楼层顶往下
	// 一点点，中间留一条很窄的缝，避免和上/下相邻楼层的对应slab共面z-fighting。
	constexpr float kSlabThickness = 0.02f; // 地图单位，照抄老工程Ceiling/Ground的0.02f厚度
	float floorBottomWorld = BUILDING_HEIGHT_EPSILON + floorBaseZ * BUILDING_WORLD_SCALE;
	float floorTopWorld = BUILDING_HEIGHT_EPSILON + (floorBaseZ + floorHeight) * BUILDING_WORLD_SCALE;
	if (buildShell) for (const Ground& ground : floor->GetGrounds()) {
		float wx, wy;
		ComputeWorldPosition(*building, ground.GetPosX(), ground.GetPosY(), wx, wy);
		float wz = floorBottomWorld + kSlabThickness * 0.5f * BUILDING_WORLD_SCALE;
		float sizeX = ground.GetSizeX() * BUILDING_WORLD_SCALE, sizeY = ground.GetSizeY() * BUILDING_WORLD_SCALE;
		float sizeZ = kSlabThickness * BUILDING_WORLD_SCALE;
		AddCubeInstance(wx, wy, wz, sizeX, sizeY, sizeZ, rotation, floorMaterial);
	}
	if (buildShell) for (const Ceiling& ceiling : floor->GetCeilings()) {
		float wx, wy;
		ComputeWorldPosition(*building, ceiling.GetPosX(), ceiling.GetPosY(), wx, wy);
		float wz = floorTopWorld - kSlabThickness * 0.5f * BUILDING_WORLD_SCALE;
		float sizeX = ceiling.GetSizeX() * BUILDING_WORLD_SCALE, sizeY = ceiling.GetSizeY() * BUILDING_WORLD_SCALE;
		float sizeZ = kSlabThickness * BUILDING_WORLD_SCALE;
		AddCubeInstance(wx, wy, wz, sizeX, sizeY, sizeZ, rotation, ceilingMaterial);
	}

	FlushCubeInstances();
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
		UStaticMeshComponent* comp = SpawnMesh(wx, wy, cabinZ,
			meshSizeX * BUILDING_WORLD_SCALE, meshSizeY * BUILDING_WORLD_SCALE, minFloorHeight * BUILDING_WORLD_SCALE,
			meshRotation, cabinMesh, nullptr, /*isMovable=*/true);
		if (!comp) continue;

		FCabin cabin;
		cabin.cabin = coreCabin;
		cabin.comp = comp;
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
			float fz = (building->GetFloorBaseZ(level) + levelHeight * 0.5f) * BUILDING_WORLD_SCALE + BUILDING_HEIGHT_EPSILON;
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

void ABuildingElement::GatherPendingDoors() {
	if (!building) return;
	// Door*(Core对象)在建筑生成时就创建好，和LOD状态无关，随时可读——不需要等楼层先建完
	// 再去发现门，Far->Near转场排队的这一刻就能把全部待建门一次性收集好。
	for (Door* door : building->GetDoorEntities()) {
		if (door) pendingDoors.Add(door);
	}
	for (Room* room : building->GetRooms()) {
		if (!room) continue;
		for (Door* door : room->GetDoorEntities()) {
			if (door) pendingDoors.Add(door);
		}
	}
}

void ABuildingElement::BuildDoorBatch() {
	if (pendingDoors.IsEmpty() || !building || !framework.IsValid()) return;

	int32 budget = framework->GetMaxDoorsPerOp();
	while (budget-- > 0 && !pendingDoors.IsEmpty()) {
		Door* door = pendingDoors.Pop();
		if (!door) continue;

		int32 floorIndex = door->GetLevel() + building->GetBasementCount();
		if (!nearDoorsByFloor.IsValidIndex(floorIndex)) continue;

		UForeverDoorComponent* comp = NewObject<UForeverDoorComponent>(this, NAME_None, RF_Transient);
		comp->SetupAttachment(elementRoot);
		comp->RegisterComponent();
		comp->Init(door);
		AddInstanceComponent(comp);
		nearDoorsByFloor[floorIndex].Add(comp);
	}
}

void ABuildingElement::ClearNearSections() {
	// 清空尚未建出视觉组件的待建门——避免这个Actor被反复Far<->Near循环使用时，上一轮
	// 没排完的门残留污染下一轮GatherPendingDoors的收集结果。
	pendingDoors.Empty();

	// 墙体/地板/天花板cube：ISM组件本身常驻，只清空instance数组，不Register/Destroy——这是
	// 这次ISM改造消除注册开销的关键，见cubeInstancePools/AddCubeInstance声明处注释。
	for (auto& [material, ism] : cubeInstancePools) {
		if (ism) ism->ClearInstances();
	}
	// 楼梯/坡道网格(SpawnMesh产出)不在ISM改造范围内，仍然逐个销毁。
	for (TArray<TObjectPtr<UStaticMeshComponent>>& floorComponents : nearComponentsByFloor) {
		for (UStaticMeshComponent* comp : floorComponents) {
			if (!comp) continue;
			RemoveInstanceComponent(comp);
			comp->DestroyComponent();
		}
		floorComponents.Empty();
	}
	for (TArray<TObjectPtr<UForeverDoorComponent>>& floorDoors : nearDoorsByFloor) {
		for (UForeverDoorComponent* doorComp : floorDoors) {
			if (!doorComp) continue;
			doorComp->ClearDoor();
			RemoveInstanceComponent(doorComp);
			doorComp->DestroyComponent();
		}
		floorDoors.Empty();
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
	case ELodOpType::BuildFarMesh:
		BuildFarSection();
		break;
	case ELodOpType::DeleteAllNearMeshes:
		ClearNearSections();
		currentLod = ELod::Far;
		transitionPending = false;
		if (framework.IsValid()) framework->NotifyLodTransitionFinished();
		break;
	case ELodOpType::BuildFloorMesh:
		BuildFloorSection(op.floorIndex);
		break;
	case ELodOpType::BuildElevatorCabins:
		BuildElevatorCabinsForBuilding();
		break;
	case ELodOpType::DeleteFarMesh:
		ClearFarSection();
		currentLod = ELod::Near;
		transitionPending = false;
		if (framework.IsValid()) framework->NotifyLodTransitionFinished();
		break;
	case ELodOpType::BuildDoorBatch:
		BuildDoorBatch();
		break;
	case ELodOpType::BuildDecorations:
		BuildDecorations();
		break;
	}
}

void ABuildingElement::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	if (!building || !framework.IsValid()) return;

	// 电梯行程放在最前面：不依赖玩家Pawn，也不管LOD(远景下电梯照样到站)。
	UpdateCabins(DeltaTime);

	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn) return;

	if (!transitionPending) {
		FVector pawnMapPos = pawn->GetActorLocation() / BUILDING_WORLD_SCALE;
		float dist = FVector2D::Distance(FVector2D(pawnMapPos.X, pawnMapPos.Y),
			FVector2D(building->GetPosX(), building->GetPosY()));

		// 迟滞区间：远->近用比较小的阈值(默认20)，近->远用比较大的阈值(默认40)，两者不共用
		// 一个值——已经精细化的building要离得更远才会退化成单box，防止玩家在临界距离附近
		// 小范围来回走动时反复Near/Far抖动式切换(每次切换都要建/删一整层楼的组件，抖动等于
		// 反复触发卡顿)。
		if (currentLod == ELod::Far && dist <= framework->GetLodNearEnterDistance()) {
			for (int32 i = 0; i < nearFloorCount; i++) {
				floorTierOpQueue.Enqueue({ ELodOpType::BuildFloorMesh, i });
			}
			// 自定义模型单独一条操作，不跟楼层/门一起建，但和楼层同一个优先级(楼层级队列)。
			int32 decorationOps = building->GetDecorations().empty() ? 0 : 1;
			if (decorationOps > 0) floorTierOpQueue.Enqueue({ ELodOpType::BuildDecorations, -1 });
			floorTierOpQueue.Enqueue({ ELodOpType::BuildElevatorCabins, -1 });
			floorTierOpQueue.Enqueue({ ELodOpType::DeleteFarMesh, -1 });
			framework->NotifyFloorTierOpEnqueued(nearFloorCount + decorationOps + 2);

			// 门和楼层分开排队：Door*生成时就和LOD状态无关，不需要等楼层先建完再发现门，
			// 这里一次性收集好、按批次排进低优先级的doorTierOpQueue，见
			// GatherPendingDoors/BuildDoorBatch声明处注释。
			GatherPendingDoors();
			int32 doorBatches = FMath::DivideAndRoundUp(pendingDoors.Num(), FMath::Max(1, framework->GetMaxDoorsPerOp()));
			for (int32 i = 0; i < doorBatches; i++) {
				doorTierOpQueue.Enqueue({ ELodOpType::BuildDoorBatch, -1 });
			}

			transitionPending = true;
			framework->NotifyLodTransitionStarted();
		}
		else if (currentLod == ELod::Near && dist > framework->GetLodFarExitDistance()) {
			floorTierOpQueue.Enqueue({ ELodOpType::BuildFarMesh, -1 });
			floorTierOpQueue.Enqueue({ ELodOpType::DeleteAllNearMeshes, -1 });
			framework->NotifyFloorTierOpEnqueued(2);
			transitionPending = true;
			framework->NotifyLodTransitionStarted();
		}
	}

	// 每个操作都要先问框架组件要一份全地图共享的预算，预算耗尽这一帧就先停手，下一帧再继续
	// ——避免大量building同时穿越距离阈值时，所有Element在同一帧一起疯狂建组件卡成一张ppt。
	// 必须先判断队列是否为空再消耗预算(不能反过来)，否则每一栋"这一帧其实无事可做"的building
	// 也会白白吃掉一份全局预算，挤占真正需要建楼层的building的份额。
	//
	// 楼层级操作永远先尝试：只要全地图任意一栋楼还有楼层级操作排队，framework->
	// TryConsumeLodOpBudget(true)对所有building的门操作都会拒绝发预算，门操作停滞直到
	// 楼层级工作全部清空，见ForeverBuildingFrameworkComponent::TryConsumeLodOpBudget。
	while (!floorTierOpQueue.IsEmpty() && framework->TryConsumeLodOpBudget(/*isDoorTierOp=*/false)) {
		FLodOp op;
		floorTierOpQueue.Dequeue(op);
		ExecuteLodOp(op);
		framework->NotifyFloorTierOpExecuted();
	}
	while (!doorTierOpQueue.IsEmpty() && framework->TryConsumeLodOpBudget(/*isDoorTierOp=*/true)) {
		FLodOp op;
		doorTierOpQueue.Dequeue(op);
		ExecuteLodOp(op);
	}
}

void ABuildingElement::UpdateCabins(float deltaSeconds) {
	// Core的Cabin记录当前高度/速度/目标楼层，这里每帧用StepCabin积分并写回，到站时把currentLevel
	// 更新成目标楼层。高度在Core里是地图单位，这里换算成UE单位积分再换回去。
	for (Cabin* cabin : building->GetCabinEntities()) {
		if (!cabin || !cabin->IsMoving()) continue;
		cabin->Advance(deltaSeconds);
		float targetZ = building->GetFloorBaseZ(cabin->GetTargetLevel());
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
		cabin.comp->SetWorldLocation(FVector(cabin.worldX, cabin.worldY, z));
		FVector innerLocation(cabin.innerX, cabin.innerY, z + cabin.innerHalfZ);
		if (cabin.innerTrigger) cabin.innerTrigger->SetWorldLocation(innerLocation);
	}

}
