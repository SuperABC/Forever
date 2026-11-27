#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TrafficLightElement.generated.h"

class UInstancedStaticMeshComponent;
class JunctionTraffic;

// 一个路口的全部红绿灯：每条有驶入车道的路左右各一个，由UForeverTrafficLightFrameworkComponent按距离生成/销毁。
// 4个ISM(框架、红、黄、绿灯罩)，每个灯在4个ISM里各有一个实例；灯罩亮不亮写在逐实例数据0里(1亮0灭，
// M_TrafficLamp读它)。详见TrafficLightElement.md。
UCLASS()
class FOREVER_API ATrafficLightElement : public AActor
{
	GENERATED_BODY()

public:
	ATrafficLightElement();

	// 建实例：transforms是每个灯的世界变换，approaches是这个灯显示哪条路(RoadJunctionApproach下标)的信号。
	// 建完所有灯都是灭的，等RefreshSignals点亮。
	void Build(int32 inJunctionId, const TArray<FTransform>& transforms, const TArray<int32>& approaches);

	// 按路口当前信号刷新：每个灯的信号变了才改那个灯三个灯罩实例的逐实例数据。
	void RefreshSignals(const JunctionTraffic& traffic, double now);

private:
	void SetLamp(int32 light, uint8 signal);

	UPROPERTY(VisibleAnywhere, Category = "TrafficLight")
	TObjectPtr<UInstancedStaticMeshComponent> frameMesh;
	UPROPERTY(VisibleAnywhere, Category = "TrafficLight")
	TObjectPtr<UInstancedStaticMeshComponent> redMesh;
	UPROPERTY(VisibleAnywhere, Category = "TrafficLight")
	TObjectPtr<UInstancedStaticMeshComponent> yellowMesh;
	UPROPERTY(VisibleAnywhere, Category = "TrafficLight")
	TObjectPtr<UInstancedStaticMeshComponent> greenMesh;

	int32 junctionId = -1;
	TArray<int32> lightApproach; // 第i个灯显示的路
	TArray<uint8> lightSignal;   // 第i个灯当前显示的JunctionTraffic::Signal
};
