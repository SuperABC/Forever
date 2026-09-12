#include "Framework/ForeverFrameworkActor.h"

#include "map/map.h"

#include "Framework/ForeverAssetFrameworkComponent.h"
#include "Framework/ForeverBuildingFrameworkComponent.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"
#include "Framework/ForeverRoadnetFrameworkComponent.h"
#include "Framework/ForeverRoomFrameworkComponent.h"
#include "Framework/ForeverStoryFrameworkComponent.h"
#include "Framework/ForeverTerrainFrameworkComponent.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"
#include "Framework/ForeverZoneFrameworkComponent.h"
#include "Components/SceneComponent.h"

AForeverFrameworkActor::AForeverFrameworkActor()
{
	PrimaryActorTick.bCanEverTick = false;

	sceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = sceneRoot;

	assetFramework = CreateDefaultSubobject<UForeverAssetFrameworkComponent>(TEXT("AssetFramework"));
	buildingFramework = CreateDefaultSubobject<UForeverBuildingFrameworkComponent>(TEXT("BuildingFramework"));
	populaceFramework = CreateDefaultSubobject<UForeverPopulaceFrameworkComponent>(TEXT("PopulaceFramework"));
	roadnetFramework = CreateDefaultSubobject<UForeverRoadnetFrameworkComponent>(TEXT("RoadnetFramework"));
	roomFramework = CreateDefaultSubobject<UForeverRoomFrameworkComponent>(TEXT("RoomFramework"));
	storyFramework = CreateDefaultSubobject<UForeverStoryFrameworkComponent>(TEXT("StoryFramework"));
	terrainFramework = CreateDefaultSubobject<UForeverTerrainFrameworkComponent>(TEXT("TerrainFramework"));
	trafficFramework = CreateDefaultSubobject<UForeverTrafficFrameworkComponent>(TEXT("TrafficFramework"));
	zoneFramework = CreateDefaultSubobject<UForeverZoneFrameworkComponent>(TEXT("ZoneFramework"));
}

AForeverFrameworkActor::~AForeverFrameworkActor()
{
	delete map;
}

void AForeverFrameworkActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 同步释放map(见头文件EndPlay声明处的注释)——不能只靠析构函数兜底，
	// 那个时机取决于UObject垃圾回收，不保证在下一次PIE开始前跑完。
	delete map;
	map = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AForeverFrameworkActor::BeginPlay()
{
	Super::BeginPlay();

	// 阶段2临时日志:骨架里其余8个域组件还是空实现,没有任何可见效果,
	// 靠这条日志在PIE的Output Log里确认Actor+组件都正常构造/初始化。
	// 阶段4逐个域落地后可以删除或改成更有用的调试信息。
	UE_LOG(LogTemp, Log, TEXT("AForeverFrameworkActor initialized with 9 framework components: Asset/Building/Populace/Roadnet/Room/Story/Terrain/Traffic/Zone"));

	EnsureMapGenerated();
}

namespace {
	// 默认地图尺寸(地图元素,1元素=10m)。必须满足两个条件:
	// 1) MountainTerrain的生成密度公式densityScale=width*height/(512*512)(整数除法)要
	//    严格大于1才会生成山(densityScale=1时mountainCount直接是0)——512x512刚好等于1,
	//    完全不出山,这是本来的错误,1024x1024给densityScale=4。
	// 2) LOD覆盖范围num=log2(target)-1,最粗一级LOD网格跨度4*2^(num-1)个元素必须正好等于
	//    地图边长,最外圈网格才能完整盖住地图边缘——只有target是2的整数次幂才精确成立,
	//    所以不能只调大随便一个数,得继续选2的整数次幂。详见Source/Basic/map/terrain_basic.md。
	constexpr int kDefaultMapWidth = 1024;
	constexpr int kDefaultMapHeight = 1024;
}

void AForeverFrameworkActor::EnsureMapGenerated()
{
	if (map) return;

	map = new Map(kDefaultMapWidth, kDefaultMapHeight);
	map->InitTerrains();
	map->InitRoadnet();
	map->InitZones();
	map->InitBuildings();

	if (terrainFramework) {
		terrainFramework->GenerateTerrain(map);
	}
	if (roadnetFramework) {
		// InitZones/InitBuildings裁剪Lot自由空间时新增的小路(Map::GetPathRoads())要在这里面
		// 一起画出来，必须放在InitZones/InitBuildings跑完之后调用，见
		// ForeverRoadnetFrameworkComponent.md"小路可视化"一节。
		roadnetFramework->GenerateRoadnet(map);
	}
	if (zoneFramework) {
		zoneFramework->GenerateZones(map);
	}
	if (buildingFramework) {
		buildingFramework->GenerateBuildings(map);
	}
}
