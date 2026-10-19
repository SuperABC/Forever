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
#include "story/script.h"
#include "industry/industry.h"
#include "traffic/traffic.h"
#include "player/player.h"
#include "common/registry.h"
#include "common/utility.h"
#include "common/implement.h"
#include "common/config.h"
#include "common/error.h"
#include "common/save.h"
#include "map/map_save.h"
#include "populace/populace_save.h"
#include "society/society_save.h"
#include "traffic/traffic_save.h"
#include "story/script_save.h"
#include "player/player_save.h"
#include "Player/ForeverCharacter.h"
#include "common/json.h"
#include "Element/CitizenElement.h"
#include "Element/VehicleElement.h"
#include "Element/DoorComponent.h"
#include "Element/BuildingElement.h"
#include "map/cabin.h"
#include "map/door.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"

#include "Framework/ForeverAssetFrameworkComponent.h"
#include "Framework/ForeverBuildingFrameworkComponent.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"
#include "Framework/ForeverRoadnetFrameworkComponent.h"
#include "Framework/ForeverRoomFrameworkComponent.h"
#include "Framework/ForeverSkyFrameworkComponent.h"
#include "Framework/ForeverStoryFrameworkComponent.h"
#include "Framework/ForeverTerrainFrameworkComponent.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"
#include "Framework/ForeverVegetationFrameworkComponent.h"
#include "Mod/ForeverSaveSubsystem.h"
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
	skyFramework = CreateDefaultSubobject<UForeverSkyFrameworkComponent>(TEXT("SkyFramework"));
	storyFramework = CreateDefaultSubobject<UForeverStoryFrameworkComponent>(TEXT("StoryFramework"));
	terrainFramework = CreateDefaultSubobject<UForeverTerrainFrameworkComponent>(TEXT("TerrainFramework"));
	trafficFramework = CreateDefaultSubobject<UForeverTrafficFrameworkComponent>(TEXT("TrafficFramework"));
	vegetationFramework = CreateDefaultSubobject<UForeverVegetationFrameworkComponent>(TEXT("VegetationFramework"));
	zoneFramework = CreateDefaultSubobject<UForeverZoneFrameworkComponent>(TEXT("ZoneFramework"));
}

AForeverFrameworkActor::~AForeverFrameworkActor()
{
	// 必须先等所有已经派发到线程池的异步寻路任务彻底跑完，才能delete map/populace——
	// 那些后台线程手上还攥着map裸指针(算Dijkstra)、算完还要touch citizen/destination
	// (推进pathResultQueue)，谁先跑完不确定，见
	// UForeverPopulaceFrameworkComponent::WaitForPendingPathfinding()的注释。
	UForeverPopulaceFrameworkComponent::WaitForPendingPathfinding();
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

	// 同上，必须先等所有异步寻路任务跑完——不能用"派发一个GameThread任务、等它跑完"的
	// 方案在这里等，会在EndPlay这个本身跑在GameThread上的调用栈里死锁。
	UForeverPopulaceFrameworkComponent::WaitForPendingPathfinding();

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

	// 在PIE的Output Log里确认Actor+全部9个域组件都正常构造/初始化。
	UE_LOG(LogTemp, Log, TEXT("AForeverFrameworkActor initialized with 9 framework components: Asset/Building/Populace/Roadnet/Room/Story/Terrain/Traffic/Zone"));

	// 每次真正开局都要重新按当前已经读进内存的config内容刷新一遍参数表——不同局可能用不同
	// 的config.json(参数因此也可能不同),但mod dll的发现/注册(Registry构造函数)只应该跑
	// 一次,两者调用频率不同,不能合并,见Source/Core/common/registry.md。放在这里(而不是
	// 某一个具体域的Ensure*Generated()里)是因为它不属于任何单个域,下面7个Ensure*Generated()
	// 都会用到某个Factory,理应在它们之前统一刷新一次。

	Registry::Get().ReloadModArgs();

	// 读档：各个域在各自的Ensure*Generated里按存档恢复；较早的存档缺哪一段，那个域就照新开局
	// 生成(见Core/common/save.md"版本号"一节的required)。mod集合已经在RequestLoad里按存档设好了。
	if (FForeverPendingLoad* load = FetchActiveLoad()) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverFrameworkActor: loading %s - the world is restored from the save (sections missing from older saves are regenerated)."),
			*load->path);
	}

	// 按依赖顺序显式列出全部7个域的Ensure*Generated()——一个函数体内写出来的调用顺序就是
	// 实际执行顺序,不存在"调用方可能不按顺序调"这回事,所以每个Ensure*Generated()自己只保证
	// 自己那个域,不会再调用别的Ensure*Generated()。顺序本身由真实依赖决定:EnsurePopulaceGenerated
	// 要用到map(Map::ComputeAccommodationTarget/Checkin),必须排在EnsureMapGenerated之后;
	// EnsurePlayerGenerated要用到populace->GetCurrentYear(),必须排在EnsurePopulaceGenerated
	// 之后;Society/Industry/Traffic/Story这次都是互相独立的空骨架或独立数据源,顺序上没有
	// 别的硬性要求。ValidateMainStoryDependencies()放在最前面——校验主线剧情.script声明的
	// mod_dependences，不满足就直接拒绝生成世界，不浪费时间跑完地图/市民生成再失败。
	//
	// 这一整段包一层try/catch：Core层遇到"生成世界过程中出现的致命错误"（mod_dependences
	// 缺失依赖、Name::GenerateName重试耗尽仍撞上name_reserve占位名、chinese取名器未启用等，
	// 见populace.md/name.md/script.md相关章节）统一THROW_EXCEPTION，不需要每一层调用方
	// 各自判断/传递失败信号——这里是唯一需要真正处理这类异常的地方（打日志+退出游戏），
	// 中间的Populace::Init()/EnsurePopulaceGenerated()等每一层都不需要感知这个异常。
	try {
		ValidateMainStoryDependencies();
		EnsureMapGenerated();
		EnsurePopulaceGenerated();
		EnsureSocietyGenerated();
		EnsurePlayerGenerated();
		EnsureIndustryGenerated();
		EnsureTrafficGenerated();
		EnsureStoryGenerated();

		// 必须排在EnsureStoryGenerated()(进而BroadcastGameStart())之后——车辆自己的Script
		// (vehicle_basic.script)的game_start milestone要先广播完、把"上车"选项加进
		// vehicle->GetOptions()，GenerateVehicles()生成AVehicleElement时BuildProximityBox()
		// 烘焙的选项快照才不会是空的，见EnsureTrafficGenerated()声明处的说明。
		if (trafficFramework) {
			trafficFramework->GenerateVehicles(map, traffic);
			trafficFramework->BuildRouteDebugMesh(traffic);
			trafficFramework->BuildTracks(traffic);
		}

		// 读档跳过了game_start(它本来会随机交给玩家一个市民)：按存档恢复玩家操控的市民/车辆。
		// 必须在GenerateVehicles之后——上车要用到车辆Actor。
		if (bScriptsRestored) {
			if (FForeverPendingLoad* load = FetchActiveLoad()) RestoreControlledPawn(*load);
		}

	}
	catch (const ExceptionBase& e) {
		UE_LOG(LogTemp, Error, TEXT("生成世界时发生致命错误，拒绝继续，退出游戏：%s"),
			UTF8_TO_TCHAR(e.GetDetailedInfo().data()));
		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(GetWorld(), 0),
			EQuitPreference::Quit, false);
	}
}

void AForeverFrameworkActor::ValidateMainStoryDependencies() {
	// 优先遍历Story screen里玩家显式添加的脚本列表(Config::GetStoryScripts())——这个列表
	// 为空时才退回旧的单路径查找，保证编辑器里不经过菜单、直接PIE在World.umap上的既有工作流
	// 不受影响。这里只是个兜底二次校验：真正阻止玩家进入游戏的校验发生在配置界面点击"开始
	// 游戏"的时候(更早)，见ForeverConfigBridgeSubsystem::ValidateAndStartGame。
	std::vector<std::string> scripts = Config::GetStoryScripts();
	if (scripts.empty()) {
		std::string fallback = Config::GetMainStoryScriptPath();
		if (!fallback.empty()) {
			scripts.push_back(fallback);
		}
	}

	for (const std::string& scriptPath : scripts) {
		for (const std::string& id : Script::GetModDependences(scriptPath)) {
			if (!Registry::Get().CheckModRegistered(id)) {
				THROW_EXCEPTION(RuntimeException, "Main story script depends on mod '" + id + "' which is not registered.\n");
			}
		}
	}
}

void AForeverFrameworkActor::ApplyChange(const Change* change, const ScriptContext& context) {
	if (auto* nav = dynamic_cast<const NPCNavigateChange*>(change)) {
		// NPCNavigateChange::destination是纯std::string（原因见change.h顶部崩溃分析注释）。
		// 这里不再能像原来lambda那样直接拿到Citizen*（这个函数的调用方只给Change本身+
		// context），改成用change自带的occupant姓名反查Populace（Populace::
		// FindCitizenByName）——NPCNavigateChange构造时的name本来就是occupantName，和
		// Citizen::GetName()一致。
		// mod生成的NPCNavigateChange里是现成的姓名/房间地址；脚本(npc_navigate)里写的可能是
		// 表达式(比如$$self.name)——先按原样查，查不到再当表达式求值后查。
		Citizen* citizen = populace ? populace->FindCitizenByName(nav->GetName()) : nullptr;
		if (!citizen && populace) citizen = populace->FindCitizenByName(ToString(EvaluateExpression(nav->GetName(), context)));
		Room* dest = map ? map->LocateRoom(nav->GetDestination()) : nullptr;
		if (!dest && map) dest = map->LocateRoom(ToString(EvaluateExpression(nav->GetDestination(), context)));
		if (citizen && dest && populaceFramework) {
			populaceFramework->RequestWalk(citizen, dest);
		}
		return;
	}

	if (dynamic_cast<const PlaceHolderChange*>(change)) {
		// 只是ScriptMod::FindLabel用来定位插入位置的锚点，执行时什么都不做——不能落到下面七个域，
		// 否则每次都会打"未实现"警告。
		return;
	}

	if (auto* debugPrint = dynamic_cast<const DebugPrintChange*>(change)) {
		std::string text = ToString(EvaluateExpression(debugPrint->GetMessage(), context));
		if (GEngine) {
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, UTF8_TO_TCHAR(text.c_str()));
		}
		return;
	}

	if (auto* controlChange = dynamic_cast<const ChangeControlChange*>(change)) {
		// 真正的操控权切换逻辑留在UForeverStoryFrameworkComponent::ApplyControlChange里不动
		// （它本来就知道怎么找/生成citizen Actor、怎么Possess），这里只是转发调用。
		if (storyFramework) storyFramework->ApplyControlChange(controlChange, context);
		return;
	}

	if (auto* startPuzzle = dynamic_cast<const StartPuzzleChange*>(change)) {
		// 跟ChangeControlChange同一个理由：Core层不知道UMG Widget的存在，"弹出一个小游戏
		// 界面"这件事只能转发给UE层处理，见UForeverStoryFrameworkComponent::ApplyStartPuzzle。
		if (storyFramework) storyFramework->ApplyStartPuzzle(startPuzzle, context);
		return;
	}

	if (auto* forRange = dynamic_cast<const ForRangeChange*>(change)) {
		// 子Change可能是任何类型(包括UE层的)，所以循环放在这个统一入口里递归。循环变量写进当前脚本
		// 自己的变量池：var写"floor"或"self.floor"都是$$self.floor。含两端，step省略为1；循环次数
		// 超过kMaxForRangeIterations截断并警告。子Change的condition每轮都按当前循环变量求值。
		constexpr int kMaxForRangeIterations = 1000;
		std::string key = forRange->GetVar();
		if (key.rfind("self.", 0) == 0) key = key.substr(5);
		int from = ToInt(EvaluateExpression(forRange->GetFrom(), context));
		int to = ToInt(EvaluateExpression(forRange->GetTo(), context));
		int step = forRange->GetStep().empty() ? 1 : ToInt(EvaluateExpression(forRange->GetStep(), context));
		if (step == 0) step = 1;
		int iterations = 0;
		for (int value = from; step > 0 ? value <= to : value >= to; value += step) {
			if (++iterations > kMaxForRangeIterations) {
				UE_LOG(LogTemp, Warning, TEXT("AForeverFrameworkActor::ApplyChange: for_range exceeded %d iterations, truncated."), kMaxForRangeIterations);
				break;
			}
			if (context.self) context.self->SetValue(key, ValueType(value));
			for (const Change* child : forRange->GetChanges()) {
				if (!child) continue;
				if (!child->GetCondition().empty() && !EvaluateExpressionBool(child->GetCondition(), context)) continue;
				ApplyChange(child, context);
			}
		}
		return;
	}

	if (auto* globalMessage = dynamic_cast<const GlobalMessageChange*>(change)) {
		// 求出消息文本，广播一个GlobalMessageEvent给所有挂了Script的东西，见
		// UForeverStoryFrameworkComponent::BroadcastGlobalMessage。
		if (storyFramework) {
			storyFramework->BroadcastGlobalMessage(ToString(EvaluateExpression(globalMessage->GetMessage(), context)));
		}
		return;
	}

	if (auto* createTimer = dynamic_cast<const CreateTimerChange*>(change)) {
		// 计时器的归属要按脚本的稳定键记录(可能指向别的域的对象，见story.md"计时器")，需要全部域，
		// 交给StoryFramework处理。
		if (storyFramework) storyFramework->ApplyCreateTimer(createTimer, context);
		return;
	}

	if (auto* removeTimer = dynamic_cast<const RemoveTimerChange*>(change)) {
		if (storyFramework) storyFramework->ApplyRemoveTimer(removeTimer, context);
		return;
	}

	if (auto* enterVehicle = dynamic_cast<const EnterVehicleChange*>(change)) {
		// 跟ChangeControlChange同一个理由：Core层不认识Actor/Controller，"把操控权切换到
		// 载具上"这件事只能转发给UE层处理，见UForeverStoryFrameworkComponent::ApplyEnterVehicle。
		if (storyFramework) storyFramework->ApplyEnterVehicle(enterVehicle, context);
		return;
	}

	// 都不是这个Actor自己能处理的类型，转发给七个域各自的ApplyChange，谁认识就处理(同一种
	// Change可以有好几个域都认识，比如add_option门/市民/车辆各管各的)。七个域都不认识时才打一条
	// "未实现"警告——用UE_LOG，进Output Log/Forever.log(Core的debugf只走OutputDebugString)。
	bool handled = false;
	if (map) handled |= map->ApplyChange(change, context);
	if (populace) handled |= populace->ApplyChange(change, context);
	if (society) handled |= society->ApplyChange(change, context);
	if (industry) handled |= industry->ApplyChange(change, context);
	if (traffic) handled |= traffic->ApplyChange(change, context);
	if (story) handled |= story->ApplyChange(change, context);
	if (player) handled |= player->ApplyChange(change, context);
	if (!handled) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverFrameworkActor::ApplyChange: change type %s is not implemented yet."),
			UTF8_TO_TCHAR(change->GetType().c_str()));
	}

	// 交互选项变了：场景里对应的市民/车辆/门Actor刷新自己的选项快照(玩家站在旁边时连同屏幕上的
	// 选项列表一起换)，否则要等它下次被重新生成才看得到变化。
	if (auto* addOption = dynamic_cast<const AddOptionChange*>(change)) {
		RefreshInteractOptions(ToString(EvaluateExpression(addOption->GetName(), context)));
	}
	else if (auto* removeOption = dynamic_cast<const RemoveOptionChange*>(change)) {
		RefreshInteractOptions(ToString(EvaluateExpression(removeOption->GetName(), context)));
	}
}

void AForeverFrameworkActor::RefreshInteractOptions(const std::string& name) {
	if (name.empty()) return;
	if (populace && populaceFramework) {
		if (Citizen* citizen = populace->FindCitizenByName(name)) {
			if (ACitizenElement* element = populaceFramework->FindActiveCitizen(citizen)) element->RefreshOptions();
		}
	}
	if (trafficFramework) {
		if (AVehicleElement* element = trafficFramework->FindVehicleElement(UTF8_TO_TCHAR(name.c_str()))) element->RefreshOptions();
	}
	if (map) {
		if (Door* door = map->FindDoorByName(name)) {
			if (UForeverDoorComponent* component = UForeverDoorComponent::FindForDoor(door)) component->RefreshOptions();
		}
		if (Cabin* cabin = map->FindCabinByName(name)) {
			if (ABuildingElement* element = ABuildingElement::FindForBuilding(cabin->GetBuilding())) element->RefreshCabinOptions(cabin);
		}
	}
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

	// 昼夜循环：根据刚推进过的游戏时钟驱动太阳/天空/雾/后处理，见
	// ForeverSkyFrameworkComponent.md。
	if (skyFramework) skyFramework->UpdateSky(*player->GetTime());

	// 驱动Job/Organization各自独立的DailyPlan/ExecNode调度（两套独立的timer，见
	// populace.md/society.md"两套独立timer"一节）。ExecNode这次是纯C++通道，直接给
	// Change*，Populace/Society自己不delete这些指针，所有权留在产出它们的
	// JobMod/OrganizationMod实例身上，这里只读值使用。
	// 开局当天永远不会天然触发一次CrossDay()(时钟是EnsurePlayerGenerated刚设好的，"day"
	// 缓存和当前日期本来就相同)，第一帧强制当成"跨天"，让第一天的调度表也能生成，见.h里
	// bIsFirstTick的说明。
	bool crossedDay = bIsFirstTick || player->CrossDay();
	bIsFirstTick = false;

	// 现场构造一个PostImplement，供Job/OrganizationMod::DailyPlan/ExecNode通过Post()
	// 按需查citizen家/工位的具体地址（"citizen home address"/"citizen workplace
	// address"，见Core/common/implement.cpp）——和UForeverStoryFrameworkComponent::
	// BroadcastGameStart同一个"现场构造、只覆盖这次调用"的用法，不需要长期持有。
	PostImplement postImplement(map, populace, society, story, industry, traffic, player);

	// 按EnsureXxxGenerated()的依赖顺序，六个Core域（Player已经在上面单独Tick过，推进了
	// 全局时钟）挨个调用一遍Tick，哪怕对应域现在什么都不做——保持"每个域都有Tick"这个形状
	// 一致，见各自Tick声明处的占位注释。
	if (map) map->Tick(*player->GetTime(), crossedDay, &postImplement);

	if (populace) {
		populace->Tick(*player->GetTime(), crossedDay,
			[this](Citizen* citizen, const std::vector<Change*>& changes) {
				ScriptContext context;
				if (citizen->GetJob()) context.self = citizen->GetJob()->GetScript();
				context.system = story ? story->GetSystemScript() : nullptr;
				context.global = story ? story->GetGlobalScript() : nullptr;
				for (Change* change : changes) {
					ApplyChange(change, context);
				}
			}, &postImplement);
	}

	if (society) {
		society->Tick(*player->GetTime(), crossedDay,
			[this](Organization* organization, const std::vector<Change*>& changes) {
				// ShopOrganization这次不产出changes，回调体基本用不到，但设施要落地——
				// context.self指向Organization自己的Script（Organization这次一起补上了
				// Script成员，见organization.md"Script配置"一节），和上面Populace::Tick
				// 回调同一个处理方式。
				ScriptContext context;
				if (organization) context.self = organization->GetScript();
				context.system = story ? story->GetSystemScript() : nullptr;
				context.global = story ? story->GetGlobalScript() : nullptr;
				for (Change* change : changes) {
					ApplyChange(change, context);
				}
			}, &postImplement);
	}

	if (industry) industry->Tick(*player->GetTime(), crossedDay, &postImplement);
	if (traffic) traffic->Tick(*player->GetTime(), crossedDay, &postImplement);
	if (story) story->Tick(*player->GetTime(), crossedDay, &postImplement);

	// 到时的计时器给各自的归属脚本发time_up。
	if (storyFramework) storyFramework->FireExpiredTimers();
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

FForeverPendingLoad* AForeverFrameworkActor::FetchActiveLoad()
{
	if (!bActiveLoadFetched) {
		bActiveLoadFetched = true;
		if (UGameInstance* gameInstance = GetGameInstance()) {
			if (UForeverSaveSubsystem* saveSubsystem = gameInstance->GetSubsystem<UForeverSaveSubsystem>()) {
				activeLoad = saveSubsystem->ConsumePendingLoad();
			}
		}
	}
	return activeLoad.Get();
}

void AForeverFrameworkActor::EnsureMapGenerated()
{
	if (map) return;

	// 读档：按存档恢复地图(不跑任何生成)，再把各mod类的命名计数器恢复成存档值——必须在其余域
	// 重新生成之前恢复，它们新建的mod实例才会从存档时的编号接着往下编，不和恢复出来的名字撞。
	if (FForeverPendingLoad* load = FetchActiveLoad()) {
		std::string error;
		map = MapSaveCodec::Load(load->archive, error);
		if (map) {
			UE_LOG(LogTemp, Log, TEXT("AForeverFrameworkActor: %s"), UTF8_TO_TCHAR(MapSaveCodec::GetLastReport().c_str()));
			SaveGame::RestoreInstanceCounters(load->manifest);
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("AForeverFrameworkActor: failed to restore the map from %s (%s); generating a new world instead."),
				*load->path, UTF8_TO_TCHAR(error.c_str()));
			if (GEngine) {
				GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red,
					FString::Printf(TEXT("读档失败，已重新生成世界：%s"), UTF8_TO_TCHAR(error.c_str())));
			}
		}
	}

	if (!map) {
		map = new Map(kDefaultMapWidth, kDefaultMapHeight);
		map->InitTerrains();
		map->InitRoadnet();
		map->InitZones();
		map->InitBuildings();
		// 植被放在最后：要等路网开口/园区/建筑/房间全部生成完，行道树才能避开出入口。
		map->InitVegetation();
	}

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
	// 植被：Core里的树在InitVegetation时已经全部展开好，这里一次性
	// 加载mesh+批量实例化；Z取地形高度，必须在GenerateTerrain之后(用的是同一个Map::SampleHeight)。
	if (vegetationFramework) {
		vegetationFramework->GenerateVegetation(map);
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
	// 读档：按存档恢复市民(连同住处/房产归属)，不跑Init/Checkin。存档里没有人口段(P2阶段的存档)
	// 或者恢复失败时退回新开局生成。
	if (FForeverPendingLoad* load = FetchActiveLoad()) {
		std::string error;
		populace = PopulaceSaveCodec::Load(load->archive, *map, pendingJobExperienceOrganizations, error);
		bPopulaceRestored = populace != nullptr;
		if (populace) {
			UE_LOG(LogTemp, Log, TEXT("AForeverFrameworkActor: populace restored from the save (%d citizens)."),
				static_cast<int32>(populace->GetCitizens().size()));
		}
		else if (!error.empty()) {
			UE_LOG(LogTemp, Error, TEXT("AForeverFrameworkActor: failed to restore populace (%s); generating it instead."), UTF8_TO_TCHAR(error.c_str()));
		}
	}

	if (!populace) {
		populace = new Populace();
		populace->Init(map->ComputeAccommodationTarget());
		map->Checkin(*populace);
	}

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

	// 读档：社会段按人口段的市民下标引用市民，只有人口也是按存档恢复的才能读。
	if (FForeverPendingLoad* load = FetchActiveLoad()) {
		if (bPopulaceRestored) {
			std::string error;
			society = SocietySaveCodec::Load(load->archive, *map, *populace, pendingJobExperienceOrganizations, error);
			if (society) {
				UE_LOG(LogTemp, Log, TEXT("AForeverFrameworkActor: society restored from the save (%d organizations)."),
					static_cast<int32>(society->GetOrganizations().size()));
			}
			else if (!error.empty()) {
				UE_LOG(LogTemp, Error, TEXT("AForeverFrameworkActor: failed to restore society (%s); generating it instead."), UTF8_TO_TCHAR(error.c_str()));
			}
		}
		pendingJobExperienceOrganizations.clear();
	}
	if (society) return;

	// Society这次真的做了组织分配+招聘（不是空骨架了）：按地图里所有Component加权随机
	// 分配Organization（每个Organization自己遍历claimed components设计Job），再把成年
	// 市民随机匹配到还空缺的Job上，见society.md。
	society = new Society();
	society->Init(map->GetAllComponents());
	society->RecruitCitizens(populace->GetCitizens(), populace->GetCurrentYear());
	// 入职历史反推 + 同事关系——依赖RecruitCitizens()已经把当前在职citizen分配进Job，
	// 也依赖Citizen::GetLastGraduationYear()（Populace::Init()里GenerateEducations()
	// 已经算好），见society.md"四类人际关系生成"一节。人口按存档恢复时经历和同事关系里
	// 已经有这些了，不再补一遍。
	if (!bPopulaceRestored) {
		society->GenerateEmploymentHistory(populace->GetCitizens(), populace->GetCurrentYear());
	}
}

void AForeverFrameworkActor::EnsurePlayerGenerated()
{
	if (player) return;

	// 读档：按存档恢复时钟/生命值/身上和房产里的物品/所在房间，并把散落在房间里的物品放回地图。
	if (FForeverPendingLoad* load = FetchActiveLoad()) {
		std::string error;
		player = PlayerSaveCodec::Load(load->archive, *map, error);
		if (player) {
			UE_LOG(LogTemp, Log, TEXT("AForeverFrameworkActor: player restored from the save."));
			return;
		}
		if (!error.empty()) {
			UE_LOG(LogTemp, Error, TEXT("AForeverFrameworkActor: failed to restore player (%s); generating it instead."), UTF8_TO_TCHAR(error.c_str()));
		}
	}

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

	// 主线剧情.script的global_settings.time_flow_ratio字段——没有声明就保持
	// Player::timeFlowRatio的默认值（2.0），见player.md"time_flow_ratio"一节。
	const auto& settings = Script::GetGlobalSettings(Config::GetMainStoryScriptPath());
	if (auto it = settings.find("time_flow_ratio"); it != settings.end()) {
		player->SetTimeFlowRatio(ToDouble(it->second));
	}
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

	// Traffic的Route/Station两个域仍然是空骨架，Vehicle这一份已经落地(见traffic.md)——
	// Init(map)遍历所有停车位房间预置生成车辆(Core层，只是new Vehicle对象，这时候每辆车的
	// options还是空的)。真正的AVehicleElement Actor生成**不能**在这里就做——
	// AVehicleElement::Init()会立刻BuildProximityBox()烘焙一份vehicle->GetOptions()快照，
	// 而"上车"这个选项要等下面EnsureStoryGenerated()里的BroadcastGameStart()广播完车辆自己
	// 的Script(vehicle_basic.script的game_start milestone)之后才会被加进去——如果在这里就
	// SpawnActor，烘焙到的会是一份空选项快照，"靠近车辆弹上车选项"永远不会出现(和
	// ACitizenElement故意等到game_start广播跑完之后才会被距离流式生成是同一个道理，见
	// ACitizenElement::BuildProximityBox的注释)。GenerateVehicles()真正的调用点挪到
	// BeginPlay()里EnsureStoryGenerated()之后，见那边的调用。
	// 读档：按存档恢复站点/线路/车辆；存档带脚本状态段时(会跳过game_start广播)连车辆的交互选项
	// 一起恢复。存档里没有交通段时重新生成。
	if (FForeverPendingLoad* load = FetchActiveLoad()) {
		std::string error;
		bool restoreOptions = load->archive.Find(SAVE_SECTION_SCRIPTS) != nullptr;
		traffic = TrafficSaveCodec::Load(load->archive, *map, restoreOptions, error);
		if (traffic) {
			UE_LOG(LogTemp, Log, TEXT("AForeverFrameworkActor: traffic restored from the save (%d vehicles, %d stations, %d routes)."),
				static_cast<int32>(traffic->GetVehicles().size()), static_cast<int32>(traffic->GetStations().size()),
				static_cast<int32>(traffic->GetRoutes().size()));
			return;
		}
		if (!error.empty()) {
			UE_LOG(LogTemp, Error, TEXT("AForeverFrameworkActor: failed to restore traffic (%s); generating it instead."), UTF8_TO_TCHAR(error.c_str()));
		}
	}

	traffic = new Traffic();
	traffic->Init(map);
}

void AForeverFrameworkActor::EnsureStoryGenerated()
{
	if (story) return;

	// 和map/populace不互相依赖。Init()读取Resource/Story/test.json，
	// 随后立刻广播一次GameStartEvent，见storyFramework.md。
	story = new Story();
	story->Init();
	if (storyFramework) storyFramework->Init(story);

	// 读档：存档带脚本状态段时，按存档恢复全部Script(门/组织/岗位/Scheduler/车辆/主线)的状态，
	// 不再广播game_start——那些game_start里做过的事(加交互选项、随机交给玩家一个市民……)在存档
	// 里已经是结果了，再广播一次会重复。
	if (FForeverPendingLoad* load = FetchActiveLoad()) {
		std::string error;
		bScriptsRestored = ScriptSaveCodec::Load(load->archive, map, populace, society, traffic, story, error);
		if (bScriptsRestored) {
			UE_LOG(LogTemp, Log, TEXT("AForeverFrameworkActor: script states restored from the save, game_start not broadcast."));
			return;
		}
		if (!error.empty()) {
			UE_LOG(LogTemp, Error, TEXT("AForeverFrameworkActor: failed to restore script states (%s); broadcasting game_start instead."), UTF8_TO_TCHAR(error.c_str()));
		}
	}

	if (storyFramework) storyFramework->BroadcastGameStart();
}

void AForeverFrameworkActor::RestoreControlledPawn(const FForeverPendingLoad& load)
{
	const SaveSection* section = load.archive.Find(SAVE_SECTION_UE);
	if (!section) return;
	JsonValue root;
	JsonReader reader;
	std::string text(section->data.begin(), section->data.end());
	if (!reader.Parse(text, root, false)) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverFrameworkActor: the save's ue section is corrupted, player control not restored."));
		return;
	}

	APlayerController* playerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!playerController) return;

	// 先占有市民(开车时是下车后要回到的那个市民)，再上车——和游戏里上车的顺序一样。
	FString citizenName = UTF8_TO_TCHAR(root["controlledCitizen"].AsString().c_str());
	FString vehicleName = UTF8_TO_TCHAR(root["controlledVehicle"].AsString().c_str());
	bool hasPose = !root["pawnX"].IsNull();
	FVector pawnLocation(root["pawnX"].AsDouble(), root["pawnY"].AsDouble(), root["pawnZ"].AsDouble());
	FRotator pawnRotation(0.f, root["pawnYaw"].AsDouble(), 0.f);

	if (!citizenName.IsEmpty() && populaceFramework) {
		if (ACitizenElement* citizen = populaceFramework->FindOrSpawnCitizenByName(citizenName)) {
			// 步行时把市民放回存档时的位置朝向；开车时市民藏在车里，位置以车为准。
			if (hasPose && vehicleName.IsEmpty()) {
				citizen->SetActorLocationAndRotation(pawnLocation, pawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
			}
			playerController->Possess(citizen);
			// PossessedBy会默认激活左肩、换满弹匣，这里改回存档时的武器和弹匣余量。
			if (!root["weaponSlot"].IsNull()) {
				citizen->RestoreWeaponState(root["weaponSlot"].AsInt(), root["magazineAmmo"].IsNull() ? -1 : root["magazineAmmo"].AsInt());
			}
			if (buildingFramework) buildingFramework->RequestFreezeUntilLodSettled();
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("AForeverFrameworkActor: saved controlled citizen %s not found."), *citizenName);
		}
	}
	if (!vehicleName.IsEmpty() && trafficFramework) {
		trafficFramework->ApplyEnterVehicle(vehicleName, playerController);
		if (hasPose) {
			if (APawn* vehicle = playerController->GetPawn()) {
				vehicle->SetActorLocationAndRotation(pawnLocation, pawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
	}
	if (hasPose && !root["controlYaw"].IsNull()) {
		playerController->SetControlRotation(FRotator(root["controlPitch"].AsDouble(), root["controlYaw"].AsDouble(), 0.f));
	}
}
