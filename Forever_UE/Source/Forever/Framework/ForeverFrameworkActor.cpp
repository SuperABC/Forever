#include "Framework/ForeverFrameworkActor.h"

#include "map/map.h"
#include "populace/populace.h"
#include "story/story.h"

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
	// 这个Actor整个生命周期都不会移动(纯粹是"世界生成结果的挂载点")，根组件应该是Static、
	// 不是引擎默认的Movable——不这么做的话，挂在它下面的每一个子组件(哪怕子组件自己显式设成
	// Static)都会被引擎按"父级更活跃"的规则强制视为Movable，导致渲染器的动态图元八叉树/
	// 物理引擎的动态broadphase里堆积海量本该是静态的图元(每栋building的墙体分段、每个
	// Room/Building/Zone的碰撞盒等等，数量级是成千上万)，且这两套结构对"新增/移除一个动态
	// 图元"的开销会随着已有动态图元数量增长而变差——这正是"建筑近处LOD每次整层增删都巨卡，
	// 而且感觉越来越卡"的根因，见ForeverBuildingFrameworkComponent.md"性能"一节。
	sceneRoot->SetMobility(EComponentMobility::Static);

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
	delete story;
	delete populace;
	delete map;
}

void AForeverFrameworkActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 同步释放map/populace/story(见头文件EndPlay声明处的注释)——不能只靠析构函数兜底，
	// 那个时机取决于UObject垃圾回收，不保证在下一次PIE开始前跑完。populace/map/story删除
	// 顺序互不影响内存安全(Citizen只持有Zone*/Building*/Room*裸指针、析构不解引用它们；Map
	// 也不持有任何Citizen*；Story不持有Map/Populace)，这里先删story只是保持和创建顺序相反
	// 的直觉。
	delete story;
	story = nullptr;
	delete populace;
	populace = nullptr;
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

	// Populace和Map平级，不是Map的成员——这里编排两者之间唯一的交互(Map::
	// ComputeAccommodationTarget()喂给Populace::Init()，Populace::Init()跑完的结果再喂给
	// Map::Checkin())，和老工程GlobalBase里"int accomodation = map->InitContents();
	// populace->Init(accomodation, ...); map->Checkin(populace, ...);"同一个编排顺序，
	// 详见Source/Core/populace/populace.md。
	populace = new Populace();
	populace->Init(map->ComputeAccommodationTarget());
	map->Checkin(*populace);

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
	// Room进入/离开检测碰撞盒现在由ABuildingElement自己生成(每栋building的所有Room都跟着
	// 这栋楼自己的近/远LOD组件一起attach到它专属的Actor上，不再是roomFramework这个单例组件
	// 统一生成)，buildingFramework->GenerateBuildings(map)已经涵盖了这一步，不需要在这里
	// 再单独调用roomFramework——见Source/Forever/Element/BuildingElement.md。
	if (buildingFramework) {
		buildingFramework->GenerateBuildings(map);
	}
	if (populaceFramework) {
		// 这里不SpawnActor任何ACitizenElement——populaceFramework只是缓存
		// populace->GetCitizens()列表，真正的生成/销毁全部按玩家距离在TickComponent里做，
		// 见Source/Forever/Framework/ForeverPopulaceFrameworkComponent.md。
		populaceFramework->GenerateCitizens(map, populace);
	}

	// 阶段4 Story落地：和map/populace不互相依赖，放在最后创建即可。Init()读取
	// Resource/Story/test.json，随后立刻广播一次GameStartEvent，见storyFramework.md。
	story = new Story();
	story->Init();
	if (storyFramework) {
		storyFramework->Init(story);
		storyFramework->BroadcastGameStart();
	}
}
