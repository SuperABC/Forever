#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForeverFrameworkActor.generated.h"

class Map;
class Populace;
class Society;
class Story;
class Industry;
class Traffic;
class Player;

class USceneComponent;
class UForeverAssetFrameworkComponent;
class UForeverBuildingFrameworkComponent;
class UForeverPopulaceFrameworkComponent;
class UForeverRoadnetFrameworkComponent;
class UForeverRoomFrameworkComponent;
class UForeverStoryFrameworkComponent;
class UForeverTerrainFrameworkComponent;
class UForeverTrafficFrameworkComponent;
class UForeverZoneFrameworkComponent;

// 阶段2:场景里唯一的Framework入口Actor,取代旧工程里分散的9个Framework Actor
// (Asset/Building/Global/Populace/Roadnet/Room/Story/Terrain/Traffic/Zone)。
// 内部按域组件划分职责,详见ForeverFrameworkActor.md。**不含Global域组件**——旧工程里
// `GlobalBase`本身就是那个唯一放在关卡里、串联其它Framework Actor的入口,而这个角色现在由
// `AForeverFrameworkActor`自己承担了,不需要再嵌一个"Global"子组件重复这件事;`GlobalBase`
// 剩下的编排逻辑(GlobalPause/DrawMap/InitPhone等)将来直接落在这个Actor自己身上,详见
// PHASE4_PLAN.md。
UCLASS()
class FOREVER_API AForeverFrameworkActor : public AActor
{
	GENERATED_BODY()

public:
	AForeverFrameworkActor();

	// 显式声明+在.cpp里定义(那里map/map.h已完整include)——map是原生指针手动管理,
	// Map在这个头文件里只有前置声明。用unique_ptr<Map>试过,UHT为每个UCLASS生成的
	// VTableHelper构造函数(定义在.gen.cpp,看不到map/map.h)会在异常展开路径里引用
	// ~unique_ptr<Map>而编译失败,所以这里改回原生指针+显式delete(纯C++类型,不跨模块
	// 边界——Core.lib静态链接进本模块,不是REFACTOR_PLAN.md说的那种跨DLL new/delete场景)。
	virtual ~AForeverFrameworkActor();

	// 幂等(map已存在直接返回):只保证Map这一个域——新建Map、依次跑InitTerrains/InitRoadnet/
	// InitZones/InitBuildings、交给Terrain/Roadnet/Zone/Building四个Framework组件生成对应
	// 网格(顺序不能反,Roadnet要采样地形/水面,Zone/Building要用到Roadnet产出的Lot)。**不会
	// 调用其它Ensure*Generated()**——BeginPlay()里已经按依赖顺序把全部7个显式列出来顺序
	// 调用了,函数体内部重复调上游没有意义(见BeginPlay定义处的说明)。BeginPlay和
	// ForeverGameMode::FindPlayerStart_Implementation都会调用它,保证不论两者实际调用顺序
	// 如何,出生点计算时地形都已经生成好,详见ForeverFrameworkActor.md。
	// 阶段4-1(Roadnet落地)从EnsureTerrainGenerated改名——现在编排的不只是地形。原来这一个
	// 函数还顺带创建了Populace/Society/Player/Industry/Traffic/Story共7个域的对象,现在
	// 按域拆成7个独立的Ensure*Generated(),这个函数收窄回只保证Map,语义详见下面6个同名方法。
	void EnsureMapGenerated();

	// 幂等(populace已存在直接返回):只保证Populace这一个域——new Populace()、
	// Populace::Init(map->ComputeAccommodationTarget())、map->Checkin(*populace)、交给
	// populaceFramework缓存citizen列表。假定map已经生成好(EnsureMapGenerated()已经跑过,
	// 要用到Map::ComputeAccommodationTarget()/Checkin()),调用方负责保证调用顺序,这个函数
	// 自己不会调EnsureMapGenerated()。
	void EnsurePopulaceGenerated();

	// 幂等(society已存在直接返回):只保证Society这一个域——new Society()、
	// Society::Init(map->GetAllComponents())按Component加权随机分配Organization(每个
	// Organization自己遍历claimed components设计Job)、
	// Society::RecruitCitizens(populace->GetCitizens(), populace->GetCurrentYear())把
	// 成年市民随机匹配到还空缺的Job上。假定map/populace都已经生成好,调用方负责保证调用
	// 顺序,这个函数自己不会调EnsureMapGenerated()/EnsurePopulaceGenerated(),见
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

protected:
	virtual void BeginPlay() override;

	// 阶段4-7(部分)：驱动Player的全局时钟(Player::Tick)——目前这个Actor唯一需要每帧更新
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

	// 阶段4-1:Terrain域落地,Map的生命周期归这个Actor持有(纯C++类型,不是UPROPERTY)。
	// Zone/Building/Roadnet等后续系统迁移时会继续扩展同一个Map实例,不是各自另建一个。
	Map* map = nullptr;

	// 进入populace域新增：和map平级持有，不是map的成员——Populace不知道Map的存在，
	// 通过Map::Checkin(*populace)单向读取，和老工程GlobalBase同时持有map/populace两个
	// 顶层对象、由它做两者之间编排是同一个分工，详见Source/Core/populace/populace.md。
	Populace* populace = nullptr;

	// 阶段4 Story落地：和map/populace平级持有，生命周期管理方式完全一致(EnsureStoryGenerated
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverStoryFrameworkComponent> storyFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverTerrainFrameworkComponent> terrainFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverTrafficFrameworkComponent> trafficFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverZoneFrameworkComponent> zoneFramework;

public:
	FORCEINLINE UForeverAssetFrameworkComponent* GetAssetFramework() const { return assetFramework; }
	FORCEINLINE UForeverBuildingFrameworkComponent* GetBuildingFramework() const { return buildingFramework; }
	FORCEINLINE UForeverPopulaceFrameworkComponent* GetPopulaceFramework() const { return populaceFramework; }
	FORCEINLINE UForeverRoadnetFrameworkComponent* GetRoadnetFramework() const { return roadnetFramework; }
	FORCEINLINE UForeverRoomFrameworkComponent* GetRoomFramework() const { return roomFramework; }
	FORCEINLINE UForeverStoryFrameworkComponent* GetStoryFramework() const { return storyFramework; }
	FORCEINLINE UForeverTerrainFrameworkComponent* GetTerrainFramework() const { return terrainFramework; }
	FORCEINLINE UForeverTrafficFrameworkComponent* GetTrafficFramework() const { return trafficFramework; }
	FORCEINLINE UForeverZoneFrameworkComponent* GetZoneFramework() const { return zoneFramework; }
};
