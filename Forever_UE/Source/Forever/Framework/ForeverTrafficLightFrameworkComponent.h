#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverTrafficLightFrameworkComponent.generated.h"

class ATrafficLightElement;
class JunctionTraffic;
class Map;

// 红绿灯：有信号的路口(多于两条路，分组在Core JunctionTraffic里)登记成一条，Tick里按玩家水平距离生成/销毁
// 这个路口的ATrafficLightElement，同时激活/关掉它的信号(只有生成出来的路口有红绿灯，没生成的当作没有)，
// 每帧把当前信号推给灯。设计见ForeverTrafficLightFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverTrafficLightFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverTrafficLightFrameworkComponent();
	virtual void TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	// 流式距离(地图单位，1=10米，水平距离)：玩家到路口中心小于spawn就生成灯并激活信号，大于despawn就销毁并关掉。
	// 没有数量上限和每帧上限。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrafficLight|Streaming")
	float trafficLightSpawnDistance = 16.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrafficLight|Streaming")
	float trafficLightDespawnDistance = 32.f;

private:
	struct FJunctionEntry
	{
		int32 junctionId = -1;
		FVector2D center = FVector2D::ZeroVector; // UE世界坐标
		TArray<FTransform> transforms;            // 每个灯的世界变换
		TArray<int32> approaches;                 // 每个灯显示哪条路的信号
		TWeakObjectPtr<ATrafficLightElement> element;
	};

	// 按Map的路口和JunctionTraffic的信号分组算出每个路口的灯位。
	void RegisterJunctions(const Map& map, const JunctionTraffic& traffic);
	void DestroyAll(JunctionTraffic* traffic);

	TArray<FJunctionEntry> entries;
	// 登记时用的路口通行表：交通组件换地图时会重建它(GenerateVehicles)，变了就重新登记。只用来比较，不解引用。
	const JunctionTraffic* registeredFor = nullptr;
};
