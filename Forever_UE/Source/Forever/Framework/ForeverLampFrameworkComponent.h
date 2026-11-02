#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverLampFrameworkComponent.generated.h"

class ALampElement;
class Map;
class Time;

// 全图路灯：Core(Map::GetLamps())里每盏灯生成一个ALampElement蓝图Actor，并按游戏时间统一开关灯
// (晚上17点到次日早上7点亮，其余时间灭)。详见ForeverLampFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverLampFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverLampFrameworkComponent();

	// 由AForeverFrameworkActor在地形生成之后调用一次(Z取地形高度)。map生命周期由调用方持有。
	void GenerateLamps(Map* inMap);

	// 运行时新增一盏灯，mapX/mapY是地图element坐标，yaw是弧度。Z按地形高度自动取，
	// 新灯立即同步当前亮度。返回灯id，失败返回-1。
	int32 AddLamp(const FString& blueprintPath, float mapX, float mapY, float yaw);

	// 按id删除一盏灯，id不存在返回false。
	bool RemoveLamp(int32 lampId);

	// 每帧由AForeverFrameworkActor::Tick调用：按游戏时间算出目标亮度，只在目标变化时才推给全部灯；
	// 另外每updateInterval秒按玩家相机距离给灯切换近/远(SwitchNear/SwitchFar)。
	void UpdateLamps(const Time& gameTime, float deltaTime);

private:
	// 返回蓝图类，已加载过的直接取缓存，加载失败返回nullptr(只打一次Warning)。
	UClass* GetOrLoadClass(const FString& blueprintPath);
	ALampElement* SpawnLamp(UClass* lampClass, float mapX, float mapY, float yaw);
	// 按相机位置给每盏灯切换近/远：灯自己的IsNear()就是当前状态，滞回区间内保持不变。
	void UpdateNearFar();

	Map* map = nullptr; // 非持有,AForeverFrameworkActor拥有生命周期

	// 亮灯时间段：[onHour, 24) 并 [0, offHour)。
	UPROPERTY(EditAnywhere, Category = "Lamp")
	int32 onHour = 17;

	UPROPERTY(EditAnywhere, Category = "Lamp")
	int32 offHour = 7;

	// 近/远切换距离(cm)：远处的灯进入nearDistance变近，近处的灯超过farDistance才变远(两者之差是滞回，
	// 避免在边界上反复切换)。updateInterval是两次近/远检查之间的间隔(秒)。
	UPROPERTY(EditAnywhere, Category = "Lamp")
	float nearDistance = 4000.f;

	UPROPERTY(EditAnywhere, Category = "Lamp")
	float farDistance = 5000.f;

	UPROPERTY(EditAnywhere, Category = "Lamp")
	float updateInterval = 0.2f;

	float nearFarTimer = 0.f;

	UPROPERTY()
	TMap<FString, TObjectPtr<UClass>> classByPath;
	TSet<FString> failedPaths;

	UPROPERTY()
	TMap<int32, TObjectPtr<ALampElement>> lampActors;
	int32 nextLampId = 0;

	// 当前已经推给灯的亮度；初始-1表示还没推过，第一次UpdateLamps一定会推。
	float currentBrightness = -1.f;
};
