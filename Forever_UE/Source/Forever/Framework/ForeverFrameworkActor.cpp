#include "Framework/ForeverFrameworkActor.h"

#include "map/map.h"
#include "map/room.h"
#include "populace/populace.h"
#include "populace/citizen.h"
#include "society/society.h"
#include "society/organization.h"
#include "society/job.h"
#include "story/story.h"
#include "story/change.h"
#include "industry/industry.h"
#include "traffic/traffic.h"
#include "player/player.h"
#include "common/registry.h"
#include "common/utility.h"

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
	PrimaryActorTick.bCanEverTick = true; // 驱动Player::Tick(全局时钟)，见.h的Tick覆写注释

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
	delete player;
	delete traffic;
	delete industry;
	delete society;
	delete story;
	delete populace;
	delete map;
}

void AForeverFrameworkActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 同步释放map/populace/story及新增的society/industry/traffic/player(见头文件EndPlay
	// 声明处的注释)——不能只靠析构函数兜底，那个时机取决于UObject垃圾回收，不保证在下一次
	// PIE开始前跑完。这几个对象删除顺序互不影响内存安全(彼此都不持有对方的裸指针)，这里
	// 按和创建相反的顺序删只是保持直觉。
	delete player;
	player = nullptr;
	delete traffic;
	traffic = nullptr;
	delete industry;
	industry = nullptr;
	delete society;
	society = nullptr;
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

	// 每次真正开局都要重新按当前已经读进内存的config内容刷新一遍参数表——不同局可能用不同
	// 的config.json(参数因此也可能不同),但mod dll的发现/注册(Registry构造函数)只应该跑
	// 一次,两者调用频率不同,不能合并,见Source/Core/common/registry.md。放在这里(而不是
	// 某一个具体域的Ensure*Generated()里)是因为它不属于任何单个域,下面7个Ensure*Generated()
	// 都会用到某个Factory,理应在它们之前统一刷新一次。

	Registry::Get().ReloadModArgs();

	// 按依赖顺序显式列出全部7个域的Ensure*Generated()——一个函数体内写出来的调用顺序就是
	// 实际执行顺序,不存在"调用方可能不按顺序调"这回事,所以每个Ensure*Generated()自己只保证
	// 自己那个域,不会再调用别的Ensure*Generated()。顺序本身由真实依赖决定:EnsurePopulaceGenerated
	// 要用到map(Map::ComputeAccommodationTarget/Checkin),必须排在EnsureMapGenerated之后;
	// EnsurePlayerGenerated要用到populace->GetCurrentYear(),必须排在EnsurePopulaceGenerated
	// 之后;Society/Industry/Traffic/Story这次都是互相独立的空骨架或独立数据源,顺序上没有
	// 别的硬性要求。
	EnsureMapGenerated();
	EnsurePopulaceGenerated();
	EnsureSocietyGenerated();
	EnsurePlayerGenerated();
	EnsureIndustryGenerated();
	EnsureTrafficGenerated();
	EnsureStoryGenerated();
}

void AForeverFrameworkActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// player在EnsurePlayerGenerated()跑完之前是nullptr——BeginPlay里7个Ensure*Generated()
	// 在Super::BeginPlay()之后同步跑完，引擎只会在BeginPlay之后才调用Tick，理论上不会遇到
	// 空指针，这里判空只是和其它地方一致的防御性写法。
	if (!player) return;
	player->Tick(DeltaTime);

	// 持续在屏幕左上角显示当前游戏内时间——固定key(不是-1)保证每帧原地刷新同一行，不会
	// 每帧都新增一条往下堆；TimeToDisplay给1秒(比一帧长)，哪怕某一帧意外跳过也不会闪烁。
	if (GEngine) {
		GEngine->AddOnScreenDebugMessage(/*Key=*/9000, /*TimeToDisplay=*/1.f, FColor::White,
			FString::Printf(TEXT("游戏时间: %s"), UTF8_TO_TCHAR(player->GetTime()->ToString(true, true).c_str())));
	}

	// 驱动Job/Organization各自独立的DailyPlan/ExecNode调度（两套独立的timer，见
	// populace.md/society.md"两套独立timer"一节）。ExecNode这次是纯C++通道，直接给
	// Change*，Populace/Society自己不delete这些指针，所有权留在产出它们的
	// JobMod/OrganizationMod实例身上，这里只读值使用。
	// 开局当天永远不会天然触发一次CrossDay()(时钟是EnsurePlayerGenerated刚设好的，"day"
	// 缓存和当前日期本来就相同)，第一帧强制当成"跨天"，让第一天的调度表也能生成，见.h里
	// bIsFirstTick的说明。
	bool crossedDay = bIsFirstTick || player->CrossDay();
	bIsFirstTick = false;

	if (populace) {
		populace->Tick(*player->GetTime(), crossedDay,
			[this](Citizen* citizen, const std::vector<Change*>& changes) {
				for (Change* change : changes) {
					if (auto* nav = dynamic_cast<const NPCNavigateChange*>(change)) {
						ScriptContext context; // NPCNavigateChange的Expression字段这次都是
						// 字面量常量，求值不需要真正有意义的context，但Evaluate接口要求传一份
						std::string destinationText = ToString(nav->GetDestination().EvaluateValue(context));
						Room* dest = destinationText == "home"
							? citizen->GetRoom()
							: (citizen->GetJob() ? citizen->GetJob()->GetPosition() : nullptr);
						UE_LOG(LogTemp, Log, TEXT("[DEBUG-FREEZE] dispatch citizen=%s dest=%s destRoomNull=%s populaceFrameworkNull=%s"),
							UTF8_TO_TCHAR(citizen->GetName().c_str()), UTF8_TO_TCHAR(destinationText.c_str()),
							dest ? TEXT("false") : TEXT("true"), populaceFramework ? TEXT("false") : TEXT("true"));
						if (dest && populaceFramework) {
							populaceFramework->RequestWalk(citizen, dest);
						}
					}
					else if (story && citizen->GetJob()) {
						ScriptContext context;
						context.self = citizen->GetJob()->GetScript();
						story->ApplyChange(change, context);
					}
				}
			});
	}

	if (society) {
		society->Tick(*player->GetTime(), crossedDay,
			[this](Organization*, const std::vector<Change*>& changes) {
				// ShopOrganization这次不产出changes，回调体基本用不到，但设施要落地——
				// 真有Change产出时和上面Populace::Tick的回调同一个"context.self指向
				// 产出它的Script"的处理方式（Organization目前没有自己的Script，等以后
				// 真的有组织级Change时再补）。
				for (Change* change : changes) {
					if (story) {
						ScriptContext context;
						story->ApplyChange(change, context);
					}
				}
			});
	}
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
	// Room进入/离开检测碰撞盒现在由ABuildingElement自己生成(每栋building的所有Room都跟着
	// 这栋楼自己的近/远LOD组件一起attach到它专属的Actor上，不再是roomFramework这个单例组件
	// 统一生成)，buildingFramework->GenerateBuildings(map)已经涵盖了这一步，不需要在这里
	// 再单独调用roomFramework——见Source/Forever/Element/BuildingElement.md。
	if (buildingFramework) {
		buildingFramework->GenerateBuildings(map);
	}
}

void AForeverFrameworkActor::EnsurePopulaceGenerated()
{
	if (populace) return;

	// Populace和Map平级，不是Map的成员——这里编排两者之间唯一的交互(Map::
	// ComputeAccommodationTarget()喂给Populace::Init()，Populace::Init()跑完的结果再喂给
	// Map::Checkin())，和老工程GlobalBase里"int accomodation = map->InitContents();
	// populace->Init(accomodation, ...); map->Checkin(populace, ...);"同一个编排顺序，
	// 详见Source/Core/populace/populace.md。假定map已经生成好，调用方(BeginPlay)负责保证
	// EnsureMapGenerated()已经先跑过。
	populace = new Populace();
	populace->Init(map->ComputeAccommodationTarget());
	map->Checkin(*populace);

	if (populaceFramework) {
		// 这里不SpawnActor任何ACitizenElement——populaceFramework只是缓存
		// populace->GetCitizens()列表，真正的生成/销毁全部按玩家距离在TickComponent里做，
		// 见Source/Forever/Framework/ForeverPopulaceFrameworkComponent.md。
		populaceFramework->GenerateCitizens(map, populace);
	}
}

void AForeverFrameworkActor::EnsureSocietyGenerated()
{
	if (society) return;

	// Society这次真的做了组织分配+招聘（不是空骨架了）：按地图里所有Component加权随机
	// 分配Organization（每个Organization自己遍历claimed components设计Job），再把成年
	// 市民随机匹配到还空缺的Job上，见society.md。
	society = new Society();
	society->Init(map->GetAllComponents());
	society->RecruitCitizens(populace->GetCitizens(), populace->GetCurrentYear());
}

void AForeverFrameworkActor::EnsurePlayerGenerated()
{
	if (player) return;

	// Player这次迁移了全局时钟部分，Init()创建Time并交给AForeverFrameworkActor::Tick每帧
	// 推进，见player.md。
	player = new Player();
	player->Init();
	// 市民繁衍模拟从2000年起演化到populace自己认为的"当前年份"(Populace::GetCurrentYear()，
	// 老工程population.cpp里time->SetYear(year+2000)的新工程对应物——那边直接改Player的
	// 时钟，这边改成读populace算出来的年份再喂给Player::SetTime)，开局时间定为那一年的
	// 1月1日8点，让Citizen的生日/年龄和这个初始时钟保持自洽，见populace.md"生日换算"一节。
	// 假定populace已经生成好，调用方(BeginPlay)负责保证EnsurePopulaceGenerated()已经先跑过。
	player->SetTime(Time(populace->GetCurrentYear(), 1, 1, 8));
}

void AForeverFrameworkActor::EnsureIndustryGenerated()
{
	if (industry) return;

	// Industry这次还是空骨架，新增它纯粹是为了让PostImplement能拿到7个域的真实指针。
	industry = new Industry();
}

void AForeverFrameworkActor::EnsureTrafficGenerated()
{
	if (traffic) return;

	// Traffic这次还是空骨架，新增它纯粹是为了让PostImplement能拿到7个域的真实指针。
	traffic = new Traffic();
}

void AForeverFrameworkActor::EnsureStoryGenerated()
{
	if (story) return;

	// 阶段4 Story落地：和map/populace不互相依赖。Init()读取Resource/Story/test.json，
	// 随后立刻广播一次GameStartEvent，见storyFramework.md。
	story = new Story();
	story->Init();
	if (storyFramework) {
		storyFramework->Init(story);
		storyFramework->BroadcastGameStart();
	}
}
