# ForeverSkyFrameworkComponent.h / .cpp

## 职责

昼夜循环：根据`Player::GetTime()`驱动场景里的`DirectionalLight`/`SkyAtmosphere`/
`SkyLight`/`ExponentialHeightFog`/`PostProcessVolume`，太阳旋转/颜色/强度、大气散射、
雾浓度、自动曝光的公式/常量照搬老工程`ChronodeManager`的数学。不对应
`ForeverFrameworkComponent.md`域对照表里的任何一个Framework Actor。

## 关键设计

1. **全部C++驱动**：`AForeverFrameworkActor::Tick()`每帧直接调用`UpdateSky(const
   Time&)`，组件自己不开`TickComponent`（见`ForeverFrameworkActor.md`"Tick"一节"不要
   新开一个Tick入口"的规矩）。
2. **程序化生成，不依赖手动摆放**：地图/建筑/道路全部程序化生成，没有手动美术摆放的
   先例，`EnsureSkyGenerated()`是"找不到(`DirectionalLight`/`SkyAtmosphere`/
   `SkyLight`/`ExponentialHeightFog`/`PostProcessVolume`)就`SpawnActor`一份"，和
   `EnsureMapGenerated()`等"Ensure*Generated幂等保证"同一个模式，不要求在关卡里手动
   摆放任何东西。
3. **`PostProcessVolume`需要显式设置两个字段**：程序`SpawnActor`出来的实例默认
   `bOverride_AutoExposureMinBrightness`为false、有包围盒——必须在
   `EnsureSkyGenerated()`里显式设`bUnbound = true`(没有包围盒，强制对整个世界生效)和
   `Settings.bOverride_AutoExposureMinBrightness = true`(否则`UpdatePostProcess()`
   对这个字段的赋值完全没有任何效果，这是实现时容易漏掉的一步)。

## 昼夜数学（照搬老工程`ChronodeManager`）

- `chronode`(当天进度，`[0,1)`)：`(hour*3600+minute*60+second+millisecond/1000)/86400`，
  在`UpdateSky()`里自己算。
- 太阳：俯仰角`Lerp(90°, -270°, chronode)`(跨360°一圈)；强度`Clamp(Sin(chronode·π),0,1)
  ×100000`；颜色5段硬编码if-else(橙红→橙黄→暖白→橙红→蓝紫)。
- 大气：复用同一个"sunIntensity"公式，缩放进`[0.1,1]`做`MultiScatteringFactor`。
- 雾：以正午(`chronode=0.5`)为中心的抛物线，正午最浓(0.0002)两端(午夜)最淡(0.0001)。
- 后处理：`-Cos(chronode·2π)×10`驱动`AutoExposureMinBrightness`。
- 每帧瞬时重算直接赋值，**没有插值/平滑**——视觉平滑感纯粹来自`chronode`本身连续变化。
- 没有月亮/星星——夜晚效果就是太阳颜色变成蓝紫色那一段，不是独立的第二个光源。
- 没有用`UCurveFloat`——全部硬编码if-else/三角函数。
- `SkyLight`只生成、不随时间更新。

## API差异踩坑提醒

- `ADirectionalLight::GetLightComponent()`返回基类`ULightComponent*`，要
  `Cast<UDirectionalLightComponent>`才能调`SetIntensity`/`SetLightColor`。
- `ASkyAtmosphere`有`GetComponent()`便捷accessor，直接返回`USkyAtmosphereComponent*`。
- `AExponentialHeightFog`**没有**同名的`GetComponent()`便捷accessor，要用
  `GetComponentByClass<UExponentialHeightFogComponent>()`——这是本次调研时确认的一个
  具体API差异，不是两个类设计上应该一致、可以想当然互换的。
- **程序`SpawnActor<ADirectionalLight>()`出来的灯光，Mobility默认是Static，不是
  Movable**——编辑器"放置Actor"面板拖进关卡的`DirectionalLight`默认是Movable，是因为
  走的是一个预设了`Mobility=Movable`的蓝图/模板；`world->SpawnActor<ADirectionalLight>()`
  直接构造没有这层模板，拿到的是`UDirectionalLightComponent`自己的C++默认值(Static)。
  Static灯光的方向是运行时"烘焙"语义，运行时改Transform不会触发重新渲染。
  **`EnsureSkyGenerated()`里灯光一旦确定(不管是找到的还是新Spawn的)，必须显式
  `SetMobility(EComponentMobility::Movable)`**，漏了这一步的现象是"整条链路都在跑
  (日志/数值都对)，但画面上太阳纹丝不动"，容易误判成`UpdateSunLight`的公式/数据有问题。

## 依赖关系

- 依赖：`Source/Dependence/common/utility.h`(`Time`类，只读，不持有)。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`
  (`skyFramework`成员，`Tick()`每帧调用`UpdateSky(*player->GetTime())`)。

## 待办/后续阶段

- 老工程`chronode`的具体数值常数(100000强度、50000缩放基准、0.0001/0.0002雾浓度区间、
  ×10后处理系数)都是照抄过来的占位性质数值，没有针对这个项目的地形/建筑规模重新调过，
  如果视觉效果偏暗/偏亮/雾太浓，优先调这些常数，不需要改公式结构。
- 没有月相/星空系统，以后如果需要可以在这个组件里加一个独立的"月光"`DirectionalLight`
  (夜晚chronode>0.8时启用，白天关闭)，这次范围内不做。
