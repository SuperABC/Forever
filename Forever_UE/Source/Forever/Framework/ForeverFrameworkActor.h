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
class Citizen;
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
class UForeverLampFrameworkComponent;
class UForeverWeatherFrameworkComponent;
class UForeverZoneFrameworkComponent;

// 场景里唯一的Framework入口Actor,持有Core的7个顶层域对象,按域组件划分UE层职责,
// 负责世界分阶段生成、每帧Tick编排和Change分发。详见ForeverFrameworkActor.md。
UCLASS()
class FOREVER_API AForeverFrameworkActor : public AActor
{
	GENERATED_BODY()

public:
	AForeverFrameworkActor();

	// 显式声明+在.cpp里定义(那里各域头文件已完整include)。域对象用原生指针+显式delete,不能用
	// unique_ptr:UHT生成的VTableHelper构造函数看不到完整类型,会因~unique_ptr编译失败。
	virtual ~AForeverFrameworkActor();

	// 地图域拆成"Core数据 + UE网格"各步骤，由BuildGenerationStages登记、按顺序每帧执行一个。
	// 顺序不能变，各步骤自己不会调用其它步骤。读档时BuildMapTerrain恢复整张地图，其余Build*跳过。
	void BuildMapTerrain();   // 新建/恢复Map，InitTerrains
	void BuildMapRoadnet();
	void BuildMapZones();
	void BuildMapBuildings();
	void BuildMapVegetation();
	void BuildMapLamps();
	void GenerateTerrainMesh();
	void GenerateRoadnetMesh();
	void GenerateZoneMesh();
	void GenerateBuildingMesh();
	void GenerateVegetationMesh();
	void GenerateLampMesh();

	// 地图中心、地形生成之前用的出生位置(GameMode找出生点时地形还没生成)，见ForeverPlayerGameMode.cpp。
	static FVector GetDefaultSpawnLocation();

	// 以下Ensure*Generated都幂等(已存在直接返回),只保证自己那一个域,不会调别的域/步骤,
	// 依赖顺序由BuildGenerationStages保证。读档时按存档恢复,失败退回重新生成。
	// 人口:假定map已生成。
	void EnsurePopulaceGenerated();

	// 社会:假定map/populace已生成,见society.md。
	void EnsureSocietyGenerated();

	// 玩家:假定populace已生成(开局时间=populace当前年份1月1日8点,见populace.md"生日换算")。
	void EnsurePlayerGenerated();

	// 产业:按地图的Room生成仓库/工坊。
	void EnsureIndustryGenerated();

	// 交通:只创建Core对象,AVehicleElement要等game_start之后才生成(见.cpp)。
	void EnsureTrafficGenerated();

	// 剧情:Story::Init后交给storyFramework并广播game_start(读档恢复脚本状态时不广播),见ForeverStoryFrameworkComponent.md。
	void EnsureStoryGenerated();

	Map* GetMap() const { return map; }
	Populace* GetPopulace() const { return populace; }
	Society* GetSociety() const { return society; }
	Story* GetStory() const { return story; }
	Industry* GetIndustry() const { return industry; }
	Traffic* GetTraffic() const { return traffic; }
	Player* GetPlayer() const { return player; }

	// 玩家当前操控的市民(玩家Pawn是ACitizenElement时)，不是市民(载具/没有Pawn)返回nullptr。
	// 玩家的钱就是它在Ledger里的账户，物品在Player上，见Core/society/economy.md"玩家参与经济"。
	Citizen* GetControlledCitizen() const;

	// 统一的Change消费入口:任何地方产出的Change都通过它消费。先处理需要UE层能力的类型,
	// 余下的转发给Core各域的ApplyChange,见ForeverFrameworkActor.md。
	// @change: 待执行的变化
	// @context: 变量路由上下文
	void ApplyChange(const Change* change, const ScriptContext& context);

protected:
	virtual void BeginPlay() override;

	// 校验主线剧情.script的mod_dependences声明的每个mod id都已注册(Registry::CheckModRegistered)，
	// 缺失就THROW_EXCEPTION,由RunGenerationStep统一处理。
	void ValidateMainStoryDependencies();

	// 世界生成阶段：BeginPlay登记，Tick每帧执行一个(RunGenerationStep)，见.md"分阶段生成"。
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

	// 生成期间驱动生成阶段;生成完后驱动全局时钟、天空/天气/植被风/路灯和各域Tick,见.md"Tick"。
	virtual void Tick(float DeltaTime) override;

	// 必须在这里同步delete各域,不能只靠析构函数:析构时机取决于UObject垃圾回收,可能晚于下一次PIE,
	// 旧mod dll没卸载干净会让下一次LoadLibrary拿到旧镜像(代码改了编译了PIE里跑的还是旧逻辑)。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 读档:第一次调用时从UForeverSaveSubsystem取走等待中的读档请求(之后一直返回同一份,没有读档返回nullptr)。
	FForeverPendingLoad* FetchActiveLoad();
	TSharedPtr<FForeverPendingLoad> activeLoad;
	bool bActiveLoadFetched = false;
	// 读档时人口是否按存档恢复了(社会段按人口段的市民下标引用市民)，以及人口段留给社会段去连的
	// (JobExperience, 组织下标),见PopulaceSaveCodec::Load。
	bool bPopulaceRestored = false;
	std::vector<std::pair<JobExperience*, int>> pendingJobExperienceOrganizations;
	// 读档时脚本状态按存档恢复了:不广播game_start,玩家操控谁改由RestoreControlledPawn按存档UE段恢复。
	bool bScriptsRestored = false;
	// 整段世界生成流程(新游戏含game_start广播，读档含RestoreControlledPawn)已经走完。
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

	// 7个Core顶层域对象的生命周期都归这个Actor持有(纯C++类型,不是UPROPERTY),彼此平级,
	// 域之间的编排在这个Actor里做(如Populace不知道Map的存在,只通过Map::Checkin单向被读取,
	// 见Source/Core/populace/populace.md)。Zone/Building/Roadnet等组件共用同一个Map实例。
	Map* map = nullptr;
	Populace* populace = nullptr;
	Story* story = nullptr;
	Society* society = nullptr;
	Industry* industry = nullptr;
	Traffic* traffic = nullptr;
	Player* player = nullptr;

	// 第一帧强制当作跨天:开局当天不会天然触发CrossDay(),否则第一天的Job/Organization调度表生成不出来。
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

	// 全图路灯(每盏一个ALampElement蓝图Actor)+昼夜开关灯，见Source/Forever/Framework/ForeverLampFrameworkComponent.md。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverLampFrameworkComponent> lampFramework;

	// 天气表现(云/雨雪/风/光照衰减)，见Source/Forever/Framework/ForeverWeatherFrameworkComponent.md。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverWeatherFrameworkComponent> weatherFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverZoneFrameworkComponent> zoneFramework;

public:
	FORCEINLINE UForeverAssetFrameworkComponent* GetAssetFramework() const { return assetFramework; }
	FORCEINLINE UForeverBuildingFrameworkComponent* GetBuildingFramework() const { return buildingFramework; }

	// BeginPlay的世界生成流程是否已经全部结束(game_start已广播完/读档的玩家与世界物品已恢复)。
	// 开局黑屏(UFadeWidget)用它判断世界是否就绪。
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
