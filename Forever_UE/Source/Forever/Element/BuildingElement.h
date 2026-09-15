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
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class Building;
class Floor;
class Room;
struct FHitResult;
class UForeverBuildingFrameworkComponent;

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

	// 复用一个通用的单位立方体网格，缩放到目标尺寸摆一个墙体/地板/天花板slab；始终attach到
	// 这个Element自己(不再需要像实验阶段那样传一个外部owner)。返回值已经
	// RegisterComponent+AddInstanceComponent，调用方负责收集进nearComponentsByFloor。
	UStaticMeshComponent* SpawnCube(float centerX, float centerY, float centerZ,
		float sizeX, float sizeY, float sizeZ, float rotation, UMaterialInterface* material);

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
		UMaterialInterface* wallMaterial, int32 floorIndex);

	Building* building = nullptr;
	TWeakObjectPtr<UForeverBuildingFrameworkComponent> framework;

	UPROPERTY()
	TObjectPtr<USceneComponent> elementRoot;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> farMesh; // 这栋楼专属，只有一个section(index 0)

	ELod currentLod = ELod::Far;
	bool transitionPending = false;
	int32 nearFloorCount = 0; // basements+layers，BuildFloorMesh按floorIndex 0..nearFloorCount-1处理
	// 近处这栋building当前占用的所有独立组件(墙体分段/地板/天花板slab/楼梯/坡道/窗户网格)，
	// 按floorIndex分组，方便ClearNearSections只清空单层或整栋。
	TArray<TArray<TObjectPtr<UStaticMeshComponent>>> nearComponentsByFloor;
	// 电梯轿厢，近处LOD专属，DeleteAllNearMeshes时一并清空。
	TArray<FCabin> cabins;
	TQueue<FLodOp> lodOpQueue;

	UPROPERTY()
	TObjectPtr<UBoxComponent> collisionBox;
	FString collisionLabel; // Overlap回调只读这份烘焙好的字符串，绝不解引用building

	// Room碰撞盒对应组件指针 -> 调试日志用的显示名(按room->GetAddress()烘焙)——一栋楼有多个
	// Room，不能像building自己的碰撞盒那样只用一个FString，需要按组件查表。
	UPROPERTY()
	TMap<TObjectPtr<UPrimitiveComponent>, FString> roomBoxLabels;
};
