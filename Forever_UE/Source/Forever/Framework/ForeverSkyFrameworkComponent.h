#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverSkyFrameworkComponent.generated.h"

class ADirectionalLight;
class ASkyAtmosphere;
class ASkyLight;
class AExponentialHeightFog;
class APostProcessVolume;
class Time;

// 昼夜循环：仿照老工程`AChronodeManager`的数学(太阳旋转/颜色/强度、大气散射、雾浓度、
// 自动曝光全部照抄原样公式/常量)，但这次不走"C++算+蓝图Tick调度"的分工——逻辑全部在
// C++里，由AForeverFrameworkActor::Tick()统一调用UpdateSky，不新开Tick入口，见
// ForeverFrameworkActor.md"Tick"一节的既定规矩。
//
// 老工程的DirectionalLight/SkyAtmosphere/SkyLight/ExponentialHeightFog/
// PostProcessVolume这5个Actor是手动摆在关卡里的，EnsureSkyGenerated()这次改成找不到就
// 程序化Spawn一份(和EnsureMapGenerated()等"Ensure*Generated幂等保证"同一个既定模式)，
// 不要求在关卡里手动摆放任何东西。
UCLASS()
class FOREVER_API UForeverSkyFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverSkyFrameworkComponent();

	// 由AForeverFrameworkActor::Tick()每帧调用一次，传入当前游戏时钟。
	void UpdateSky(const Time& currentTime);

private:
	// 幂等：五样东西各自独立检查，已存在的跳过，不存在的先TActorIterator找现成的，找不到
	// 再SpawnActor一份。
	void EnsureSkyGenerated();

	// 以下四个方法的公式/常量原样照抄老工程ChronodeManager.cpp对应实现，见.cpp。
	void UpdateSunLight(float chronode);
	void UpdateSkyAtmosphere(float chronode);
	void UpdateHeightFog(float chronode);
	void UpdatePostProcess(float chronode);

	UPROPERTY()
	TObjectPtr<ADirectionalLight> sunLight;

	// 微弱的全局环境光——没有窗户/光源的室内空间(比如地下室)完全照不到sunLight或
	// skyLight(两者的光照贡献都和"这个点能不能看到天空"绑定，封闭空间天然是0)，这个
	// 光源专门解决"完全没有光照来源的室内一片漆黑"这个问题。固定设置一次(不随时间变化、
	// 不随UpdateSky()每帧重算)，见.cpp里EnsureSkyGenerated()。
	UPROPERTY()
	TObjectPtr<ADirectionalLight> ambientLight;

	UPROPERTY()
	TObjectPtr<ASkyAtmosphere> skyAtmosphere;

	// 只生成，不随时间更新——老工程代码从未动过场景里的SkyLight，照抄这个处理方式。
	UPROPERTY()
	TObjectPtr<ASkyLight> skyLight;

	UPROPERTY()
	TObjectPtr<AExponentialHeightFog> heightFog;

	UPROPERTY()
	TObjectPtr<APostProcessVolume> postProcessVolume;
};
