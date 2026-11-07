#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverWeatherFrameworkComponent.generated.h"

class AVolumetricCloud;
class UDecalComponent;
class UMaterialInstanceDynamic;
class UNiagaraComponent;
class UNiagaraSystem;
class UForeverSkyFrameworkComponent;
class UForeverVegetationFrameworkComponent;
class Weather;

// 一种天气对应的目标参数。UForeverWeatherFrameworkComponent按当前天气取这一组值，平滑过渡过去。
USTRUCT()
struct FForeverWeatherParams
{
	GENERATED_BODY()

	// 体积云材质的全局云量/云密度(0~1)，0=晴空，1=满天乌云。参数名见UForeverWeatherFrameworkComponent。
	UPROPERTY(EditAnywhere, Category = "Weather")
	float cloudCoverage = 0.f;

	UPROPERTY(EditAnywhere, Category = "Weather")
	float cloudDensity = 0.f;

	// 太阳光(和大气散射)的缩放，1=不变；阴天压低。
	UPROPERTY(EditAnywhere, Category = "Weather")
	float lightScale = 1.f;

	// 雾浓度的缩放，>=1。
	UPROPERTY(EditAnywhere, Category = "Weather")
	float fogScale = 1.f;

	// 树的风力/风速倍数，1=微风基准(默认晴天)。
	UPROPERTY(EditAnywhere, Category = "Weather")
	float windStrengthScale = 1.f;

	UPROPERTY(EditAnywhere, Category = "Weather")
	float windSpeedScale = 1.f;

	// 降雨/降雪强度(0~1)，驱动Niagara的User.Intensity。
	UPROPERTY(EditAnywhere, Category = "Weather")
	float rain = 0.f;

	UPROPERTY(EditAnywhere, Category = "Weather")
	float snow = 0.f;
};

// 天气的UE表现层：把Core的Weather翻译成云量、阳光/雾、树的风、雨雪粒子、地面湿润/积雪，天气变化时按现实时间平滑过渡。
// 组件自己不开Tick，由AForeverFrameworkActor::Tick每帧调UpdateWeather。详见ForeverWeatherFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverWeatherFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverWeatherFrameworkComponent();

	// @weather: 当前天气(Core)
	// @playerIndoors: 玩家在室内时雨雪粒子平滑压到0(出门再回来)
	void UpdateWeather(float deltaTime, const Weather& weather, bool playerIndoors);

	// 7种天气各一组目标参数，下标=Weather::Type，默认值在构造函数里，PIE里在Framework Actor细节里调。
	UPROPERTY(EditAnywhere, Category = "Weather")
	TArray<FForeverWeatherParams> weatherParams;

	// 换天气时各项参数大约多少现实秒过渡完(指数趋近，这个时间内走完约95%)。
	UPROPERTY(EditAnywhere, Category = "Weather")
	float transitionSeconds = 20.f;

	// 引擎自带体积云材质里控制云量/云密度的标量参数名。
	UPROPERTY(EditAnywhere, Category = "Weather|Cloud")
	FName cloudCoverageParameter = TEXT("Cloud_GlobalCoverage");

	UPROPERTY(EditAnywhere, Category = "Weather|Cloud")
	FName cloudDensityParameter = TEXT("Cloud_GlobalDensity");

	// 雨/雪粒子随风飘的水平速度基准(cm/s)，乘风力倍数后写进Niagara的User.Wind。
	UPROPERTY(EditAnywhere, Category = "Weather|Precipitation")
	float precipitationWindSpeed = 150.f;

	// 玩家进出室内时，雨雪强度趋近0/1的快慢(现实秒)。
	UPROPERTY(EditAnywhere, Category = "Weather|Precipitation")
	float indoorFadeSeconds = 1.5f;

	// 地面湿润/积雪贴花(跟随相机的俯投影大贴花，见.md)：投影盒的水平半径(cm)和垂直半高(cm)。
	UPROPERTY(EditAnywhere, Category = "Weather|Ground")
	float decalRadius = 15000.f;

	UPROPERTY(EditAnywhere, Category = "Weather|Ground")
	float decalHalfHeight = 15000.f;

	// 地面变湿/积雪、变干/融化各需要多少现实秒(积累快、消退慢)。
	UPROPERTY(EditAnywhere, Category = "Weather|Ground")
	float wetSeconds = 30.f;

	UPROPERTY(EditAnywhere, Category = "Weather|Ground")
	float drySeconds = 120.f;

	UPROPERTY(EditAnywhere, Category = "Weather|Ground")
	float snowSeconds = 60.f;

	UPROPERTY(EditAnywhere, Category = "Weather|Ground")
	float meltSeconds = 180.f;

private:
	// 幂等：云Actor/降水粒子第一次需要时创建。
	void EnsureWeatherGenerated();
	void UpdateClouds(const FForeverWeatherParams& params);
	void UpdatePrecipitation(float deltaTime, const FForeverWeatherParams& params, bool playerIndoors);
	void UpdateGround(float deltaTime, const FForeverWeatherParams& params);

	// 创建一个跟随相机的Niagara降水组件，资源加载失败返回nullptr(只警告一次)。
	UNiagaraComponent* CreatePrecipitation(const TCHAR* assetPath);

	// 当前(已平滑)的参数，每帧向目标靠近。
	FForeverWeatherParams current;
	bool bInitialized = false;
	// 室内系数：1=室外、0=室内，趋近玩家是否在室外。
	float outdoorFactor = 1.f;

	bool bGenerated = false;
	bool bCloudParameterWarned = false;

	// 地面湿润度/积雪度(0~1)，由降雨/降雪强度推动，积累快、消退慢，不受室内影响。
	float wetness = 0.f;
	float snowCover = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> groundDecal;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> groundMaterial;

	UPROPERTY(Transient)
	TObjectPtr<AVolumetricCloud> volumetricCloud;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> cloudMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> rainComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> snowComponent;

	UPROPERTY(Transient)
	TObjectPtr<UForeverSkyFrameworkComponent> skyFramework;

	UPROPERTY(Transient)
	TObjectPtr<UForeverVegetationFrameworkComponent> vegetationFramework;
};
