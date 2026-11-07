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

// 昼夜循环：按游戏时钟驱动太阳/大气/雾/环境光/曝光，场景里没有对应Actor就程序化Spawn。
// 组件自己不开Tick，由AForeverFrameworkActor::Tick()每帧调UpdateSky。详见ForeverSkyFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverSkyFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverSkyFrameworkComponent();

	// 传入当前游戏时钟。
	void UpdateSky(const Time& currentTime);

	// 当前这一帧用的曝光EV100(夜里=nightExposureEV，白天=referenceDayEV，黄昏黎明之间过渡)。
	// 世界空间的UI(比如识别名牌)乘的是固定亮度，要按曝光变化换算，见ForeverIdentifyComponent。
	float GetCurrentExposureEV() const { return currentExposureEV; }
	float GetReferenceDayEV() const { return referenceDayEV; }

	// 天气对光照/雾的缩放，由UForeverWeatherFrameworkComponent每帧写入(已平滑)，晴天都是1，见.md。
	void SetWeatherScales(float lightScale, float fogScale) {
		weatherLightScale = lightScale;
		weatherFogScale = fogScale;
	}

	// ---- 夜晚随游戏时间变暗 ----
	// 下面所有"白天值/夜晚值"都按白天系数t(见.cpp DayFactor，0=深夜,1=白天)线性插值。数值按"月夜微亮"给，PIE里调。

	// 全局环境光(ambientLight)强度(lux)：夜里压到接近月光量级，留一点让轮廓可辨。
	UPROPERTY(EditAnywhere, Category = "Sky|Night")
	float dayAmbientLux = 300.f;

	UPROPERTY(EditAnywhere, Category = "Sky|Night")
	float nightAmbientLux = 2.f;

	// 自动曝光范围(EV100，项目开了ExtendDefaultLuminanceRange，EV越大画面越暗)：夜里Min=nightExposureEV，
	// Max不分昼夜都是dayExposureMaxEV(夜里不设上限，室内亮处才能自动压暗)。
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

	// Lumen的光照刷新速度(引擎默认1，越大越快跟上昼夜/天气变化，也更费)。只管反应速度，不管漏光。
	UPROPERTY(EditAnywhere, Category = "Sky|Lumen")
	float lumenSceneLightingUpdateSpeed = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Sky|Lumen")
	float lumenFinalGatherUpdateSpeed = 3.0f;

private:
	// 幂等：六样东西各自独立检查，缺的先找关卡里现成的，找不到再Spawn。
	void EnsureSkyGenerated();

	void UpdateSunLight(float chronode);
	void UpdateSkyAtmosphere(float chronode);
	void UpdateHeightFog(float chronode);
	void UpdatePostProcess(float chronode);
	void UpdateAmbientLight(float chronode);

	// 当前帧曝光EV，UpdatePostProcess写，GetCurrentExposureEV读。
	float currentExposureEV = 15.f;

	float weatherLightScale = 1.f;
	float weatherFogScale = 1.f;

	UPROPERTY()
	TObjectPtr<ADirectionalLight> sunLight;

	// 不投影的全局环境光，让封闭室内不至于全黑，见.md。
	UPROPERTY()
	TObjectPtr<ADirectionalLight> ambientLight;

	UPROPERTY()
	TObjectPtr<ASkyAtmosphere> skyAtmosphere;

	// 不手动改强度：Movable+实时捕获，跟着SkyAtmosphere的亮度走。
	UPROPERTY()
	TObjectPtr<ASkyLight> skyLight;

	UPROPERTY()
	TObjectPtr<AExponentialHeightFog> heightFog;

	UPROPERTY()
	TObjectPtr<APostProcessVolume> postProcessVolume;
};
