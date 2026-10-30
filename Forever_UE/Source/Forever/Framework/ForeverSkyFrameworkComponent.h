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

	// 当前这一帧用的曝光EV100(夜里=nightExposureEV，白天=referenceDayEV，黄昏黎明之间过渡)。
	// 世界空间的UI(比如识别名牌)乘的是固定亮度，要按曝光变化换算，见ForeverIdentifyComponent。
	float GetCurrentExposureEV() const { return currentExposureEV; }
	float GetReferenceDayEV() const { return referenceDayEV; }

	// ---- 夜晚随游戏时间变暗 ----
	// 白天系数t(0=深夜,1=白天)在太阳高度-0.12~0.2之间平滑过渡(约一个游戏小时的黄昏/黎明)，下面所有
	// "白天值/夜晚值"都按t线性插值。数值先按"月夜微亮"给，PIE里看着不对就改这些属性。

	// 全局环境光(ambientLight)强度(lux)：白天300保持原样，夜里压到接近月光量级(真实月光约0.1~0.3 lux，
	// 这里留一点让轮廓可辨)。
	UPROPERTY(EditAnywhere, Category = "Sky|Night")
	float dayAmbientLux = 300.f;

	UPROPERTY(EditAnywhere, Category = "Sky|Night")
	float nightAmbientLux = 2.f;

	// 自动曝光范围(EV100，r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange=True下的单位，EV越大
	// 画面越暗)：白天保持原来的宽范围，夜里Min=Max=nightExposureEV，等于关掉自动适应、固定曝光——
	// 否则自动曝光会在暗场景里一路提亮，夜晚就"适应"成白天一样亮。
	UPROPERTY(EditAnywhere, Category = "Sky|Night")
	float dayExposureMinEV = -10.f;

	UPROPERTY(EditAnywhere, Category = "Sky|Night")
	float dayExposureMaxEV = 20.f;

	UPROPERTY(EditAnywhere, Category = "Sky|Night")
	float nightExposureEV = 3.f;

	// 白天的参考曝光EV(识别名牌这类固定亮度的世界UI以它为基准换算)。
	UPROPERTY(EditAnywhere, Category = "Sky|Night")
	float referenceDayEV = 15.f;

	// Local Exposure暗部对比度：白天用项目ini里的0.8(提亮暗部)，夜里1.0(不额外提亮暗部)。
	UPROPERTY(EditAnywhere, Category = "Sky|Night")
	float dayLocalShadowContrast = 0.8f;

	UPROPERTY(EditAnywhere, Category = "Sky|Night")
	float nightLocalShadowContrast = 1.0f;

private:
	// 幂等：五样东西各自独立检查，已存在的跳过，不存在的先TActorIterator找现成的，找不到
	// 再SpawnActor一份。
	void EnsureSkyGenerated();

	// 以下四个方法的公式/常量原样照抄老工程ChronodeManager.cpp对应实现，见.cpp。
	void UpdateSunLight(float chronode);
	void UpdateSkyAtmosphere(float chronode);
	void UpdateHeightFog(float chronode);
	void UpdatePostProcess(float chronode);
	void UpdateAmbientLight(float chronode);

	// 当前帧曝光EV，UpdatePostProcess写，GetCurrentExposureEV读。
	float currentExposureEV = 15.f;

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

	// 不手动改强度，EnsureSkyGenerated把它设成Movable+实时捕获，跟着SkyAtmosphere的亮度走(夜里天空
	// 暗，它的光照贡献自然跟着暗)；原来的Static默认值在运行时程序化生成的场景里捕获不到有用的内容。
	UPROPERTY()
	TObjectPtr<ASkyLight> skyLight;

	UPROPERTY()
	TObjectPtr<AExponentialHeightFog> heightFog;

	UPROPERTY()
	TObjectPtr<APostProcessVolume> postProcessVolume;
};
