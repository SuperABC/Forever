#include "Element/BuildingElement.h"

#include "Framework/ForeverBuildingFrameworkComponent.h"

#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "map/geometry.h"

#include <algorithm>

#define BUILDING_WORLD_SCALE 1000.f
// 和ForeverZoneFrameworkComponent.cpp的ZONE_HEIGHT_EPSILON同一类问题：Building没有真实
// 高度数据，楼层cube统一从这个高度起算(相当于室外地坪)，避开和地形网格共面z-fighting。
#define BUILDING_HEIGHT_EPSILON 10.f
// 墙体厚度(地图单位)，照抄老工程BuildingBase.cpp::ConstructQuad里写死的0.01f。
#define BUILDING_WALL_THICKNESS 0.01f
// UE标准立方体静态网格(/Game/Asset/Meshes/Cube.Cube)的原生边长(cm)——SpawnCube用
// SetWorldScale3D把它缩放到目标尺寸，缩放系数=目标尺寸/这个原生边长。
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

	// 电梯轿厢"匀速+两端缓入缓出"往返运动。用平滑Hermite曲线smoothstep(u)=3u²-2u³做速度曲线：
	// 一段缓入时间te内速度从0平滑升到cruiseSpeed，这段位移是smoothstep速度对时间的积分，精确
	// 解析式=cruiseSpeed*te*(u³-u⁴/2)(u=s/te)，u=1时正好是cruiseSpeed*te*0.5(smoothstep在
	// [0,1]上的积分均值精确等于0.5)。缓出段由缓入段的公式按"距终点还有多久"对称复用(减速曲线
	// 是加速曲线的时间倒放)。te按min(te, distance/speed)夹到不超过半程对应时间，矮建筑会退化
	// 成"没有匀速段、纯缓入接缓出"，不会算出负的匀速时间。
	float ComputeCabinZ(float zBottom, float zTop, float cruiseSpeed, float easeSecondsRaw, float elapsedSeconds) {
		float distance = zTop - zBottom;
		if (distance <= 0.f || cruiseSpeed <= 0.f) return zBottom;

		float te = FMath::Min(easeSecondsRaw, distance / cruiseSpeed);
		auto easeInDistance = [&](float s) {
			float u = (te > 0.f) ? FMath::Clamp(s / te, 0.f, 1.f) : 1.f;
			return cruiseSpeed * te * (u * u * u - u * u * u * u * 0.5f);
			};

		float easeDistance = easeInDistance(te);
		float cruiseDistance = FMath::Max(0.f, distance - easeDistance * 2.f);
		float cruiseTime = cruiseDistance / cruiseSpeed;
		float oneWayTime = te * 2.f + cruiseTime;
		float period = oneWayTime * 2.f;
		if (period <= 0.f) return zBottom;

		float t = FMath::Fmod(elapsedSeconds, period);
		if (t < 0.f) t += period;

		bool goingUp = t < oneWayTime;
		float legT = goingUp ? t : (t - oneWayTime);

		float travelled;
		if (legT < te) {
			travelled = easeInDistance(legT);
		} else if (legT < te + cruiseTime) {
			travelled = easeDistance + cruiseSpeed * (legT - te);
		} else {
			float remaining = oneWayTime - legT; // 距这一程结束还有多久，落在[0,te]
			travelled = distance - easeInDistance(remaining);
		}
		travelled = FMath::Clamp(travelled, 0.f, distance);

		return goingUp ? (zBottom + travelled) : (zTop - travelled);
	}
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

	nearFloorCount = building->GetBasementCount() + building->GetLayerCount();
	nearComponentsByFloor.SetNum(nearFloorCount);

	farMesh = NewObject<UProceduralMeshComponent>(this, TEXT("FarMesh"));
	farMesh->SetupAttachment(elementRoot);
	farMesh->SetMobility(EComponentMobility::Static); // 整个组件自己不会移动(section增删不算"移动")
	farMesh->RegisterComponent();

	BuildFarSection();
	// building自己的进入/离开碰撞盒 + 这栋楼所有Room各自的碰撞盒——都常驻到这个Element被
	// 销毁，不随近/远LOD切换增删。之前排查"碰撞盒常驻数量拖慢注册"时临时禁用过，结论已经
	// 明确是"全地图共用一个owner Actor"的问题，现在两者都attach到这栋楼专属的Actor上，
	// 不会再互相拖累。
	BuildCollisionBox();
	BuildRoomCollisionBoxes();
}

void ABuildingElement::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	building = nullptr;
	Super::EndPlay(EndPlayReason);
}

void ABuildingElement::BuildFarSection() {
	if (!farMesh || !building || !framework.IsValid()) return;

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

	// 只烘焙一份显示用的地址字符串，Overlap回调绝不解引用building——和EndPlay处理同一套
	// 安全原则。
	collisionLabel = FString::Printf(TEXT("Building: %s"), UTF8_TO_TCHAR(building->GetAddress().c_str()));
	collisionBox = box;
}

void ABuildingElement::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn || !GEngine) return;
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("进入 %s"), *collisionLabel));
}

void ABuildingElement::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn || !GEngine) return;
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("离开 %s"), *collisionLabel));
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

		roomBoxLabels.Add(box, FString::Printf(TEXT("Room: %s"), UTF8_TO_TCHAR(room->GetAddress().c_str())));
	}
}

void ABuildingElement::OnRoomOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn) return;
	FString* label = roomBoxLabels.Find(OverlappedComponent);
	if (!label || !GEngine) return;
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("进入 %s"), **label));
}

void ABuildingElement::OnRoomOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn) return;
	FString* label = roomBoxLabels.Find(OverlappedComponent);
	if (!label || !GEngine) return;
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("离开 %s"), **label));
}

UStaticMeshComponent* ABuildingElement::SpawnCube(float centerX, float centerY, float centerZ,
	float sizeX, float sizeY, float sizeZ, float rotation, UMaterialInterface* material) {
	if (!framework.IsValid()) return nullptr;
	UStaticMesh* cubeMesh = framework->GetCubeMesh();
	if (!cubeMesh) return nullptr;

	UStaticMeshComponent* comp = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	comp->SetStaticMesh(cubeMesh);
	comp->SetupAttachment(elementRoot);
	// 墙体/地板/天花板slab摆好之后永远不会再移动——必须在RegisterComponent()之前把mobility
	// 设成Static、且世界坐标/旋转/缩放也要在注册前设好(Static组件注册之后就不允许再
	// SetWorldLocation之类的"移动"了)。
	comp->SetMobility(EComponentMobility::Static);
	// 墙体/地板/天花板slab只需要"挡住玩家"这个纯阻挡碰撞，不需要任何Overlap通知。
	comp->SetGenerateOverlapEvents(false);
	comp->SetWorldLocation(FVector(centerX, centerY, centerZ));
	comp->SetWorldRotation(FRotator(0.f, FMath::RadiansToDegrees(rotation), 0.f));
	comp->SetWorldScale3D(FVector(sizeX / BUILDING_CUBE_MESH_SIZE, sizeY / BUILDING_CUBE_MESH_SIZE,
		sizeZ / BUILDING_CUBE_MESH_SIZE));
	if (material) comp->SetMaterial(0, material);
	comp->RegisterComponent();
	AddInstanceComponent(comp);
	return comp;
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
	UMaterialInterface* wallMaterial, int32 floorIndex) {
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

	auto ensureSlot = [&]() -> TArray<TObjectPtr<UStaticMeshComponent>>& {
		return nearComponentsByFloor[floorIndex];
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
			UStaticMeshComponent* comp = SpawnCube(wx, wy, wz, sizeXWorld, sizeYWorld, sizeZWorld, rotation, wallMaterial);
			if (comp) ensureSlot().Add(comp);
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
	if (BuildingMod* mod = building->GetMod()) {
		auto specIt = mod->floors.find(level);
		if (specIt != mod->floors.end()) {
			const FloorAssetSpec& assets = specIt->second.assets;
			wallMaterial = framework->ResolveMaterial(assets.wallMaterial, wallMaterial);
			floorMaterial = framework->ResolveMaterial(assets.floorMaterial, floorMaterial);
			ceilingMaterial = framework->ResolveMaterial(assets.ceilingMaterial, ceilingMaterial);
			stairMesh = framework->ResolveMesh(assets.stairMeshPath, stairMesh);
			rampMesh = framework->ResolveMesh(assets.rampMeshPath, rampMesh);
		}
	}

	float rotation = building->GetRotation();
	auto& slot = nearComponentsByFloor[floorIndex];

	// 楼梯/坡道：井道墙体(和corridor/single/row同一套BuildWallsForElement)+按该层资产指定的
	// 3D网格摆一个实体；电梯只有井道墙体，不摆网格(轿厢单独由BuildElevatorCabinsForBuilding
	// 处理)。
	for (const Stair& stair : floor->GetStairs()) {
		BuildWallsForElement(floorBaseZ, floorHeight, stair.GetPosX(), stair.GetPosY(),
			stair.GetSizeX(), stair.GetSizeY(),
			stair.GetWall(FACE_WEST), stair.GetWall(FACE_EAST), stair.GetWall(FACE_NORTH), stair.GetWall(FACE_SOUTH),
			{}, {}, wallMaterial, floorIndex);
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
		BuildWallsForElement(floorBaseZ, floorHeight, ramp.GetPosX(), ramp.GetPosY(),
			ramp.GetSizeX(), ramp.GetSizeY(),
			ramp.GetWall(FACE_WEST), ramp.GetWall(FACE_EAST), ramp.GetWall(FACE_NORTH), ramp.GetWall(FACE_SOUTH),
			{}, {}, wallMaterial, floorIndex);
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
		BuildWallsForElement(floorBaseZ, floorHeight, elevator.GetPosX(), elevator.GetPosY(),
			elevator.GetSizeX(), elevator.GetSizeY(),
			elevator.GetWall(FACE_WEST), elevator.GetWall(FACE_EAST), elevator.GetWall(FACE_NORTH), elevator.GetWall(FACE_SOUTH),
			{}, {}, wallMaterial, floorIndex);
	}
	for (const Corridor& corridor : floor->GetCorridors()) {
		BuildWallsForElement(floorBaseZ, floorHeight, corridor.GetPosX(), corridor.GetPosY(),
			corridor.GetSizeX(), corridor.GetSizeY(),
			corridor.GetWall(FACE_WEST), corridor.GetWall(FACE_EAST), corridor.GetWall(FACE_NORTH), corridor.GetWall(FACE_SOUTH),
			corridor.GetDoors(), corridor.GetWindows(), wallMaterial, floorIndex);
	}
	// Single/Row槽位隐含四面都有墙——但槽位本身在实例化成真正的Room之后，门/窗/朝向都已经
	// 转移到Room身上(见Building::AssignRoom/ArrangeRow)，槽位自己的门窗数据不再是最新的，
	// 这里改成遍历building->GetRooms()按GetLayer()==level筛选，画每个真正Room的墙体，
	// 不直接用Floor::GetSingles()/GetRows()（那两个列表只是"模板槽位"，槽位数量/大小和实际
	// 生成的Room数量不是一一对应——ArrangeRow会把一个row槽位切成好几个Room）。
	for (Room* room : building->GetRooms()) {
		if (!room || room->GetLayer() != level) continue;
		BuildWallsForElement(floorBaseZ, floorHeight, room->GetPosX(), room->GetPosY(),
			room->GetSizeX(), room->GetSizeY(),
			true, true, true, true, // Single/Row隐含四面都有墙
			room->GetDoors(), room->GetWindows(), wallMaterial, floorIndex);
	}

	// 地板/天花板：老工程两层独立薄slab的做法——地板贴地坪往上一点点，天花板贴楼层顶往下
	// 一点点，中间留一条很窄的缝，避免和上/下相邻楼层的对应slab共面z-fighting。
	constexpr float kSlabThickness = 0.02f; // 地图单位，照抄老工程Ceiling/Ground的0.02f厚度
	float floorBottomWorld = BUILDING_HEIGHT_EPSILON + floorBaseZ * BUILDING_WORLD_SCALE;
	float floorTopWorld = BUILDING_HEIGHT_EPSILON + (floorBaseZ + floorHeight) * BUILDING_WORLD_SCALE;
	for (const Ground& ground : floor->GetGrounds()) {
		float wx, wy;
		ComputeWorldPosition(*building, ground.GetPosX(), ground.GetPosY(), wx, wy);
		float wz = floorBottomWorld + kSlabThickness * 0.5f * BUILDING_WORLD_SCALE;
		float sizeX = ground.GetSizeX() * BUILDING_WORLD_SCALE, sizeY = ground.GetSizeY() * BUILDING_WORLD_SCALE;
		float sizeZ = kSlabThickness * BUILDING_WORLD_SCALE;
		if (UStaticMeshComponent* comp = SpawnCube(wx, wy, wz, sizeX, sizeY, sizeZ, rotation, floorMaterial)) {
			slot.Add(comp);
		}
	}
	for (const Ceiling& ceiling : floor->GetCeilings()) {
		float wx, wy;
		ComputeWorldPosition(*building, ceiling.GetPosX(), ceiling.GetPosY(), wx, wy);
		float wz = floorTopWorld - kSlabThickness * 0.5f * BUILDING_WORLD_SCALE;
		float sizeX = ceiling.GetSizeX() * BUILDING_WORLD_SCALE, sizeY = ceiling.GetSizeY() * BUILDING_WORLD_SCALE;
		float sizeZ = kSlabThickness * BUILDING_WORLD_SCALE;
		if (UStaticMeshComponent* comp = SpawnCube(wx, wy, wz, sizeX, sizeY, sizeZ, rotation, ceilingMaterial)) {
			slot.Add(comp);
		}
	}
}

void ABuildingElement::BuildElevatorCabinsForBuilding() {
	if (!building || !framework.IsValid()) return;
	BuildingMod* mod = building->GetMod();
	if (!mod) return;

	float rotation = building->GetRotation();
	const vector<float>& floorHeights = building->GetFloorHeights();
	int32 basements = building->GetBasementCount();

	for (const ElevatorCabinSpec& spec : mod->cabins) {
		int32 minIdx = basements + spec.minFloor, maxIdx = basements + spec.maxFloor;
		int32 floorCount = static_cast<int32>(floorHeights.size());
		if (minIdx < 0 || minIdx >= floorCount || maxIdx < 0 || maxIdx >= floorCount) continue;

		const Floor* floor = building->GetFloor(spec.minFloor);
		if (!floor) continue;
		const vector<Elevator>& elevators = floor->GetElevators();
		if (spec.shaftIndex < 0 || spec.shaftIndex >= static_cast<int32>(elevators.size())) continue;
		const Elevator& elevator = elevators[spec.shaftIndex];

		// 轿厢mesh和楼梯/坡道同一个约定([0,1]单位包围盒，按实际尺寸缩放+按方向旋转)，
		// 直接复用ComputeDirectionalMeshTransform；Z缩放用轿厢"家"所在楼层(minFloor)的层高，
		// 不是min~max的整个垂直跨度——那个跨度是轿厢的移动范围，不是它自身的缩放尺寸。
		float meshSizeX, meshSizeY, meshRotation;
		ComputeDirectionalMeshTransform(elevator.GetDirection(), elevator.GetSizeX(), elevator.GetSizeY(),
			rotation, meshSizeX, meshSizeY, meshRotation);

		float wx, wy;
		ComputeWorldPosition(*building, elevator.GetPosX(), elevator.GetPosY(), wx, wy);

		float minFloorHeight = floorHeights[minIdx];
		float maxFloorHeight = floorHeights[maxIdx];
		float zBottom = building->GetFloorBaseZ(spec.minFloor) * BUILDING_WORLD_SCALE + BUILDING_HEIGHT_EPSILON;
		float zTop = (building->GetFloorBaseZ(spec.maxFloor) + maxFloorHeight) * BUILDING_WORLD_SCALE + BUILDING_HEIGHT_EPSILON;

		UStaticMesh* cabinMesh = framework->ResolveMesh(spec.cabinMeshPath, framework->GetDefaultCabinMesh());
		// isMovable=true——轿厢每帧要SetWorldLocation播往返动画，必须是Movable，和楼梯/坡道
		// (Static)不一样。
		UStaticMeshComponent* comp = SpawnMesh(wx, wy, zBottom,
			meshSizeX * BUILDING_WORLD_SCALE, meshSizeY * BUILDING_WORLD_SCALE, minFloorHeight * BUILDING_WORLD_SCALE,
			meshRotation, cabinMesh, nullptr, /*isMovable=*/true);
		if (!comp) continue;

		FCabin cabin;
		cabin.comp = comp;
		cabin.worldX = wx;
		cabin.worldY = wy;
		cabin.rotation = meshRotation;
		cabin.zBottom = zBottom;
		cabin.zTop = zTop;
		// 用世界坐标错开相位，避免同一栋楼/相邻楼的轿厢看起来同步摆动。
		cabin.phaseOffset = FMath::Fmod(FMath::Abs(wx) * 0.017f + FMath::Abs(wy) * 0.013f, 30.f);
		cabins.Add(cabin);
	}
}

void ABuildingElement::ClearNearSections() {
	for (TArray<TObjectPtr<UStaticMeshComponent>>& floorComponents : nearComponentsByFloor) {
		for (UStaticMeshComponent* comp : floorComponents) {
			if (!comp) continue;
			RemoveInstanceComponent(comp);
			comp->DestroyComponent();
		}
		floorComponents.Empty();
	}
	for (FCabin& cabin : cabins) {
		if (!cabin.comp) continue;
		RemoveInstanceComponent(cabin.comp);
		cabin.comp->DestroyComponent();
	}
	cabins.Empty();
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
		break;
	}
}

void ABuildingElement::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	if (!building || !framework.IsValid()) return;

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
				lodOpQueue.Enqueue({ ELodOpType::BuildFloorMesh, i });
			}
			lodOpQueue.Enqueue({ ELodOpType::BuildElevatorCabins, -1 });
			lodOpQueue.Enqueue({ ELodOpType::DeleteFarMesh, -1 });
			transitionPending = true;
		}
		else if (currentLod == ELod::Near && dist > framework->GetLodFarExitDistance()) {
			lodOpQueue.Enqueue({ ELodOpType::BuildFarMesh, -1 });
			lodOpQueue.Enqueue({ ELodOpType::DeleteAllNearMeshes, -1 });
			transitionPending = true;
		}
	}

	// 每个操作都要先问框架组件要一份全地图共享的预算，预算耗尽这一帧就先停手，下一帧再继续
	// ——避免大量building同时穿越距离阈值时，所有Element在同一帧一起疯狂建组件卡成一张ppt。
	// 必须先判断队列是否为空再消耗预算(不能反过来)，否则每一栋"这一帧其实无事可做"的building
	// 也会白白吃掉一份全局预算，挤占真正需要建楼层的building的份额。
	while (!lodOpQueue.IsEmpty() && framework->TryConsumeLodOpBudget()) {
		FLodOp op;
		lodOpQueue.Dequeue(op);
		ExecuteLodOp(op);
	}

	// 电梯轿厢往返动画——只在currentLod==Near时做，和上面的LOD切换队列无关(不受
	// transitionPending影响，转场期间已经建好的轿厢照常继续动)。只用FCabin里缓存的浮点数，
	// 不解引用building。
	if (currentLod == ELod::Near) {
		float worldTime = GetWorld()->GetTimeSeconds();
		for (FCabin& cabin : cabins) {
			if (!cabin.comp) continue;
			float z = ComputeCabinZ(cabin.zBottom, cabin.zTop, framework->GetCabinCruiseSpeed(),
				framework->GetCabinEaseSeconds(), worldTime + cabin.phaseOffset);
			cabin.comp->SetWorldLocation(FVector(cabin.worldX, cabin.worldY, z));
		}
	}
}
