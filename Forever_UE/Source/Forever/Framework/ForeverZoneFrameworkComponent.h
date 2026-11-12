#pragma once

#include <string>

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverZoneFrameworkComponent.generated.h"

class UInstancedStaticMeshComponent;
class UBoxComponent;
class UPrimitiveComponent;
class AForeverFrameworkActor;
class Map;
class Zone;
struct ZoneWallSpec;
struct FHitResult;

// 园区(Zone)的围墙、大门、自定义模型和进出检测碰撞盒。围墙按ZoneMod声明的ZoneWallSpec沿边铺ISM实例；
// 园区内部建筑不在这里画，由ForeverBuildingFrameworkComponent统一渲染。详见ForeverZoneFrameworkComponent.md。
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

	// 园区自定义模型(Zone::GetDecorations()，世界摆位Core已经按大门方向换算好)：每个一个Static的
	// UStaticMeshComponent，和围墙/大门一样常驻(园区没有LOD)。
	void BuildDecorations(const Zone& zone);

	// 园区地面：zone.GetGapMaterialPath()非空时，园区quad扣掉内部建筑quad后剩下的空隙，用扫描线拆成梯形铺一块ProceduralMesh(高度ZONE_GROUND_TOP=20cm，与建筑gap同高)。
	void BuildGround(const Zone& zone);

	UFUNCTION()
	void OnZoneOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnZoneOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	Map* map = nullptr;

	AForeverFrameworkActor* GetFrameworkActor();
	TWeakObjectPtr<AForeverFrameworkActor> cachedFrameworkActor;

	UPROPERTY()
	TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> wallMeshInstances;

	// 碰撞盒 -> zone->GetName()(UTF-8)，进出园区事件用。烘焙好的快照，Overlap回调绝不解引用
	// Zone*/Map*，和Building同一套安全原则。
	TMap<UPrimitiveComponent*, std::string> zoneBoxNames;

public:
	// 这个碰撞盒是不是某个园区的检测盒，是则返回园区名，否则nullptr。供DescribeLocation按世界坐标反查用。
	const std::string* FindZoneNameOfBox(UPrimitiveComponent* box) const { return zoneBoxNames.Find(box); }
};
