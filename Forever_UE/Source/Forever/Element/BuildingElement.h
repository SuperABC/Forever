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
class UMaterialInstanceDynamic;
class Building;
class Floor;
class Room;
class Door;
struct FHitResult;
class UForeverBuildingFrameworkComponent;
class UForeverDoorComponent;

// 每栋building一个独立Actor，替代之前"全地图所有building共用UForeverBuildingFrameworkComponent
// 单例owner"的做法——后者会让物理引擎对同一个Actor根组件下的大量Static简单碰撞子组件做焊接，
// 焊接开销随这个owner身上已有组件总数线性增长，导致地图越大/玩得越久，新建一层楼越卡(实测
// 验证过程见ForeverBuildingFrameworkComponent.md"性能"一节)。现在每栋楼的近/远LOD组件、
// 碰撞盒全部attach到这栋楼自己的Actor上，爆炸范围被关在"单栋楼自己的组件数"内，不会波及
// 其它building。
//
// UForeverBuildingFrameworkComponent::GenerateBuildings()在Map::InitBuildings()跑完后为
// 每栋building各SpawnActor一个，立刻调用Init()完成绑定；LOD切换(近/远状态机、组件增删)、
// 电梯轿厢动画完全在这个Actor自己的Tick里做，不再由框架组件的TickComponent统一处理——框架
// 组件只保留"每帧全地图共享的LOD操作预算"这一个全局节流点(见
// UForeverBuildingFrameworkComponent::TryConsumeLodOpBudget)，避免大量building同时穿越
// 距离阈值时所有Element一起在同一帧疯狂建组件。
UCLASS()
class FOREVER_API ABuildingElement : public AActor
{
	GENERATED_BODY()

public:
	ABuildingElement();

	// 由UForeverBuildingFrameworkComponent::GenerateBuildings()在SpawnActor之后立刻调用一次：
	// 绑定这个Element对应哪个Building，framework用来回调材质/网格解析(ResolveMaterial/
	// ResolveMesh，全地图共用一份按软路径缓存，不适合每个Element各自维护一份)和共享配置
	// (LOD切换距离/电梯速度/每帧LOD操作预算)。同步建好远处灰色box作为初始状态(currentLod
	// 初值就是Far)。
	void Init(Building* inBuilding, UForeverBuildingFrameworkComponent* inFramework);

	virtual void Tick(float DeltaTime) override;

	// PIE停止/退出游戏时置空building——这个Actor的Tick每帧都会解引用building(GetPosX()等)，
	// AForeverFrameworkActor::EndPlay会delete Core侧的Map(含它拥有的所有Building)，但那和
	// 这个独立Actor自己的EndPlay是两个不同Actor各自的生命周期，不能假设两者的调用顺序——
	// 只保证同一个Actor自己的EndPlay跑完之后这个Actor不会再被Tick，所以这里只需要保证"自己
	// 的EndPlay一跑完，自己的Tick就再也不会解引用building"，不需要关心和框架Actor的EndPlay
	// 谁先谁后，见ForeverBuildingFrameworkComponent.h同一套安全原则。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	enum class ELod : uint8 { Near, Far };

	enum class ELodOpType : uint8 {
		BuildFarMesh,        // 近->远，第1步
		DeleteAllNearMeshes, // 近->远，最后一步(执行完才更新currentLod)
		BuildFloorMesh,      // 远->近，每层一条
		BuildElevatorCabins, // 远->近，所有BuildFloorMesh之后、DeleteFarMesh之前
		DeleteFarMesh,       // 远->近，最后一步(执行完才更新currentLod)
		BuildDoorBatch,      // 远->近，DeleteFarMesh之后，每条最多处理GetMaxDoorsPerOp()扇门
	};

	struct FLodOp {
		ELodOpType type = ELodOpType::BuildFarMesh;
		int32 floorIndex = -1; // 仅BuildFloorMesh使用
	};

	// 一台电梯轿厢的近处LOD状态——只在currentLod==Near时存在，随DeleteAllNearMeshes一起清空。
	// zBottom/zTop/worldX/worldY/rotation都是生成时缓存好的世界坐标数值，Tick动画只读这些
	// 缓存值，不解引用building。
	struct FCabin {
		TObjectPtr<UStaticMeshComponent> comp;
		float worldX = 0.f, worldY = 0.f, rotation = 0.f;
		float zBottom = 0.f, zTop = 0.f;
		float phaseOffset = 0.f; // 往返周期里的起始相位(秒)，避免所有轿厢同步摆动
	};

	void BuildFarSection();
	void BuildFloorSection(int32 floorIndex);
	void BuildElevatorCabinsForBuilding();
	void ClearNearSections();
	void ClearFarSection();
	void ExecuteLodOp(const FLodOp& op);

	// 门从楼层构造里拆出来、单独按批次处理，见BuildingElement.md"门加载优先级"一节。
	// GatherPendingDoors在排队Far->Near转场的那一刻一次性收集全部待建门(Door*生成时就
	// 和LOD状态无关，不需要等楼层先建完)；BuildDoorBatch每次从pendingDoors弹出最多
	// framework->GetMaxDoorsPerOp()个执行Init()。
	void GatherPendingDoors();
	void BuildDoorBatch();

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

	// Room进入/离开检测碰撞盒——每栋building自己的所有Room各一个，原来在独立的
	// UForeverRoomFrameworkComponent里(全地图共用一个owner)，现在挪进这里，跟building自己
	// 的碰撞盒/近处LOD组件一样attach到这栋楼专属的Actor，不会再让全地图所有Room的碰撞盒
	// 挤在同一个owner身上互相拖累注册开销。Init()里创建一次，常驻到这个Element被销毁。
	void BuildRoomCollisionBoxes();

	UFUNCTION()
	void OnRoomOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnRoomOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 墙体/地板/天花板slab改成ISM批量实例(替代原来"每段cube一个独立UStaticMeshComponent"
	// 的SpawnCube)——按实际材质指针分组，每组一个常驻的UInstancedStaticMeshComponent，
	// 近处LOD建层只是AddInstance，离开近处只是ClearInstances，组件本身不随LOD切换
	// 销毁/重建，消除了Register/Destroy开销随楼层数累积的性能问题，见
	// ForeverBuildingFrameworkComponent.md"ISM改造"一节。
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

	// 照抄老工程BuildingBase.cpp::ConstructQuad里processFace的算法，逐段生成墙体cube，
	// 门/窗开口处不生成任何东西——和之前UForeverBuildingFrameworkComponent::
	// BuildWallsForElement完全一样的逻辑，只是不再需要传building/state，直接用成员。
	void BuildWallsForElement(float floorBaseZ, float floorHeight,
		float elemCenterX, float elemCenterY, float elemSizeX, float elemSizeY,
		bool wallWest, bool wallEast, bool wallNorth, bool wallSouth,
		const std::unordered_map<int, std::vector<std::array<float, 8>>>& doors,
		const std::unordered_map<int, std::vector<std::array<float, 8>>>& windows,
		UMaterialInterface* wallMaterial);

	Building* building = nullptr;
	TWeakObjectPtr<UForeverBuildingFrameworkComponent> framework;

	UPROPERTY()
	TObjectPtr<USceneComponent> elementRoot;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> farMesh; // 这栋楼专属，只有一个section(index 0)

	ELod currentLod = ELod::Far;
	bool transitionPending = false;
	int32 nearFloorCount = 0; // basements+layers，BuildFloorMesh按floorIndex 0..nearFloorCount-1处理

	// 墙体/地板/天花板cube的ISM池——key是实际材质指针(同一层楼的墙/地/顶各自共享一份
	// UMaterialInstanceDynamic，通常整栋楼只有3个key，mod覆盖出不同材质时会多几个)。
	// 近处LOD建层时AddInstance，离开近处时ClearInstances——组件本身常驻不随LOD切换
	// 销毁/重建，这是消除Register/Destroy开销的关键，见AddCubeInstance声明处注释。
	// Init()之后按需懒创建，不需要预先知道有几种材质。
	TMap<UMaterialInterface*, TObjectPtr<UInstancedStaticMeshComponent>> cubeInstancePools;

	// AddCubeInstance本身不再直接调ism->AddInstance()——改成按材质分组暂存world transform，
	// 一层楼画完之后由FlushCubeInstances()对每个材质各调一次批量AddInstances()。单独调用
	// AddInstance()在有碰撞(BlockAll)的ISM上实测每次约0.1~0.4ms(单独建一次物理体+更新渲染
	// 状态)，一层墙体分段多的楼层(300+段)逐个调用能堆到90~120ms的单帧卡顿，批量提交能把这些
	// 开销合并成一次。只在BuildFloorSection内部的"收集-一次性提交"两阶段之间临时持有，提交后
	// 清空，不跨帧保留。
	TMap<UMaterialInterface*, TArray<FTransform>> pendingCubeTransforms;

	// 近处这栋building当前占用的楼梯/坡道3D网格组件(SpawnMesh产出，网格/材质各不相同、
	// 数量少，不纳入ISM改造)，按floorIndex分组，方便ClearNearSections整栋清空。墙体/
	// 地板/天花板cube已经改走上面的cubeInstancePools，不再经过这个数组。
	TArray<TArray<TObjectPtr<UStaticMeshComponent>>> nearComponentsByFloor;
	// 门(建筑门+这一层所有room的房间门)，近处LOD专属，和nearComponentsByFloor同样按
	// floorIndex分组、ClearNearSections一并清理。门洞本身由上面的墙体生成逻辑照常挖出，
	// 跟有没有门扇无关；这里只负责"查到了DoorSpec、真的要放门扇"的那些Door，见
	// BuildFloorSection()/door_system_plan.md。
	TArray<TArray<TObjectPtr<UForeverDoorComponent>>> nearDoorsByFloor;
	// 电梯轿厢，近处LOD专属，DeleteAllNearMeshes时一并清空。
	TArray<FCabin> cabins;

	// 两个队列按优先级分桶，不是一个混合队列——分别对应"楼层级"(墙体/楼梯/电梯井/地板
	// 天花板/远近切换)和"门级"操作。必须分开：如果混在一个FIFO队列里，一栋楼门操作还没
	// 处理完时自己又排进了后续的楼层级操作(比如中途Near->Far)，会被"全地图还有楼层级
	// 操作在等"这个全局门槛挡住队头的门操作而无法出队——而挡住它的楼层级操作恰恰排在它
	// 后面，永远等不到被执行的机会，变成这栋楼自己造成的死锁，且因为门槛是全局的会连带
	// 卡死全地图所有建筑的门加载。分成两个队列后，一栋楼自己的楼层级操作永远不会被自己的
	// 门操作挡住。见Tick()/ForeverBuildingFrameworkComponent::TryConsumeLodOpBudget。
	TQueue<FLodOp> floorTierOpQueue;
	TQueue<FLodOp> doorTierOpQueue;
	// 尚未建出UForeverDoorComponent的门，GatherPendingDoors在排队Far->Near转场时一次性
	// 填入，BuildDoorBatch逐批弹出清空。ClearNearSections里也会清空，避免同一个Actor
	// 反复Far<->Near时残留污染下一次收集结果。
	TArray<Door*> pendingDoors;

	UPROPERTY()
	TObjectPtr<UBoxComponent> collisionBox;
	FString collisionLabel; // Overlap回调只读这份烘焙好的字符串，绝不解引用building

	// Room碰撞盒对应组件指针 -> 这个盒子对应哪个Room*——一栋楼有多个Room，不能像building
	// 自己的碰撞盒那样只用一个FString，需要按组件查表。回调里同时用它反查Room*(更新
	// Player::SetCurrentRoom，见Source/Core/player/player.h"武器只挂肩膀"一节旁边的房间
	// 物理绑定)和现场用room->GetAddress()生成调试文本(不预先烘焙字符串)。不能标
	// UPROPERTY——Room*是Core层类型，不是UObject/USTRUCT，UHT反射不认；GC安全性不受影响，
	// 这些UPrimitiveComponent由Actor自己的组件树持有，见类注释同类说明。
	TMap<UPrimitiveComponent*, Room*> roomBoxLabels;
};
