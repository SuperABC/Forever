#pragma once

#include <string>

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverZoneFrameworkComponent.generated.h"

class UInstancedStaticMeshComponent;
class UBoxComponent;
class UPrimitiveComponent;
class Map;
class Zone;
struct ZoneWallSpec;
struct FHitResult;

// 阶段4-1 Zone落地：这次Zone只是一个footprint+类型的占位对象(Zone内部再摆Building的递归
// 布局明确推迟，见map.md"InitZones"一节)。可视化手段是围墙——Zone自身不再画扁box占位
// (围墙本身已经足够表达zone范围)，只按ZoneMod声明的ZoneWallSpec沿边铺设围墙mesh实例；
// 大门这次没有资产，只在Zone上存数据，不生成任何几何。园区内部建筑(Zone::GetInternalBuildings)
// 改由ForeverBuildingFrameworkComponent统一渲染(和顶层Building走同一套楼体footprint/楼层/
// LOD逻辑)，这个组件不再单独画一份扁box。详见ForeverZoneFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverZoneFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverZoneFrameworkComponent();

	// 由AForeverFrameworkActor在Map::InitZones()跑完后调用一次。map生命周期由调用方持有。
	void GenerateZones(Map* inMap);

private:
	// 按mesh资产路径复用/新建ISM组件(同一路径的mesh只创建一个ISM，不同围墙段共用)，逻辑照抄
	// ForeverRoadnetFrameworkComponent::GetOrCreateRoadISM。
	UInstancedStaticMeshComponent* GetOrCreateWallISM(const std::string& meshPath);

	// 按wall.direction取zone这条边的世界坐标起止点(局部坐标原点在zone矩形中心)，
	// marginStart/marginEnd沿边收缩、depth/depthInward算出深度方向偏移，得到这段围墙的
	// 世界空间中心线，套用和ForeverRoadnetFrameworkComponent::BuildRoadInstances的tileRange
	// 同款"n∈[len/1.2u,len/0.8u]取最接近len/u的整数"算法摆放ISM实例，每个实例沿长度轴缩放
	// 到actualSegLen/unit。n<=0(这段长度连一节unit都铺不出来)这次先跳过，不做退化兜底。
	void BuildWallSegment(const ZoneWallSpec& wall, const Zone& zone);

	// 进入/离开检测碰撞盒：水平=zone自己的Quad(不做尺寸调整，只有Room缩小/Building放大)，
	// 垂直=zone内部所有building的Z范围并集(最深basement底部到最高楼层顶部)——没有内部
	// building的zone没有意义的"内部"体验，跳过、不创建碰撞盒。
	void BuildCollisionBox(Zone* zone);

	UFUNCTION()
	void OnZoneOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnZoneOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	Map* map = nullptr;

	UPROPERTY()
	TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> wallMeshInstances;

	// 碰撞盒对应组件指针 -> 调试日志用的显示名，按zone->GetAddress()烘焙——和Building同一套
	// 安全原则，Overlap回调绝不解引用Zone*/Map*。
	UPROPERTY()
	TMap<TObjectPtr<UPrimitiveComponent>, FString> zoneBoxLabels;
};
