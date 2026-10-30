#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include <string>
#include <utility>
#include <vector>

#include "ForeverFrameworkActor.generated.h"

struct FForeverPendingLoad;
class JobExperience;

class Map;
class Populace;
class Society;
class Story;
class Industry;
class Traffic;
class Player;
class Change;
class SpawnNpcChange;
class NPCReviveChange;
struct ScriptContext;

class USceneComponent;
class UForeverAssetFrameworkComponent;
class UForeverBuildingFrameworkComponent;
class UForeverPopulaceFrameworkComponent;
class UForeverRoadnetFrameworkComponent;
class UForeverRoomFrameworkComponent;
class UForeverSkyFrameworkComponent;
class UForeverStoryFrameworkComponent;
class UForeverTerrainFrameworkComponent;
class UForeverTrafficFrameworkComponent;
class UForeverVegetationFrameworkComponent;
class UForeverZoneFrameworkComponent;

// 场景里唯一的Framework入口Actor,取代旧工程里分散的9个Framework Actor
// (Asset/Building/Global/Populace/Roadnet/Room/Story/Terrain/Traffic/Zone)。
// 内部按域组件划分职责,详见ForeverFrameworkActor.md。**不含Global域组件**——旧工程里
// `GlobalBase`本身就是那个唯一放在关卡里、串联其它Framework Actor的入口,而这个角色现在由
// `AForeverFrameworkActor`自己承担了,不需要再嵌一个"Global"子组件重复这件事;`GlobalBase`
// 剩下的编排逻辑(GlobalPause/DrawMap/InitPhone等)将来直接落在这个Actor自己身上。
UCLASS()
class FOREVER_API AForeverFrameworkActor : public AActor
{
	GENERATED_BODY()

public:
	AForeverFrameworkActor();

	// 显式声明+在.cpp里定义(那里map/map.h已完整include)——map是原生指针手动管理,
	// Map在这个头文件里只有前置声明。用unique_ptr<Map>试过,UHT为每个UCLASS生成的
	// VTableHelper构造函数(定义在.gen.cpp,看不到map/map.h)会在异常展开路径里引用
	// ~unique_ptr<Map>而编译失败,所以这里用原生指针+显式delete(纯C++类型,不跨模块
	// 边界——Core.lib静态链接进本模块,不是跨DLL new/delete场景)。
	virtual ~AForeverFrameworkActor();

	// 地图域拆成"Core数据 + UE网格"十个步骤，由BeginPlay登记的生成阶段(BuildGenerationStages)按顺序
	// 每帧执行一个，让开局进度条有机会重绘。顺序不能变：先把Core里的数据(Init*)全部算完，再统一
	// 生成UE网格(Roadnet要采样地形/水面，Zone/Building要用到Roadnet产出的Lot)。读档时BuildMapTerrain
	// 从存档恢复整张地图，其余Init*跳过。**不会调用其它阶段**——顺序由BuildGenerationStages显式列出。
	void BuildMapTerrain();   // 新建/恢复Map，InitTerrains
	void BuildMapRoadnet();
	void BuildMapZones();
	void BuildMapBuildings();
	void BuildMapVegetation();
	void GenerateTerrainMesh();
	void GenerateRoadnetMesh();
	void GenerateZoneMesh();
	void GenerateBuildingMesh();
	void GenerateVegetationMesh();

	// 地图中心、地形生成之前用的出生位置(GameMode找出生点时地形还没生成)，见ForeverPlayerGameMode.cpp。
	static FVector GetDefaultSpawnLocation();

	// 幂等(populace已存在直接返回):只保证Populace这一个域——new Populace()、
	// Populace::Init(map->ComputeAccommodationTarget())、map->Checkin(*populace)、交给
	// populaceFramework缓存citizen列表。假定map已经生成好(地图的各个步骤已经跑过,
	// 要用到Map::ComputeAccommodationTarget()/Checkin()),调用方负责保证调用顺序,这个函数
	// 自己不会调地图的步骤。
	void EnsurePopulaceGenerated();

	// 幂等(society已存在直接返回):只保证Society这一个域——new Society()、
	// Society::Init(map->GetAllComponents())按Component加权随机分配Organization(每个
	// Organization自己遍历claimed components设计Job)、
	// Society::RecruitCitizens(populace->GetCitizens(), populace->GetCurrentYear())把
	// 成年市民随机匹配到还空缺的Job上。假定map/populace都已经生成好,调用方负责保证调用
	// 顺序,这个函数自己不会调地图步骤/EnsurePopulaceGenerated(),见
	// society.md。
	void EnsureSocietyGenerated();

	// 幂等(player已存在直接返回):只保证Player这一个域——new Player()、Player::Init()创建
	// 全局时钟、按populace->GetCurrentYear()把开局时间设成那一年1月1日8点(让Citizen的生日/
	// 年龄和这个初始时钟保持自洽,见populace.md"生日换算"一节)。假定populace已经生成好
	// (EnsurePopulaceGenerated()已经跑过,要用到Populace::GetCurrentYear()),调用方负责
	// 保证调用顺序,这个函数自己不会调EnsurePopulaceGenerated()。
	void EnsurePlayerGenerated();

	// 幂等(industry已存在直接返回):只保证Industry这一个域。这次还是空骨架,new出来纯粹是
	// 为了让PostImplement能拿到7个域的真实指针,不是提前实现业务逻辑。
	void EnsureIndustryGenerated();

	// 幂等(traffic已存在直接返回):只保证Traffic这一个域。这次还是空骨架,new出来纯粹是为了
	// 让PostImplement能拿到7个域的真实指针,不是提前实现业务逻辑。
	void EnsureTrafficGenerated();

	// 幂等(story已存在直接返回):只保证Story这一个域——new Story()、Story::Init()读取
	// Resource/Story/test.json,交给storyFramework后立刻广播一次GameStartEvent。和map/
	// populace不互相依赖,见storyFramework.md。
	void EnsureStoryGenerated();

	Map* GetMap() const { return map; }
	Populace* GetPopulace() const { return populace; }
	Society* GetSociety() const { return society; }
	Story* GetStory() const { return story; }
	Industry* GetIndustry() const { return industry; }
	Traffic* GetTraffic() const { return traffic; }
	Player* GetPlayer() const { return player; }

	// 统一的Change消费入口——populace/society两个Tick回调、
	// UForeverStoryFrameworkComponent::BroadcastGameStart的onActions回调，今后任何地方产出的
	// Change都通过这一个函数消费，不再各自手写dynamic_cast dispatch。顺序：先处理这个Actor
	// 自己能直接搞定的类型（NPCNavigateChange/DebugPrintChange/ChangeControlChange——都需要
	// UE层才能拿到的东西，Core域看不到`Room*`/`ACitizenElement`/`GEngine`/
	// `APlayerController`），命中就直接return；余下的类型才转发给
	// map/populace/society/story/industry/traffic六个域各自的ApplyChange，谁认识就处理，
	// 见.cpp实现和ForeverFrameworkActor.md。
	// @change: 待执行的变化
	// @context: 变量路由上下文
	void ApplyChange(const Change* change, const ScriptContext& context);

protected:
	virtual void BeginPlay() override;

	// 校验主线剧情.script的mod_dependences字段声明的每个mod id有没有被任何一个Factory
	// 注册过（见Registry::CheckModRegistered）。有任何一个没注册就THROW_EXCEPTION(
	// RuntimeException, ...)——不在这里自己打日志/退出，统一交给BeginPlay()的try/catch
	// 处理。没有声明mod_dependences（字段不存在/为空）视为直接通过，什么都不做。
	void ValidateMainStoryDependencies();

	// 世界生成阶段：BeginPlay登记，Tick每帧执行一个(RunGenerationStep)，见.cpp里BeginPlay的说明。
	struct FGenerationStage {
		FText label;
		TFunction<void()> run;
	};
	TArray<FGenerationStage> generationStages;
	int32 generationStageIndex = 0;
	bool bStageLabelShown = false;
	bool bGenerationFailed = false;
	int32 generationStartDelayFrames = 2;
	// 地图是从存档恢复的(BuildMapTerrain设置)：这时后面几个Init*步骤跳过。
	bool bMapRestored = false;
	void BuildGenerationStages();
	void RunGenerationStep();

	// 驱动Player的全局时钟(Player::Tick)——目前这个Actor唯一需要每帧更新
	// 的东西，见ForeverFrameworkActor.md"Tick"一节。
	virtual void Tick(float DeltaTime) override;

	// 必须在这里(而不是只靠~AForeverFrameworkActor())同步delete map：PIE停止时Actor只是被
	// 标记PendingKill，真正的UObject垃圾回收(进而触发C++析构函数)时机不确定，可能拖到下一次
	// 按Play之后才发生。map持有的modLoader析构时会FreeLibrary卸载Basic.dll等mod dll——如果
	// 这个FreeLibrary没有在下一次PIE的LoadLibraryA之前跑完，Windows会直接把已经加载在进程里
	// 的旧dll镜像加引用计数返回，而不是重新从磁盘读取刚编译好的新dll，导致mod代码改了、
	// 编译也成功了，但PIE里跑的还是改之前的旧逻辑(排查一次围墙缩进量对不上代码的bug才发现)。
	// EndPlay由引擎在PIE停止时同步调用，在这里delete能保证下一次Play之前dll已经被卸载干净。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 读档：第一次调用时从UForeverSaveSubsystem取走等待中的读档请求(之后一直返回同一份，没有
	// 读档时返回nullptr)。第一次调用(BeginPlay里)触发，见ForeverFrameworkActor.md"读档"一节。
	FForeverPendingLoad* FetchActiveLoad();
	TSharedPtr<FForeverPendingLoad> activeLoad;
	bool bActiveLoadFetched = false;
	// 读档时人口是否按存档恢复了(社会段要用人口段的市民下标，人口重新生成时社会也只能重新生成)，
	// 以及人口段留给社会段去连的(JobExperience, 组织下标)，见PopulaceSaveCodec::Load。
	bool bPopulaceRestored = false;
	std::vector<std::pair<JobExperience*, int>> pendingJobExperienceOrganizations;
	// 读档时脚本状态按存档恢复了：这时不广播game_start(脚本已经在存档时的进度上)，玩家操控谁
	// 也不由game_start决定，改由RestoreControlledPawn按存档的UE段恢复。
	bool bScriptsRestored = false;
	// BeginPlay里整段世界生成流程(新游戏含game_start广播，读档含RestoreControlledPawn)已经走完。
	bool bInitialGenerationDone = false;
	void RestoreControlledPawn(const FForeverPendingLoad& load);

	// add_option/remove_option执行完之后，让场景里叫这个名字的市民/车辆/门刷新交互选项快照。
	void RefreshInteractOptions(const std::string& name);

public:
	// 世界坐标所在的地址文本：房间地址 > 建筑地址 > 园区地址 > 地块(block)地址 > "x,y,z"(地图单位)。
	// 房间/建筑/园区用它们的触发盒做一次小球重叠查询反查，地块用地块矩形做几何包含判断。
	// 死亡事件的local.location用，见CitizenDeceaseEvent。
	std::string DescribeLocation(const FVector& worldLocation) const;

protected:

	// spawn_npc：Core里建市民(Populace::SpawnCitizen)、按home住进房间(Map::SettleCitizen)，再登记进
	// UE的流式生成列表——要用到Populace/Map/PopulaceFramework三方，所以放在UE层分支。
	void ApplySpawnNpc(const SpawnNpcChange* change, const ScriptContext& context);
	// npc_revive：Citizen::Revive，之后需要npc_teleport才会出现在场景里。
	void ApplyReviveNpc(const NPCReviveChange* change, const ScriptContext& context);

	// Map的生命周期归这个Actor持有(纯C++类型,不是UPROPERTY)。
	// Zone/Building/Roadnet等系统共用同一个Map实例,不是各自另建一个。
	Map* map = nullptr;

	// 进入populace域新增：和map平级持有，不是map的成员——Populace不知道Map的存在，
	// 通过Map::Checkin(*populace)单向读取，和老工程GlobalBase同时持有map/populace两个
	// 顶层对象、由它做两者之间编排是同一个分工，详见Source/Core/populace/populace.md。
	Populace* populace = nullptr;

	// 和map/populace平级持有，生命周期管理方式完全一致(EnsureStoryGenerated
	// 里创建、EndPlay/析构函数里delete)，详见Source/Core/story/story.md。
	Story* story = nullptr;

	// Society/Industry/Traffic/Player这四个域这次还没有真正migrate，只是空骨架——新增它们
	// 纯粹是为了让PostImplement（Core/common/implement.h）能拿到7个域的真实指针，不是提前
	// 实现这几个域。生命周期管理方式和map/populace/story完全一致，见各自的.md。
	Society* society = nullptr;
	Industry* industry = nullptr;
	Traffic* traffic = nullptr;
	Player* player = nullptr;

	// Player::CrossDay()只在"日期真的从一天跨到下一天"时才为true——开局当天(EnsurePlayerGenerated
	// 刚把时钟设到当年1月1日8点)永远不会天然触发一次CrossDay，导致第一天的Job/Organization
	// 调度(Populace::Tick/Society::Tick里"crossedDay时生成今天的调度表"那一步)永远排不上，
	// 一个市民永远不会有第一天的日程。这里用一个"第一帧强制当作跨天"的标记补上这个缺口，
	// 效果等价于老工程Populace::Tick里的`currentTime.GetYear() == 0 || player->CrossDay()`
	// 这个bootstrap特判，见Tick()实现。
	bool bIsFirstTick = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<USceneComponent> sceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverAssetFrameworkComponent> assetFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverBuildingFrameworkComponent> buildingFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverPopulaceFrameworkComponent> populaceFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverRoadnetFrameworkComponent> roadnetFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverRoomFrameworkComponent> roomFramework;

	// 昼夜循环，见Source/Forever/Framework/ForeverSkyFrameworkComponent.md。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverSkyFrameworkComponent> skyFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverStoryFrameworkComponent> storyFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverTerrainFrameworkComponent> terrainFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverTrafficFrameworkComponent> trafficFramework;

	// 全图植被(行道树/公园树)统一渲染，见Source/Forever/Framework/ForeverVegetationFrameworkComponent.md。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverVegetationFrameworkComponent> vegetationFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverZoneFrameworkComponent> zoneFramework;

public:
	FORCEINLINE UForeverAssetFrameworkComponent* GetAssetFramework() const { return assetFramework; }
	FORCEINLINE UForeverBuildingFrameworkComponent* GetBuildingFramework() const { return buildingFramework; }

	// BeginPlay的世界生成流程是否已经全部结束(game_start已广播完/读档的玩家与世界物品已恢复)。
	// 开局黑屏(UForeverFadeWidget)用它判断世界是否就绪。
	FORCEINLINE bool IsInitialGenerationDone() const { return bInitialGenerationDone; }

	// 开局进度条用：总格数(生成阶段数+最后一格"构建场景")、已完成格数、当前阶段名。每个阶段完成算一格；
	// 生成完之后"构建场景"(等建筑LOD排空，IsLodSettled)再占最后一格。BeginPlay之前total为0。
	void GetLoadProgress(int32& outDone, int32& outTotal, FText& outLabel) const;
	FORCEINLINE UForeverPopulaceFrameworkComponent* GetPopulaceFramework() const { return populaceFramework; }
	FORCEINLINE UForeverRoadnetFrameworkComponent* GetRoadnetFramework() const { return roadnetFramework; }
	FORCEINLINE UForeverRoomFrameworkComponent* GetRoomFramework() const { return roomFramework; }
	FORCEINLINE UForeverSkyFrameworkComponent* GetSkyFramework() const { return skyFramework; }
	FORCEINLINE UForeverStoryFrameworkComponent* GetStoryFramework() const { return storyFramework; }
	FORCEINLINE UForeverTerrainFrameworkComponent* GetTerrainFramework() const { return terrainFramework; }
	FORCEINLINE UForeverTrafficFrameworkComponent* GetTrafficFramework() const { return trafficFramework; }
	FORCEINLINE UForeverZoneFrameworkComponent* GetZoneFramework() const { return zoneFramework; }
};
