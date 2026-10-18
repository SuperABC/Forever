#include "Framework/ForeverSkyFrameworkComponent.h"

#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
// ASkyAtmosphere这个Actor类本身就声明在Components/SkyAtmosphereComponent.h里
// (没有独立的Engine/SkyAtmosphere.h)，和USkyAtmosphereComponent共用一个头文件。
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Kismet/GameplayStatics.h"

#include "common/utility.h"

UForeverSkyFrameworkComponent::UForeverSkyFrameworkComponent() {
	// 不开自己的Tick——由AForeverFrameworkActor::Tick()统一调用UpdateSky，见类头注释。
	PrimaryComponentTick.bCanEverTick = false;
}

void UForeverSkyFrameworkComponent::EnsureSkyGenerated() {
	UWorld* world = GetWorld();
	if (!world) return;

	if (!sunLight) {
		sunLight = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(world, ADirectionalLight::StaticClass()));
		if (!sunLight) sunLight = world->SpawnActor<ADirectionalLight>();
		if (sunLight) {
			// 原生C++类默认构造的UDirectionalLightComponent，Mobility默认是Static(不是
			// 编辑器"放置Actor"面板那个默认Movable的蓝图模板)——Static灯光的方向在运行时
			// 改了不会重新渲染，UpdateSunLight()里的SetActorRotation会执行成功但太阳视觉上
			// 完全不动，这是实测确认的根因。必须显式设成Movable才能让每帧的旋转真正生效。
			if (UDirectionalLightComponent* sunComponent = Cast<UDirectionalLightComponent>(sunLight->GetLightComponent())) {
				sunComponent->SetMobility(EComponentMobility::Movable);
				// 场景里现在有两个DirectionalLight(这个+下面的ambientLight)，两者
				// ForwardShadingPriority默认都是0(相等)，会触发引擎警告
				// "Multiple directional lights are competing to be the single one used
				// for forward shading, translucent, water or volumetric fog"——这个
				// 优先级决定"体积雾/半透明/水面"这类前向渲染效果该用哪个方向光照亮，必须
				// 真正的太阳赢，不能回退到"按亮度二选一"这个不确定的fallback。显式给
				// sunLight比默认值更高的优先级(ambientLight保持默认0不变，见下)。
				sunComponent->SetForwardShadingPriority(1);
			}
		}
	}
	if (!ambientLight) {
		// 没有在关卡里"找现成的"这一步——这是这个项目新加的灯光，不是老工程的既有设置，
		// 不存在"美术已经手动摆好一份"的可能性，直接Spawn。
		ambientLight = world->SpawnActor<ADirectionalLight>();
		if (ambientLight) {
			if (UDirectionalLightComponent* ambientComponent =
				Cast<UDirectionalLightComponent>(ambientLight->GetLightComponent())) {
				// 和sunLight同一个坑：原生Spawn出来的默认Mobility是Static，这个光源虽然
				// 不随时间变化，但这个场景的建筑/地形全是运行时程序化生成的，没有走"Build
				// Lighting"烘焙流程，Static灯光在没烘焙过的几何体上可能根本不生效，必须
				// 也设成Movable。
				ambientComponent->SetMobility(EComponentMobility::Movable);
				// 关键：关掉阴影——阴影是"没有光照来源的室内一片漆黑"这个问题的根源(墙体/
				// 天花板会挡住所有带阴影的光源)，不投射阴影的DirectionalLight会直接穿透
				// 所有几何体，均匀照到场景里每一个表面，等效于"全图生效的环境光"，这是
				// UE里做这种效果最直接的办法(没有用SkyLight，因为SkyLight的光照贡献是
				// 按"这个点能看到多少天空"算的，封闭室内天然接收不到，没法解决这个问题)。
				ambientComponent->SetCastShadows(false);
				// "微弱"——比sunLight正午10万lux的量级低了两个数量级，只是让纯黑的室内
				// 能看清形状，不会让白天的室外看起来变化(10万lux面前几百lux的叠加几乎
				// 不可见)，具体数值是占位性质，PIE里看效果不够/太强再调。
				ambientComponent->SetIntensity(300.0f);
				ambientComponent->SetLightColor(FLinearColor(0.8f, 0.85f, 1.0f)); // 偏冷调中性光
				// 这个光源不参与大气/云层渲染("谁是太阳"这件事只应该是sunLight一个)，
				// 显式退出竞争，避免SkyAtmosphere/体积云在两个方向光之间选择困难。
				ambientComponent->SetAtmosphereSunLight(false);
				// ForwardShadingPriority保持默认值0，比上面sunLight显式设的1低——两者
				// 不再相等，不会触发"多方向光竞争forward shading"的引擎警告，体积雾/
				// 半透明/水面这类效果确定性地只认sunLight。
			}
			// 朝向随便给一个固定角度即可——反正不投射阴影，均匀照到所有表面，方向只影响
			// 法线朝向不同的面各自亮度比例这种细微差异，不是关键参数。
			ambientLight->SetActorRotation(FRotator(-45.0f, 0.0f, 0.0f));
		}
	}
	if (!skyAtmosphere) {
		skyAtmosphere = Cast<ASkyAtmosphere>(UGameplayStatics::GetActorOfClass(world, ASkyAtmosphere::StaticClass()));
		if (!skyAtmosphere) skyAtmosphere = world->SpawnActor<ASkyAtmosphere>();
	}
	if (!skyLight) {
		skyLight = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(world, ASkyLight::StaticClass()));
		if (!skyLight) skyLight = world->SpawnActor<ASkyLight>();
	}
	if (!heightFog) {
		heightFog = Cast<AExponentialHeightFog>(UGameplayStatics::GetActorOfClass(world, AExponentialHeightFog::StaticClass()));
		if (!heightFog) heightFog = world->SpawnActor<AExponentialHeightFog>();
		// 默认的ExponentialHeightFog只走"雾因子"那条便宜的非体积路径，光照穿过雾气不会有
		// 真实的体积感(光束/自阴影)。要的是"体积雾"，必须显式打开bEnableVolumetricFog——
		// 这是一次性开关，不随时间变化，放在EnsureSkyGenerated这里，不用每帧在
		// UpdateHeightFog里重复设置。
		if (heightFog) {
			if (UExponentialHeightFogComponent* fogComponent =
				heightFog->GetComponentByClass<UExponentialHeightFogComponent>()) {
				fogComponent->bEnableVolumetricFog = true;
			}
		}
	}
	if (!postProcessVolume) {
		postProcessVolume = Cast<APostProcessVolume>(UGameplayStatics::GetActorOfClass(world, APostProcessVolume::StaticClass()));
		if (!postProcessVolume) {
			postProcessVolume = world->SpawnActor<APostProcessVolume>();
			if (postProcessVolume) {
				// 没有编辑器摆好的包围盒，必须整个世界生效。
				postProcessVolume->bUnbound = true;
				// 老工程这个override开关是在编辑器Details面板手动勾的，这次程序Spawn必须
				// 显式打开，否则下面UpdatePostProcess()对AutoExposureMinBrightness的赋值
				// 不会有任何效果。
					postProcessVolume->Settings.bOverride_AutoExposureMinBrightness = true;
			}
		}
	}
}

void UForeverSkyFrameworkComponent::UpdateSky(const Time& currentTime) {
	EnsureSkyGenerated();

	float chronode = (currentTime.GetHour() * 3600.f + currentTime.GetMinute() * 60.f +
		currentTime.GetSecond() + currentTime.GetMillisecond() / 1000.f) / (24.f * 3600.f);

	UpdateSunLight(chronode);
	UpdateSkyAtmosphere(chronode);
	UpdateHeightFog(chronode);
	UpdatePostProcess(chronode);
}

namespace {
	// 太阳光强度系数(0~1) = relu(-cos(2π·chronode))，也就是太阳仰角的正弦：太阳的俯仰是
	// Lerp(90°, -270°, chronode)，6点/18点正好在地平线上，6~18点亮、正午最亮，18点~6点太阳在
	// 地平线以下时恰好为0，光不会从地下照上来，全程连续。原来用sin(chronode·π)，它在(0,1)里
	// 恒为正，只有午夜那一刻是0，夜里太阳转到地平线以下仍然有几万lux。
	float SunIntensityFactor(float chronode) {
		return FMath::Max(0.0f, -FMath::Cos(2.0f * PI * chronode));
	}

	constexpr float kSunMaxIntensity = 100000.0f; // 正午直射光，lux
}

void UForeverSkyFrameworkComponent::UpdateSunLight(float chronode) {
	if (!sunLight) return;

	UDirectionalLightComponent* sunComponent = Cast<UDirectionalLightComponent>(sunLight->GetLightComponent());
	if (!sunComponent) return;

	sunLight->SetActorRotation(FRotator(FMath::Lerp(90.0f, -270.0f, chronode), 0.0f, 0.0f));
	sunComponent->SetIntensity(SunIntensityFactor(chronode) * kSunMaxIntensity);

	// 原来是5段硬切(if/else if边界处颜色直接跳一截)，改成关键帧+分段线性插值，边界处
	// 颜色连续过渡。关键帧的颜色和大致时间点原样保留原来5段各自的颜色，只是把"整段恒定"
	// 改成"在关键帧之间线性过渡，关键帧本身是原来那段最具代表性的颜色"——0.3~0.7这段
	// 正午保留一小段平台(两个相邻关键帧给同一个颜色)，不是整段都在渐变，光照角度变化
	// 很小的时候颜色也不需要一直变。首尾都是夜间色，0/1在环形时间上是同一个点，首尾
	// 颜色相同保证跨午夜也连续，不需要额外处理wraparound。
	static const TArray<TPair<float, FLinearColor>> colorKeyframes = {
		{0.00f, FLinearColor(0.5f, 0.5f, 0.8f)},  // 夜间
		{0.15f, FLinearColor(1.0f, 0.5f, 0.2f)},  // 凌晨/清晨朝霞
		{0.25f, FLinearColor(1.0f, 0.7f, 0.3f)},  // 清晨过渡
		{0.30f, FLinearColor(1.0f, 0.95f, 0.9f)}, // 正午平台起点
		{0.70f, FLinearColor(1.0f, 0.95f, 0.9f)}, // 正午平台终点
		{0.75f, FLinearColor(1.0f, 0.5f, 0.2f)},  // 黄昏夕阳(与清晨朝霞对称)
		{0.85f, FLinearColor(0.5f, 0.5f, 0.8f)},  // 入夜
		{1.00f, FLinearColor(0.5f, 0.5f, 0.8f)},  // 夜间(=0.00，闭合整个环)
	};
	FLinearColor lightColor = colorKeyframes.Last().Value;
	for (int32 i = 0; i < colorKeyframes.Num() - 1; i++) {
		const auto& cur = colorKeyframes[i];
		const auto& next = colorKeyframes[i + 1];
		if (chronode >= cur.Key && chronode <= next.Key) {
			float segLen = next.Key - cur.Key;
			float t = segLen > 0.0f ? (chronode - cur.Key) / segLen : 0.0f;
			lightColor = FLinearColor::LerpUsingHSV(cur.Value, next.Value, t);
			break;
		}
	}
	sunComponent->SetLightColor(lightColor);
}

void UForeverSkyFrameworkComponent::UpdateSkyAtmosphere(float chronode) {
	if (!skyAtmosphere) return;

	USkyAtmosphereComponent* atmosphereComponent = skyAtmosphere->GetComponent();
	if (atmosphereComponent) {
		float sunIntensity = SunIntensityFactor(chronode) * kSunMaxIntensity;
		atmosphereComponent->SetMultiScatteringFactor(FMath::Clamp(sunIntensity / 50000.0f, 0.1f, 1.0f));
	}
}

void UForeverSkyFrameworkComponent::UpdateHeightFog(float chronode) {
	if (!heightFog) return;

	UExponentialHeightFogComponent* fogComponent =
		heightFog->GetComponentByClass<UExponentialHeightFogComponent>();
	if (fogComponent) {
		// 原来的曲线是白天(chronode=0.5附近)最浓、夜间最淡——反了，夜间的雾应该比白天
		// 更浓。同一个(1-|chronode-0.5|*2)^2形状(1=正午,0=午夜)现在倒过来用：正午权重
		// 最大时要"减"，午夜权重最大时反而没有额外加项，所以改成从夜间基准值往下减，
		// 夜间(chronode=0/1)density最大=0.08，正午(chronode=0.5)density最小=0.05。
		float fogDensity = 0.08f - FMath::Pow(1.0f - FMath::Abs(chronode - 0.5f) * 2.0f, 2.0f) * 0.03f;
		fogComponent->SetFogDensity(fogDensity);
	}
}

void UForeverSkyFrameworkComponent::UpdatePostProcess(float chronode) {
	if (!postProcessVolume) return;

	float globalIntensity = -FMath::Cos(chronode * PI * 2);
	postProcessVolume->Settings.AutoExposureMinBrightness = globalIntensity * 10;
}
