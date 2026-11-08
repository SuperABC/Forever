#include "Framework/ForeverFrameworkActor.h"

#include "map/map.h"
#include "map/room.h"
#include "populace/populace.h"
#include "populace/citizen.h"
#include "society/society.h"
#include "society/organization.h"
#include "society/job.h"
#include "society/settlement.h"
#include "society/economy.h"
#include "society/immigration.h"
#include "story/story.h"
#include "story/change.h"
#include "story/script.h"
#include "industry/industry.h"
#include "traffic/traffic.h"
#include "traffic/route.h"
#include "traffic/station.h"
#include "traffic/vehicle.h"
#include "story/event.h"
#include "player/player.h"
#include "common/registry.h"
#include "common/utility.h"
#include "common/implement.h"
#include "common/config.h"
#include "common/error.h"
#include "common/save.h"
#include "map/map_save.h"
#include "industry/industry_save.h"
#include "populace/populace_save.h"
#include "society/society_save.h"
#include "traffic/traffic_save.h"
#include "story/script_save.h"
#include "player/player_save.h"
#include "Player/ForeverCharacter.h"
#include "common/json.h"
#include "Element/CitizenElement.h"
#include "Element/VehicleElement.h"
#include "Element/TransitVehicleElement.h"
#include "Element/AssetElement.h"
#include "Element/DoorComponent.h"
#include "Framework/ForeverWeatherFrameworkComponent.h"
#include "Framework/ForeverZoneFrameworkComponent.h"
#include "map/building.h"
#include "map/zone.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "Element/BuildingElement.h"
#include "map/cabin.h"
#include "map/door.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"

#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/DefaultPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Player/ForeverPlayerController.h"

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
#include "Framework/ForeverLampFrameworkComponent.h"
#include "Mod/ForeverSaveSubsystem.h"
#include "Framework/ForeverZoneFrameworkComponent.h"
#include "Components/SceneComponent.h"

AForeverFrameworkActor::AForeverFrameworkActor()
{
	PrimaryActorTick.bCanEverTick = true; // 生成阶段驱动 + 全局时钟/各域Tick，见.md"Tick"

	sceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = sceneRoot;
	// 根组件必须Static:否则挂在下面的子组件(哪怕自己设成Static)都会被引擎视为Movable，
	// 成千上万的墙体/碰撞盒堆进动态八叉树和动态broadphase，建筑LOD增删越来越卡，
	// 见ForeverBuildingFrameworkComponent.md"性能"一节。
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
	lampFramework = CreateDefaultSubobject<UForeverLampFrameworkComponent>(TEXT("LampFramework"));
	weatherFramework = CreateDefaultSubobject<UForeverWeatherFrameworkComponent>(TEXT("WeatherFramework"));
	zoneFramework = CreateDefaultSubobject<UForeverZoneFrameworkComponent>(TEXT("ZoneFramework"));
}

AForeverFrameworkActor::~AForeverFrameworkActor()
{
	// 先等异步寻路任务跑完才能delete map/populace(后台线程还持有它们的裸指针)。
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
	// 同步释放各域(原因见头文件EndPlay声明处)。删除顺序互不影响内存安全，按创建的相反顺序只是保持直觉。

	// 先等异步寻路任务跑完;不能用"派发GameThread任务再等"的方式，EndPlay本身在GameThread上会死锁。
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

	UE_LOG(LogTemp, Log, TEXT("AForeverFrameworkActor initialized with its framework components"));

	// 先让玩家Controller初始化好(创建Widget、重置输入模式)：打包版里本BeginPlay先于Controller的
	// BeginPlay，而game_start广播里的start_puzzle/对话要用这些Widget，见
	// AForeverPlayerController::InitializeOnce。
	if (AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))) {
		playerController->InitializeOnce();
	}

	// 读档：各个域在各自的生成步骤里按存档恢复（所有存档段都是必须的）。mod集合已经在RequestLoad里按存档设好了。
	if (FForeverPendingLoad* load = FetchActiveLoad()) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverFrameworkActor: loading %s - the world is restored from the save (sections missing from older saves are regenerated)."),
			*load->path);
	}

	// 世界生成拆成一串阶段由Tick每帧执行一个，阶段顺序即依赖顺序，见.md"分阶段生成"。
	BuildGenerationStages();
}

void AForeverFrameworkActor::BuildGenerationStages()
{
	const bool bLoading = FetchActiveLoad() != nullptr;
	auto add = [this](const TCHAR* label, TFunction<void()> run) {
		FGenerationStage stage;
		stage.label = FText::FromString(label);
		stage.run = MoveTemp(run);
		generationStages.Add(MoveTemp(stage));
	};

	add(TEXT("加载模组"), [this]() {
		// 每局开始按当前config补加载新mod dll并刷新启用表/参数表，见Source/Core/common/registry.md。
		Registry::Get().RefreshMods();
		// 依赖不满足就尽早拒绝生成世界。
		ValidateMainStoryDependencies();
	});
	add(TEXT("构建地形"), [this]() { BuildMapTerrain(); });
	add(TEXT("构建路网"), [this]() { BuildMapRoadnet(); });
	add(TEXT("构建园区"), [this]() { BuildMapZones(); });
	add(TEXT("构建建筑"), [this]() { BuildMapBuildings(); });
	add(TEXT("栽种植被"), [this]() { BuildMapVegetation(); });
	add(TEXT("布置路灯"), [this]() { BuildMapLamps(); });
	// 交通要在地形网格之前构建:instantiate线路(火车)铺轨要请求地形抬升/挖隧道洞(Map::ApplyTerrainFits/AddHatch)。
	add(TEXT("构建交通"), [this]() { EnsureTrafficGenerated(); });
	// Core数据全部算完才统一生成UE网格(Roadnet要采样地形/水面，Zone/Building要用Roadnet产出的Lot)。
	add(TEXT("生成地形模型"), [this]() { GenerateTerrainMesh(); });
	add(TEXT("生成道路模型"), [this]() { GenerateRoadnetMesh(); });
	add(TEXT("生成园区模型"), [this]() { GenerateZoneMesh(); });
	add(TEXT("生成建筑模型"), [this]() { GenerateBuildingMesh(); });
	add(TEXT("生成植被模型"), [this]() { GenerateVegetationMesh(); });
	add(TEXT("生成路灯模型"), [this]() { GenerateLampMesh(); });
	add(TEXT("构建人口"), [this]() { EnsurePopulaceGenerated(); });
	add(TEXT("构建社会与产业"), [this]() {
		EnsureSocietyGenerated();
		EnsurePlayerGenerated();
		EnsureIndustryGenerated();
	});
	add(bLoading ? TEXT("恢复存档") : TEXT("执行开局剧情"), [this]() { EnsureStoryGenerated(); });
	add(bLoading ? TEXT("恢复载具与玩家") : TEXT("构建载具"), [this]() {
		if (trafficFramework) {
			trafficFramework->GenerateVehicles(map, traffic);
			trafficFramework->BuildRouteDebugMesh(traffic);
			trafficFramework->BuildTracks(traffic);
		}

		// 读档跳过了game_start(它本来会随机交给玩家一个市民)：按存档恢复玩家操控的市民/车辆。
		// 必须在GenerateVehicles之后——上车要用到车辆Actor。
		if (bScriptsRestored) {
			if (FForeverPendingLoad* load = FetchActiveLoad()) RestoreControlledPawn(*load);
			// 房间外的物品(被扔出去的投掷物等)：按存档位置朝向重新生成，从静止开始模拟。
			if (assetFramework) assetFramework->RestoreWorldAssets();
		}
	});
}

void AForeverFrameworkActor::RunGenerationStep()
{
	if (bGenerationFailed || bInitialGenerationDone) return;

	// LoadMap刚结束的头几帧先空等：让黑屏和第0格进度先真正渲染出来。
	if (generationStartDelayFrames > 0) {
		generationStartDelayFrames--;
		return;
	}

	// 每个阶段占两帧：第一帧只是让"当前阶段名"(GetLoadProgress)被控件读到并画出来，第二帧才执行
	// 这个阶段——阶段本身会同步卡住主线程，不先画好文字，用户看到的就是上一格的文字卡很久。
	if (!bStageLabelShown) {
		bStageLabelShown = true;
		return;
	}
	bStageLabelShown = false;

	try {
		generationStages[generationStageIndex].run();
	}
	catch (const ExceptionBase& e) {
		bGenerationFailed = true;
		UE_LOG(LogTemp, Error, TEXT("生成世界时发生致命错误，拒绝继续，退出游戏：%s"),
			UTF8_TO_TCHAR(e.GetDetailedInfo().data()));
		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(GetWorld(), 0),
			EQuitPreference::Quit, false);
		return;
	}

	generationStageIndex++;
	if (generationStageIndex >= generationStages.Num()) {
		// 玩家还是过渡用的DefaultPawn(没有任何人把控制权交给市民)时，把它从生成前的临时位置挪到
		// 地形上方的地图中心。
		if (APlayerController* playerController = UGameplayStatics::GetPlayerController(GetWorld(), 0)) {
			if (ADefaultPawn* pawn = Cast<ADefaultPawn>(playerController->GetPawn())) {
				if (terrainFramework && terrainFramework->IsTerrainGenerated()) {
					pawn->SetActorLocation(terrainFramework->GetMapCenterWorldLocation());
				}
			}
		}
		bInitialGenerationDone = true;
	}
}

void AForeverFrameworkActor::GetLoadProgress(int32& outDone, int32& outTotal, FText& outLabel) const
{
	// 阶段还没登记(BeginPlay之前)返回total=0。
	const int32 stageCount = generationStages.Num();
	outTotal = stageCount > 0 ? stageCount + 1 : 0;
	if (stageCount == 0) {
		outDone = 0;
		outLabel = FText::GetEmpty();
		return;
	}
	if (!bInitialGenerationDone) {
		outDone = generationStageIndex;
		outLabel = generationStages[FMath::Min(generationStageIndex, stageCount - 1)].label;
		return;
	}
	const bool bSettled = buildingFramework == nullptr || buildingFramework->IsLodSettled();
	outDone = bSettled ? outTotal : stageCount;
	outLabel = FText::FromString(TEXT("构建场景"));
}

void AForeverFrameworkActor::ValidateMainStoryDependencies() {
	// 优先用Story screen里玩家添加的脚本列表,为空才退回单路径(编辑器直接PIE不经过菜单)。
	// 这是兜底二次校验,更早的校验在ForeverConfigBridgeSubsystem::ValidateAndStartGame。
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
	// 所有Change自己的condition：非空且求值为假就不执行。for_range的子change、分支选项的changes、
	// 里程碑的changes都走这个入口，所以这里统一判断一次。
	if (change && !change->GetCondition().empty() && !EvaluateExpressionBool(change->GetCondition(), context)) return;

	if (auto* nav = dynamic_cast<const NPCNavigateChange*>(change)) {
		// 按change自带的姓名/房间地址反查(NPCNavigateChange::destination是纯std::string，见change.h)。
		// mod生成的是现成的姓名/地址;脚本里写的可能是表达式($$self.name)——先按原样查，查不到再求值后查。
		Citizen* citizen = populace ? populace->FindCitizenByName(nav->GetName()) : nullptr;
		if (!citizen && populace) citizen = populace->FindCitizenByName(ToString(EvaluateExpression(nav->GetName(), context)));
		Room* dest = map ? map->LocateRoom(nav->GetDestination()) : nullptr;
		if (!dest && map) dest = map->LocateRoom(ToString(EvaluateExpression(nav->GetDestination(), context)));
		if (citizen && dest && populaceFramework) {
			populaceFramework->RequestWalk(citizen, dest);
		}
		return;
	}

	if (auto* globalSetting = dynamic_cast<const GlobalSettingChange*>(change)) {
		// 目前只认time_flow_ratio(游戏时钟流速，1.0=1真实秒1游戏分钟)，和主线脚本global_settings同名。
		const std::string setting = globalSetting->GetSetting();
		if (setting == "time_flow_ratio") {
			if (player) player->SetTimeFlowRatio(ToDouble(EvaluateExpression(globalSetting->GetValue(), context)));
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("global_setting: unknown setting \"%s\"."), UTF8_TO_TCHAR(setting.c_str()));
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
		// 操控权切换逻辑在UForeverStoryFrameworkComponent::ApplyControlChange里。
		if (storyFramework) storyFramework->ApplyControlChange(controlChange, context);
		return;
	}

	if (auto* startPuzzle = dynamic_cast<const StartPuzzleChange*>(change)) {
		// Core不知道UMG Widget，见UForeverStoryFrameworkComponent::ApplyStartPuzzle。
		if (storyFramework) storyFramework->ApplyStartPuzzle(startPuzzle, context);
		return;
	}

	if (dynamic_cast<const OpenShopChange*>(change)) {
		// 弹商店界面也是UMG，同start_puzzle只能转发给UE层。
		if (storyFramework) storyFramework->ApplyOpenShop(context);
		return;
	}

	if (auto* payWage = dynamic_cast<const PayWageChange*>(change)) {
		if (storyFramework) storyFramework->ApplyPayWage(payWage, context);
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

	if (auto* teleport = dynamic_cast<const NPCTeleportChange*>(change)) {
		// name/destination都是表达式；destination是房间地址，和npc_navigate同一种写法。
		std::string name = ToString(EvaluateExpression(teleport->GetName(), context));
		std::string destination = ToString(EvaluateExpression(teleport->GetDestination(), context));
		Citizen* citizen = populace ? populace->FindCitizenByName(name) : nullptr;
		Room* dest = map ? map->LocateRoom(destination) : nullptr;
		// destination写"home"=这个市民的家(复活后回家用)。
		if (!dest && citizen && destination == "home") dest = citizen->GetRoom();
		if (!citizen || !dest) {
			UE_LOG(LogTemp, Warning, TEXT("npc_teleport: %s not found (name=\"%s\", destination=\"%s\")."),
				citizen ? TEXT("destination room") : TEXT("citizen"), UTF8_TO_TCHAR(name.c_str()), UTF8_TO_TCHAR(destination.c_str()));
			return;
		}
		if (populaceFramework) populaceFramework->TeleportCitizen(citizen, dest);
		return;
	}

	// 背包相关Change：需要场景里的Actor和事件分发，交给资产框架组件。
	if (auto* pickAsset = dynamic_cast<const PickAssetChange*>(change)) {
		if (assetFramework) assetFramework->ApplyPickAsset(pickAsset, context);
		return;
	}
	if (auto* grantAsset = dynamic_cast<const GrantAssetChange*>(change)) {
		if (assetFramework) assetFramework->ApplyGrantAsset(grantAsset, context);
		return;
	}
	if (auto* deductAsset = dynamic_cast<const DeductAssetChange*>(change)) {
		if (assetFramework) assetFramework->ApplyDeductAsset(deductAsset, context);
		return;
	}
	if (auto* dropAsset = dynamic_cast<const DropAssetChange*>(change)) {
		if (assetFramework) assetFramework->ApplyDropAsset(dropAsset, context);
		return;
	}

	if (auto* reviveNpc = dynamic_cast<const NPCReviveChange*>(change)) {
		ApplyReviveNpc(reviveNpc, context);
		return;
	}

	if (auto* spawnNpc = dynamic_cast<const SpawnNpcChange*>(change)) {
		ApplySpawnNpc(spawnNpc, context);
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

	if (auto* testNavigateVehicle = dynamic_cast<const TestNavigateVehicleChange*>(change)) {
		// 显示这辆车当前路线的折线，不给车发任何命令；名字可能是$$self.name之类的表达式。
		FString vehicleName = UTF8_TO_TCHAR(ToString(EvaluateExpression(testNavigateVehicle->GetName(), context)).data());
		if (trafficFramework) trafficFramework->StartTestNavigation(vehicleName);
		return;
	}

	if (auto* vehicleCommand = dynamic_cast<const VehicleCommandChange*>(change)) {
		FString vehicleName = UTF8_TO_TCHAR(ToString(EvaluateExpression(vehicleCommand->GetVehicle(), context)).data());
		const std::string& command = vehicleCommand->GetCommand();
		if (!trafficFramework) return;
		if (command == "run") trafficFramework->StartBusTestRun(vehicleName);
		else if (command == "stop") trafficFramework->StopBusRun(vehicleName);
		else UE_LOG(LogTemp, Warning, TEXT("vehicle_command: unknown command %s"), UTF8_TO_TCHAR(command.c_str()));
		return;
	}

	if (auto* testNavigateCitizen = dynamic_cast<const TestNavigateCitizenChange*>(change)) {
		// 市民的test_navigate_citizen现在是"显示这个市民当前路线的折线"（"你在去哪"选项）。
		FString citizenName = UTF8_TO_TCHAR(ToString(EvaluateExpression(testNavigateCitizen->GetName(), context)).data());
		if (populaceFramework) populaceFramework->StartTestNavigation(citizenName);
		return;
	}

	if (auto* enterVehicle = dynamic_cast<const EnterVehicleChange*>(change)) {
		// Core不认识Actor/Controller，见UForeverStoryFrameworkComponent::ApplyEnterVehicle。
		if (storyFramework) storyFramework->ApplyEnterVehicle(enterVehicle, context);
		return;
	}

	// 搭乘/下车/强制下车同理，见UForeverStoryFrameworkComponent::ApplyBoardVehicle等。
	if (auto* boardVehicle = dynamic_cast<const BoardVehicleChange*>(change)) {
		if (storyFramework) storyFramework->ApplyBoardVehicle(boardVehicle, context);
		return;
	}
	if (auto* leaveVehicle = dynamic_cast<const LeaveVehicleChange*>(change)) {
		if (storyFramework) storyFramework->ApplyLeaveVehicle(leaveVehicle, context);
		return;
	}
	if (auto* forceLeave = dynamic_cast<const ForceLeaveChange*>(change)) {
		if (storyFramework) storyFramework->ApplyForceLeave(forceLeave, context);
		return;
	}

	// 转发给七个域，谁认识就处理(同一种Change可以有多个域认识，如add_option)。都不认识才打"未实现"警告
	// (用UE_LOG，进Forever.log;Core的debugf只走OutputDebugString)。
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

void AForeverFrameworkActor::ApplyReviveNpc(const NPCReviveChange* change, const ScriptContext& context) {
	if (!populace || !change) return;
	std::string name = ToString(EvaluateExpression(change->GetName(), context));
	Citizen* citizen = populace->FindCitizenByName(name);
	if (!citizen) {
		UE_LOG(LogTemp, Warning, TEXT("npc_revive: citizen \"%s\" not found."), UTF8_TO_TCHAR(name.c_str()));
		return;
	}
	if (!citizen->IsDead()) {
		UE_LOG(LogTemp, Warning, TEXT("npc_revive: citizen \"%s\" is not dead, skipped."), UTF8_TO_TCHAR(name.c_str()));
		return;
	}
	float health = -1.f;
	if (!change->GetHealth().empty()) {
		health = static_cast<float>(ToDouble(EvaluateExpression(change->GetHealth(), context)));
	}
	// 复活后没有房间也没有位置：要等npc_teleport(destination可写"home")把他放进某个房间才会出现。
	citizen->Revive(health);
}

Citizen* AForeverFrameworkActor::GetControlledCitizen() const {
	const ACitizenElement* element = Cast<ACitizenElement>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	return element ? element->GetCitizen() : nullptr;
}

void AForeverFrameworkActor::ApplySpawnNpc(const SpawnNpcChange* change, const ScriptContext& context) {
	if (!populace || !change) return;
	std::string name = ToString(EvaluateExpression(change->GetName(), context));
	std::string genderText = ToString(EvaluateExpression(change->GetGender(), context));
	std::string birthdayText = ToString(EvaluateExpression(change->GetBirthday(), context));

	if (populace->FindCitizenByName(name)) {
		UE_LOG(LogTemp, Warning, TEXT("spawn_npc: citizen \"%s\" already exists, skipped."), UTF8_TO_TCHAR(name.c_str()));
		return;
	}

	// 性别：male/男是男性，female/女是女性，其它值按女性处理并警告(老工程是"不是male就是女性")。
	GENDER_TYPE gender = GENDER_FEMALE;
	if (genderText == "male" || genderText == "男") gender = GENDER_MALE;
	else if (genderText != "female" && genderText != "女") {
		UE_LOG(LogTemp, Warning, TEXT("spawn_npc \"%s\": unknown gender \"%s\", treated as female."),
			UTF8_TO_TCHAR(name.c_str()), UTF8_TO_TCHAR(genderText.c_str()));
	}

	// 生日：Time能识别的带日期写法(如"2000-08-31"、"2000年8月31日")。
	Time birthday;
	try {
		birthday = Time(birthdayText);
	}
	catch (const ExceptionBase&) {}
	if (!birthday.IsValid()) {
		UE_LOG(LogTemp, Warning, TEXT("spawn_npc \"%s\": cannot parse birthday \"%s\", skipped."),
			UTF8_TO_TCHAR(name.c_str()), UTF8_TO_TCHAR(birthdayText.c_str()));
		return;
	}

	std::string schedulerId = ToString(EvaluateExpression(change->GetScheduler(), context));
	// 头像名：Resource/Avatars/下不带扩展名的相对路径，空=自动(随机)头像，见Citizen::GetAvatar。
	std::string avatar = ToString(EvaluateExpression(change->GetAvatar(), context));
	Citizen* citizen = populace->SpawnCitizen(name, gender, birthday.GetYear(), birthday.GetMonth(), birthday.GetDay(), schedulerId, avatar);
	if (!citizen) return;

	// economy：是否接入经济系统(空=false)。接入才分配住处并求职；不接入则不分配住处、不产生
	// 每日需求、不参与招聘/交税/补助。钱包不论接不接入都开，这样之后被玩家操控时直接有钱用。
	const std::string economyText = change->GetEconomy();
	const bool economy = !economyText.empty() && EvaluateExpressionBool(economyText, context);
	citizen->SetInEconomy(economy);
	if (society) {
		int64_t depositCents = -1;
		const std::string depositText = ToString(EvaluateExpression(change->GetDeposit(), context));
		if (!depositText.empty()) {
			try { depositCents = static_cast<int64_t>(std::llround(std::stod(depositText) * 100.0)); }
			catch (const std::exception&) {}
		}
		Settlement::InitFunds(*society, citizen, depositCents);
	}

	// home是房间地址(和npc_navigate的destination同一种写法)，省略时没有住处也没有位置，要等
	// npc_teleport之类把他放到某个房间才会在场景里出现。economy:false时只把当前位置放在这个房间，
	// 不住进去(不进household)。
	std::string homeText = ToString(EvaluateExpression(change->GetHome(), context));
	if (!homeText.empty() && map) {
		if (Room* home = map->LocateRoom(homeText)) {
			if (economy) map->SettleCitizen(citizen, home);
			else citizen->SetCurrentRoom(home);
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("spawn_npc \"%s\": home room \"%s\" not found."),
				UTF8_TO_TCHAR(name.c_str()), UTF8_TO_TCHAR(homeText.c_str()));
		}
	}

	if (economy && society) society->RecruitCitizen(citizen, populace->GetCurrentYear());

	if (populaceFramework) populaceFramework->AddCitizen(citizen);
}

std::string AForeverFrameworkActor::DescribeLocation(const FVector& worldLocation) const {
	// 地图单位=1000个UE单位，和各Framework组件的WORLD_SCALE同一个值。
	constexpr float kWorldScale = 1000.f;

	// 房间/建筑/园区：用它们已经建好的触发盒反查——小球重叠查询，Trigger档案对所有通道都是Overlap。
	Room* room = nullptr;
	Building* building = nullptr;
	const std::string* zoneName = nullptr;
	TArray<FOverlapResult> overlaps;
	if (GetWorld()->OverlapMultiByChannel(overlaps, worldLocation, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(20.f))) {
		for (const FOverlapResult& overlap : overlaps) {
			UPrimitiveComponent* component = overlap.GetComponent();
			if (!component) continue;
			if (ABuildingElement* buildingElement = Cast<ABuildingElement>(component->GetOwner())) {
				if (!room) room = buildingElement->FindRoomOfBox(component);
				if (!building) building = buildingElement->FindBuildingOfBox(component);
			}
			else if (zoneFramework && !zoneName) {
				zoneName = zoneFramework->FindZoneNameOfBox(component);
			}
		}
	}
	if (room) {
		std::string address = room->GetAddress();
		if (!address.empty()) return address;
	}
	if (building) {
		std::string address = building->GetAddress();
		if (!address.empty()) return address;
	}
	if (zoneName && map) {
		if (Zone* zone = map->GetZone(*zoneName)) {
			std::string address = zone->GetAddress();
			if (!address.empty()) return address;
		}
	}

	// 地块(block=顶层Lot)：点是否落在地块的旋转矩形里(Quad的位置是中心，旋转是弧度)。
	if (map) {
		float x = worldLocation.X / kWorldScale, y = worldLocation.Y / kWorldScale;
		for (Lot* lot : map->GetLots()) {
			if (!lot) continue;
			float dx = x - lot->GetPosX(), dy = y - lot->GetPosY();
			float c = FMath::Cos(lot->GetRotation()), s = FMath::Sin(lot->GetRotation());
			float localX = dx * c + dy * s, localY = -dx * s + dy * c;
			if (FMath::Abs(localX) <= lot->GetSizeX() * 0.5f && FMath::Abs(localY) <= lot->GetSizeY() * 0.5f) {
				std::string address = lot->GetAddress();
				if (!address.empty()) return address;
				break;
			}
		}
	}

	FString coordinates = FString::Printf(TEXT("%.2f,%.2f,%.2f"),
		worldLocation.X / kWorldScale, worldLocation.Y / kWorldScale, worldLocation.Z / kWorldScale);
	return TCHAR_TO_UTF8(*coordinates);
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
		if (ATransitVehicleElement* element = trafficFramework->FindTransitElement(UTF8_TO_TCHAR(name.c_str()))) element->RefreshOptions();
	}
	if (assetFramework) {
		if (AAssetElement* element = assetFramework->FindElementByName(UTF8_TO_TCHAR(name.c_str()))) element->RefreshOptions();
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

	// 世界还在一阶段一阶段地生成：驱动生成，且不推进游戏时钟/调度——此时domain对象只有一部分存在，
	// 开局当天的"第一帧强制跨天"也必须等story建好之后才能触发。
	if (!bInitialGenerationDone) {
		RunGenerationStep();
		return;
	}

	// 防御性判空。
	if (!player) return;
	player->Tick(DeltaTime);
	// 时钟刚推进过：同步写进系统变量system.time.*，见UpdateSystemTime。
	if (storyFramework) storyFramework->UpdateSystemTime();

	// 固定key(不是-1)让每帧原地刷新同一行;显示1秒避免偶尔跳帧闪烁。
	if (GEngine) {
		GEngine->AddOnScreenDebugMessage(/*Key=*/9000, /*TimeToDisplay=*/1.f, FColor::White,
			FString::Printf(TEXT("游戏时间: %s"), UTF8_TO_TCHAR(player->GetTime()->ToString(true, true).c_str())));
	}

	if (skyFramework) skyFramework->UpdateSky(*player->GetTime());

	// 天气:先把"变了"广播成weather_change事件，再翻译成表现。必须在UpdateWind之前，风的倍数由它写入。
	if (player->GetWeather().ConsumeChanged() && storyFramework) {
		storyFramework->BroadcastWeatherChange(player->GetWeather().GetName());
	}
	if (weatherFramework) weatherFramework->UpdateWeather(DeltaTime, player->GetWeather(), player->GetCurrentRoom() != nullptr);
	if (vegetationFramework) vegetationFramework->UpdateWind(DeltaTime);
	// 路灯昼夜开关：只在亮度目标变化时才推给各盏灯。
	if (lampFramework) lampFramework->UpdateLamps(*player->GetTime(), DeltaTime);

	// Job/Organization各自独立的DailyPlan/ExecNode调度(见populace.md/society.md"两套独立timer")。
	// Change*的所有权在产出它们的JobMod/OrganizationMod上，这里只读使用。
	// 开局当天第一帧强制当成跨天，见.h里bIsFirstTick。
	bool crossedDay = bIsFirstTick || player->CrossDay();
	bIsFirstTick = false;

	// 现场构造、只覆盖这一帧的PostImplement(Job/OrganizationMod通过Post()查地址，见Core/common/implement.cpp)。
	PostImplement postImplement(map, populace, society, story, industry, traffic, player);

	// 各Core域挨个Tick(Player已在上面推进了全局时钟)。
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
				ScriptContext context;
				if (organization) context.self = organization->GetScript();
				context.system = story ? story->GetSystemScript() : nullptr;
				context.global = story ? story->GetGlobalScript() : nullptr;
				for (Change* change : changes) {
					ApplyChange(change, context);
				}
			}, &postImplement);
	}

	if (industry && society) Economy::SyncIndustry(*society, *industry); // 倒闭企业的房间停业
	if (industry) industry->Tick(*player->GetTime(), crossedDay, &postImplement);
	if (industry && society && map && populace && crossedDay) {
		// 每日经济驱动链：货款、服务买卖、房租、工资、补助，月初做月度分红，见Core/society/economy.md。
		// 全系统资金总和必须始终为0。
		std::vector<Room*> allRooms;
		for (const auto& [name, building] : map->GetBuildings()) {
			if (!building) continue;
			for (Room* room : building->GetRooms()) allRooms.push_back(room);
		}
		DailyReport economyReport = Economy::Daily(*society, *industry, allRooms, populace->GetCitizens(),
			*player->GetTime(), &postImplement, populace->GetCurrentYear(), GetControlledCitizen());
		// 本月整户离城的市民：移除他们在场景里的Actor。
		if (populaceFramework) {
			for (Citizen* departed : economyReport.departed) populaceFramework->HandleCitizenDeath(departed);
		}
		// 城外迁入的新市民：登记给人口组件，之后按距离流式生成Actor。
		for (Citizen* immigrant : Immigration::Daily(*society, *populace, *map, populace->GetCurrentYear())) {
			if (populaceFramework) populaceFramework->AddCitizen(immigrant);
		}
		if (!economyReport.departed.empty()) {
			UE_LOG(LogTemp, Log, TEXT("AForeverFrameworkActor: %d citizens left the city."), static_cast<int32>(economyReport.departed.size()));
		}
		if (society->GetLedger().Total() != 0) {
			UE_LOG(LogTemp, Error, TEXT("AForeverFrameworkActor: ledger total is %lld, expected 0."),
				static_cast<int64>(society->GetLedger().Total()));
		}
	}
	if (traffic) {
		traffic->Tick(*player->GetTime(), crossedDay, &postImplement);

		// 公交停稳/离站：Core只产出事件队列，转发给载具自己的script(脚本据此控制上下车选项)。
		if (storyFramework) {
			for (const Route::StopEvent& stopEvent : traffic->ConsumeStopEvents()) {
				if (!stopEvent.vehicle || !stopEvent.station) continue;
				if (stopEvent.entering) {
					EnterStopEvent event(stopEvent.station->GetName());
					storyFramework->BroadcastToVehicle(event, stopEvent.vehicle);
				}
				else {
					LeaveStopEvent event(stopEvent.station->GetName());
					storyFramework->BroadcastToVehicle(event, stopEvent.vehicle);
				}
			}
		}
	}
	if (story) story->Tick(*player->GetTime(), crossedDay, &postImplement);

	// 到时的计时器给各自的归属脚本发time_up。
	if (storyFramework) storyFramework->FireExpiredTimers(DeltaTime);
}

namespace {
		// 默认地图尺寸(地图元素,1元素=10m)，必须是2的整数次幂(LOD网格最粗一级跨度要正好等于边长)，
		// 且width*height/(512*512)必须大于1才会生成山，详见Source/Basic/map/terrain_basic.md。
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

void AForeverFrameworkActor::BuildMapTerrain()
{
	if (map) return;

	// 读档：按存档恢复地图(不跑任何生成)，再把各mod类的命名计数器恢复成存档值——必须在其余域
	// 重新生成之前恢复，它们新建的mod实例才会从存档时的编号接着往下编，不和恢复出来的名字撞。
	if (FForeverPendingLoad* load = FetchActiveLoad()) {
		std::string error;
		map = MapSaveCodec::Load(load->archive, error);
		if (map) {
			bMapRestored = true;
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
	}
}

	// 读档时由存档恢复，Init*跳过。
void AForeverFrameworkActor::BuildMapRoadnet()
{
	if (!bMapRestored) map->InitRoadnet();
}

void AForeverFrameworkActor::BuildMapZones()
{
	if (!bMapRestored) map->InitZones();
}

void AForeverFrameworkActor::BuildMapBuildings()
{
	if (!bMapRestored) map->InitBuildings();
}

void AForeverFrameworkActor::BuildMapVegetation()
{
		// 要等路网开口/园区/建筑/房间生成完，行道树才能避开出入口。
	if (!bMapRestored) map->InitVegetation();
}

void AForeverFrameworkActor::BuildMapLamps()
{
		// 同样要等开口齐全。
	if (!bMapRestored) map->InitLamps();
}

void AForeverFrameworkActor::GenerateTerrainMesh()
{
	if (terrainFramework) terrainFramework->GenerateTerrain(map);
}

void AForeverFrameworkActor::GenerateRoadnetMesh()
{
		// 必须在InitZones/InitBuildings之后:它们新增的小路(Map::GetPathRoads())要一起画，
		// 见ForeverRoadnetFrameworkComponent.md"小路可视化"。
	if (roadnetFramework) roadnetFramework->GenerateRoadnet(map);
}

void AForeverFrameworkActor::GenerateZoneMesh()
{
	if (zoneFramework) zoneFramework->GenerateZones(map);
}

void AForeverFrameworkActor::GenerateBuildingMesh()
{
		// Room碰撞盒由ABuildingElement自己生成，不需要再调roomFramework，见Source/Forever/Element/BuildingElement.md。
	if (buildingFramework) buildingFramework->GenerateBuildings(map);
}

void AForeverFrameworkActor::GenerateVegetationMesh()
{
		// Z取地形高度(Map::SampleHeight)，必须在地形生成之后。
	if (vegetationFramework) vegetationFramework->GenerateVegetation(map);
}

void AForeverFrameworkActor::GenerateLampMesh()
{
		// Z取地形高度，必须在地形生成之后。
	if (lampFramework) lampFramework->GenerateLamps(map);
}

FVector AForeverFrameworkActor::GetDefaultSpawnLocation()
{
	// 地图正中心;高度只是过渡值(地形还没生成)，玩家真正位置由game_start/RestoreControlledPawn决定。
	return FVector(kDefaultMapWidth * 500.f, kDefaultMapHeight * 500.f, 200.f);
}

void AForeverFrameworkActor::EnsurePopulaceGenerated()
{
	if (populace) return;

		// Map::ComputeAccommodationTarget喂给Populace::Init，结果再喂给Map::Checkin，见Source/Core/populace/populace.md。
		// 读档：按存档恢复市民(连同住处/房产归属)，不跑Init/Checkin。
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
		int accommodation = map->ComputeAccommodationTarget();
		populace->Init(accommodation, map->ComputeWorkCapacity());
		map->Checkin(*populace);
		// Init内部还会对accommodation做随机扰动、模拟结束后丢弃约5%，所以最终人数和它不会完全相等。
		UE_LOG(LogTemp, Log, TEXT("AForeverFrameworkActor: populace generated, accommodation target %d, %d citizens."),
			accommodation, static_cast<int32>(populace->GetCitizens().size()));
	}

	if (populaceFramework) {
			// 不SpawnActor，按玩家距离流式生成由populaceFramework的TickComponent做。
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

		// 按Component加权分配Organization，再把成年市民匹配到空缺Job，见society.md。
	society = new Society();
	society->Init(map->GetAllComponents());
	society->RecruitCitizens(populace->GetCitizens(), populace->GetCurrentYear());
	// 入职历史反推+同事关系依赖RecruitCitizens和毕业年份，见society.md。人口按存档恢复时已经有了，不补。
	if (!bPopulaceRestored) {
		society->GenerateEmploymentHistory(populace->GetCitizens(), populace->GetCurrentYear());
	}

	// 新开局:选股东并由城市外账户注资，见Core/society/settlement.md。
	society->AssignShareholders(populace->GetCitizens(), populace->GetCurrentYear());
	Settlement::InitFunds(*society, populace->GetCitizens());
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

	player = new Player();
	player->Init();
	// 开局时间=populace当前年份1月1日8点，让Citizen生日/年龄与时钟自洽，见populace.md"生日换算"。
	player->SetTime(Time(populace->GetCurrentYear(), 1, 1, 8));

	// 主线脚本global_settings.time_flow_ratio覆盖默认值，多个文件按顺序后者覆盖前者，见player.md。
	for (const std::string& path : Config::GetMainStoryScriptPaths()) {
		const auto& settings = Script::GetGlobalSettings(path);
		if (auto it = settings.find("time_flow_ratio"); it != settings.end()) {
			player->SetTimeFlowRatio(ToDouble(it->second));
		}
	}
}

void AForeverFrameworkActor::EnsureIndustryGenerated()
{
	if (industry) return;

	// 读档：Industry按地图重新生成仓库/工坊，再按存档放回库存、批发价和在制批次。
	if (FForeverPendingLoad* load = FetchActiveLoad()) {
		std::string error;
		industry = IndustrySaveCodec::Load(load->archive, *map, error);
		if (industry) {
			UE_LOG(LogTemp, Log, TEXT("AForeverFrameworkActor: industry restored from the save."));
			return;
		}
		if (!error.empty()) {
			UE_LOG(LogTemp, Error, TEXT("AForeverFrameworkActor: failed to restore industry (%s); generating it instead."), UTF8_TO_TCHAR(error.c_str()));
		}
	}

	// 新开局：Industry按地图上的Room生成Storage/Manufacture(Room::StorageType()/ManufactureTypes())，
	// 每个工坊的产出cache放半天产量。
	industry = new Industry();
	industry->Init(map->GetAllComponents());
	industry->SeedInitialStock();
}

void AForeverFrameworkActor::EnsureTrafficGenerated()
{
	if (traffic) return;

	// 这里只建Core对象。AVehicleElement::Init会立刻烘焙vehicle->GetOptions()快照，而"上车"选项要等
	// game_start广播后才加上，所以GenerateVehicles放在EnsureStoryGenerated之后的"构建载具"阶段
	// (和ACitizenElement等game_start后才流式生成同理)。
	// 读档：按存档恢复站点/线路/车辆。
	if (FForeverPendingLoad* load = FetchActiveLoad()) {
		std::string error;
		traffic = TrafficSaveCodec::Load(load->archive, *map, error);
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

	story = new Story();
	story->Init();
	if (storyFramework) storyFramework->Init(story);

	// 读档：存档带脚本状态段时，按存档恢复全部Script(门/组织/岗位/Scheduler/车辆/主线)的状态，
	// 不再广播game_start——那些game_start里做过的事(加交互选项、随机交给玩家一个市民……)在存档
	// 里已经是结果了，再广播一次会重复。
	if (FForeverPendingLoad* load = FetchActiveLoad()) {
		std::string error;
		bScriptsRestored = ScriptSaveCodec::Load(load->archive, map, populace, society, traffic, story, error, player);
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
	if (!section) {
		UE_LOG(LogTemp, Error, TEXT("AForeverFrameworkActor: the save has no ue section."));
		return;
	}
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
		// 读档恢复不发enter_vehicle事件(script变量/选项已随存档恢复)。线路公交(搭乘)的位置由Core
		// 的Route算，不能用存档里的位姿覆盖。
		if (trafficFramework->FindTransitElement(vehicleName) || trafficFramework->IsSwitchableRouteBus(vehicleName)) {
			trafficFramework->ApplyBoardVehicle(vehicleName, playerController, /*bSilent=*/true);
		}
		else {
			trafficFramework->ApplyEnterVehicle(vehicleName, playerController, /*bSilent=*/true);
			if (hasPose) {
				if (APawn* vehicle = playerController->GetPawn()) {
					vehicle->SetActorLocationAndRotation(pawnLocation, pawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
				}
			}
		}
	}
	if (hasPose && !root["controlYaw"].IsNull()) {
		playerController->SetControlRotation(FRotator(root["controlPitch"].AsDouble(), root["controlYaw"].AsDouble(), 0.f));
	}
}
