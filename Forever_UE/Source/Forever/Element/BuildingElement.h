#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Containers/Queue.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "BuildingElement.generated.h"

class UProceduralMeshComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMesh;
class UMaterialInterface;
class Cabin;
class UMaterialInstanceDynamic;
class Building;
class Floor;
class Room;
struct WallStyle;
struct FHitResult;
class UForeverBuildingFrameworkComponent;

// 每栋building一个独立Actor：近/远LOD组件、碰撞盒、电梯全部挂在这栋楼自己的Actor上(避免全地图共用一个owner时碰撞体焊接开销拖累)，
// LOD切换状态机在自己的Tick里跑，只共享框架组件的全局LOD操作预算，详见BuildingElement.md。
UCLASS()
class FOREVER_API ABuildingElement : public AActor
{
	GENERATED_BODY()

public:
	ABuildingElement();

	// 由GenerateBuildings()在SpawnActor之后立刻调用一次：绑定Building和framework(材质/网格解析缓存与共享配置都在framework上)，
	// 同步建好远处灰色box作为初始状态(currentLod初值为Far)。
	void Init(Building* inBuilding, UForeverBuildingFrameworkComponent* inFramework);

	virtual void Tick(float DeltaTime) override;

	// 置空building：关卡卸载时Core的Map可能先被delete，Actor之间EndPlay顺序不保证，只需保证自己EndPlay后Tick不再解引用building。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 电梯的交互选项在运行中变了(add_option/remove_option，由AForeverFrameworkActor::
	// RefreshInteractOptions调用)：重新烘焙快照；玩家正压着它的触发盒时同步换掉屏幕上的选项。
	void RefreshCabinOptions(const Cabin* cabin);
	static ABuildingElement* FindForBuilding(const Building* building);

	// 这个碰撞盒是不是本栋楼某个Room的进入/离开检测盒，是则返回那个Room，否则nullptr。
	// 供玩家切换操控后按"当前重叠的盒子"重新判定所在房间用，不依赖进入/离开事件。
	Room* FindRoomOfBox(UPrimitiveComponent* box) const {
		Room* const* found = roomBoxLabels.Find(box);
		return found ? *found : nullptr;
	}

	// 这个碰撞盒是不是本栋楼整体的进入/离开检测盒，是则返回这栋楼，否则nullptr。
	// 供AForeverFrameworkActor::DescribeLocation按世界坐标反查所在建筑用。
	Building* FindBuildingOfBox(UPrimitiveComponent* box) const;

private:
	enum class ELod : uint8 { Near, Far };

	enum class ELodOpType : uint8 {
		BuildFarMesh,        // 近->远，第1步
		DeleteAllNearMeshes, // 近->远，最后一步(执行完才更新currentLod)
		BuildFloorMesh,      // 远->近，每层一条
		BuildElevatorCabins, // 远->近，所有BuildFloorMesh之后、DeleteFarMesh之前
		DeleteFarMesh,       // 远->近，最后一步(执行完才更新currentLod)
		BuildDecorations,    // 远->近，所有BuildFloorMesh之后、BuildElevatorCabins之前
	};

	struct FLodOp {
		ELodOpType type = ELodOpType::BuildFarMesh;
		int32 floorIndex = -1; // 仅BuildFloorMesh使用
	};

	// 一台电梯轿厢的近处LOD表现——只在currentLod==Near时存在，随DeleteAllNearMeshes一起清空。
	// 电梯的状态(所在楼层/行程)在Core的Cabin里，这里只按它摆放轿厢网格。
	// innerTrigger/frontTriggers：轿厢内(跟着轿厢移动)、各层电梯正面的交互触发盒——玩家进入
	// 任意一个就弹出这台电梯的选项，全部离开才收起，见Core/map/cabin.md。
	// interactNameOnly/cachedOptions是烘焙好的快照，Overlap回调只读它们，不解引用cabin——和
	// UForeverDoorComponent同一套安全原则。
	struct FCabin {
		Cabin* cabin = nullptr;
		TObjectPtr<UStaticMeshComponent> comp;
		TObjectPtr<UBoxComponent> innerTrigger;
		float innerX = 0.f, innerY = 0.f; // 轿厢内触发盒的世界XY(靠门那一半的中心，不是轿厢中心)
		TArray<TObjectPtr<UBoxComponent>> frontTriggers;
		float worldX = 0.f, worldY = 0.f, rotation = 0.f;
		float innerHalfZ = 0.f; // 轿厢内触发盒半高，中心=轿厢底+这个值
		float meshZOffset = 0.f; // 轿厢mesh相对轿厢底的抬高(UE单位)，触发盒不受影响
		FString interactNameOnly;
		TArray<FString> cachedOptions;
		bool hasScript = false;
		int32 playerOverlaps = 0; // 玩家当前同时压着这台电梯几个触发盒(门口和轿厢内可能同时压到)
		// 上一次同步到组件的轿厢高度(UE单位)。轿厢没动就不再SetWorldLocation：每次SetWorldLocation
		// 即使位置没变也会让组件重新做一遍碰撞重叠更新，近处楼多、电梯多时实测占游戏线程16毫秒。
		float lastSyncedZ = TNumericLimits<float>::Lowest();
	};

	UBoxComponent* SpawnCabinTrigger(float centerX, float centerY, float centerZ,
		float sizeX, float sizeY, float sizeZ, float rotation, bool isMovable);
	FCabin* FindCabinByTrigger(const UPrimitiveComponent* trigger);
	void ShowCabinOptions(const FCabin& cabin);
	void HideCabinOptions(const FCabin& cabin);

	UFUNCTION()
	void OnCabinOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnCabinOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 当前存在的Element(按Building*查)，供RefreshCabinOptions的调用方找到电梯所在的那栋楼。
	static TMap<const Building*, TWeakObjectPtr<ABuildingElement>> elementsByBuilding;

	// 推进这栋楼所有电梯的行程(不管LOD，远景下电梯也会到站)，到站写回Core；近处时同步轿厢位置。
	void UpdateCabins(float deltaSeconds);

	void BuildFarSection();
	void BuildFloorSection(int32 floorIndex);
	void BuildElevatorCabinsForBuilding();
	// mod用AddDecoration指定的自定义模型(Building::GetDecorations()，世界摆位Core已经按朝向
	// 换算好)，近处LOD专属，ClearNearSections一并清理。
	void BuildDecorations();
	void ClearNearSections();
	void ClearFarSection();
	void ExecuteLodOp(const FLodOp& op);

	// building进入/离开检测用的碰撞盒。水平=body矩形，垂直=整栋楼Z范围，三个方向各+0.01
	// (地图单位)，和Room碰撞盒的各-0.01配对，两者贴合的边界不会因为完全重合而在Overlap判定
	// 上抖动。Init()里创建一次，常驻到这个Element被销毁，不随近/远LOD切换增删。
	void BuildCollisionBox();

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// Room进入/离开检测碰撞盒——这栋building的每个Room各一个，Init()里创建一次，常驻到Element销毁。
	void BuildRoomCollisionBoxes();

	UFUNCTION()
	void OnRoomOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnRoomOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 墙体/地板/天花板slab按材质指针分组走常驻的UInstancedStaticMeshComponent：近处建层只AddInstance，离开近处只ClearInstances，
	// 组件本身不随LOD切换销毁/重建，见BuildingElement.md。
	void AddCubeInstance(float centerX, float centerY, float centerZ,
		float sizeX, float sizeY, float sizeZ, float rotation, UMaterialInterface* material);

	// 按材质指针查/建对应的ISM池条目——找不到就创建一个(复用framework->GetCubeMesh()，
	// 和原SpawnCube同一份网格资源)。
	UInstancedStaticMeshComponent* GetOrCreateCubeInstancePool(UMaterialInterface* material);

	// 把pendingCubeTransforms里按材质分组暂存的全部transform批量提交给对应ISM池——每个
	// 材质只调一次UInstancedStaticMeshComponent::AddInstances()，而不是AddCubeInstance每次
	// 都调一次单实例AddInstance()。见AddCubeInstance/pendingCubeTransforms声明处注释。
	void FlushCubeInstances();

	// 按目标尺寸缩放摆放一个网格实体，供楼梯/坡道/电梯轿厢这类有真实3D资产的元素用。isMovable
	// 只有电梯轿厢会传true。
	UStaticMeshComponent* SpawnMesh(float centerX, float centerY, float centerZ,
		float sizeX, float sizeY, float sizeZ, float rotation, UStaticMesh* mesh, UMaterialInterface* material,
		bool isMovable = false);

	// 五材质槽Wall的ISM池key：东南西北四个侧面的材质+上下面材质，见AddWallInstance。
	struct FWallMaterialKey {
		UMaterialInterface* slots[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
		bool operator==(const FWallMaterialKey& other) const {
			for (int32 i = 0; i < 5; i++) if (slots[i] != other.slots[i]) return false;
			return true;
		}
		friend uint32 GetTypeHash(const FWallMaterialKey& key) {
			uint32 hash = 0;
			for (int32 i = 0; i < 5; i++) hash = HashCombine(hash, GetTypeHash(key.slots[i]));
			return hash;
		}
	};

	// 带材质的墙体段走Wall(槽0东 1南 2西 3北 4上下)，按(五个材质)分组走独立ISM池，其余同AddCubeInstance。
	void AddWallInstance(float centerX, float centerY, float centerZ,
		float sizeX, float sizeY, float sizeZ, float rotation, const FWallMaterialKey& key);
	UInstancedStaticMeshComponent* GetOrCreateWallInstancePool(const FWallMaterialKey& key);

	// 逐段生成墙体cube，门/窗开口处不生成任何东西。styles是4项(下标=墙朝向)的WallStyle，可为nullptr(=都没标签)。
	void BuildWallsForElement(float floorBaseZ, float floorHeight,
		float elemCenterX, float elemCenterY, float elemSizeX, float elemSizeY,
		bool wallWest, bool wallEast, bool wallNorth, bool wallSouth,
		const std::unordered_map<int, std::vector<std::array<float, 8>>>& doors,
		const std::unordered_map<int, std::vector<std::array<float, 8>>>& windows,
		UMaterialInterface* wallMaterial, const WallStyle* styles = nullptr);

	Building* building = nullptr;
	TWeakObjectPtr<UForeverBuildingFrameworkComponent> framework;

	UPROPERTY()
	TObjectPtr<USceneComponent> elementRoot;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> farMesh; // 这栋楼专属，只有一个section(index 0)

	ELod currentLod = ELod::Far;
	bool transitionPending = false;
	int32 nearFloorCount = 0; // basements+layers，BuildFloorMesh按floorIndex 0..nearFloorCount-1处理

	// 墙体/地板/天花板cube的ISM池，key是实际材质指针(通常整栋楼只有3个，mod覆盖材质时会多几个)，Init()之后按需懒创建。
	TMap<UMaterialInterface*, TObjectPtr<UInstancedStaticMeshComponent>> cubeInstancePools;

	// AddCubeInstance按材质分组暂存world transform，一层楼画完后由FlushCubeInstances()对每个材质批量AddInstances()一次
	// (逐个AddInstance在BlockAll碰撞的ISM上每次约0.1~0.4ms)。只在BuildFloorSection内部临时持有，提交后清空。
	TMap<UMaterialInterface*, TArray<FTransform>> pendingCubeTransforms;
	// 与pendingCubeTransforms逐实例对应的自定义数据(每实例2个float：建筑朝向的(cos, sin))，Flush时写入ISM的PerInstanceCustomData，材质里可读。
	TMap<UMaterialInterface*, TArray<float>> pendingCubeCustomData;

	// Wall(五材质槽)的ISM池与暂存，结构同上，key换成五个材质的组合。wallMesh首次用到时才解析，失败(用户还没做资产)就一直是nullptr，带标签的墙退回普通Cube。
	TMap<FWallMaterialKey, TObjectPtr<UInstancedStaticMeshComponent>> wallInstancePools;
	TMap<FWallMaterialKey, TArray<FTransform>> pendingWallTransforms;
	// 与pendingWallTransforms逐实例对应的自定义数据(每实例(cos, sin))，同pendingCubeCustomData。
	TMap<FWallMaterialKey, TArray<float>> pendingWallCustomData;
	TObjectPtr<UStaticMesh> wallMesh;
	bool wallMeshResolved = false;

	// 近处这栋building当前占用的楼梯/坡道3D网格组件(SpawnMesh产出，网格/材质各不相同、
	// 数量少，不纳入ISM改造)，按floorIndex分组，方便ClearNearSections整栋清空。墙体/
	// 地板/天花板cube已经改走上面的cubeInstancePools，不再经过这个数组。
	TArray<TArray<TObjectPtr<UStaticMeshComponent>>> nearComponentsByFloor;
	// 电梯轿厢，近处LOD专属，DeleteAllNearMeshes时一并清空。
	TArray<FCabin> cabins;
	// 自定义模型(BuildDecorations产出)，近处LOD专属，DeleteAllNearMeshes时一并清空。
	TArray<TObjectPtr<UStaticMeshComponent>> nearDecorations;

	// 近/远LOD切换的操作队列，每帧按框架组件的全局预算出队，见Tick()/TryConsumeLodOpBudget。
	TQueue<FLodOp> floorTierOpQueue;

	UPROPERTY()
	TObjectPtr<UBoxComponent> collisionBox;
	// 进出建筑事件用的名字快照(building->GetName()/所在园区的GetName()，不在园区里为空)。
	std::string buildingNameOnly;
	std::string zoneNameOnly;

	// Room碰撞盒对应组件指针 -> 这个盒子对应哪个Room*——一栋楼有多个Room，不能像building
	// 自己的碰撞盒那样只用一个FString，需要按组件查表。回调里同时用它反查Room*(更新
	// Player::SetCurrentRoom，见Source/Core/player/player.h"武器只挂肩膀"一节旁边的房间
	// 物理绑定)和现场用room->GetAddress()生成调试文本(不预先烘焙字符串)。不能标
	// UPROPERTY——Room*是Core层类型，不是UObject/USTRUCT，UHT反射不认；GC安全性不受影响，
	// 这些UPrimitiveComponent由Actor自己的组件树持有，见类注释同类说明。
	TMap<UPrimitiveComponent*, Room*> roomBoxLabels;
};
