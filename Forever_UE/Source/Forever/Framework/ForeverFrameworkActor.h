#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForeverFrameworkActor.generated.h"

class Map;

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

	// 幂等:Map已存在则直接返回,否则新建Map、依次跑InitTerrains/InitRoadnet/InitZones/
	// InitBuildings、交给对应Framework组件生成地形/路网/Zone/Building网格(顺序不能反,
	// Roadnet要采样地形/水面,Zone/Building要用到Roadnet产出的Lot)。BeginPlay和
	// ForeverGameMode::FindPlayerStart_Implementation都会调用它,保证不论两者实际调用顺序
	// 如何,出生点计算时地形都已经生成好,详见ForeverFrameworkActor.md。
	// 阶段4-1(Roadnet落地)从EnsureTerrainGenerated改名——现在编排的不只是地形。
	void EnsureMapGenerated();

	Map* GetMap() const { return map; }

protected:
	virtual void BeginPlay() override;

	// 阶段4-1:Terrain域落地,Map的生命周期归这个Actor持有(纯C++类型,不是UPROPERTY)。
	// Zone/Building/Roadnet等后续系统迁移时会继续扩展同一个Map实例,不是各自另建一个。
	Map* map = nullptr;

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
