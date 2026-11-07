# ForeverSkyFrameworkComponent.h / .cpp

## 职责

昼夜循环：根据`Player::GetTime()`驱动场景里的太阳(`DirectionalLight`)、环境光(第二盏`DirectionalLight`)、`SkyAtmosphere`、
`SkyLight`、`ExponentialHeightFog`、`PostProcessVolume`。不对应`ForeverFrameworkComponent.md`域对照表里的任何一个Framework Actor。

## 关键设计

1. **全部C++驱动，不开自己的Tick**：`AForeverFrameworkActor::Tick()`每帧调`UpdateSky(const Time&)`(见`ForeverFrameworkActor.md`"Tick"一节的规矩)。
2. **程序化生成，不依赖手动摆放**：`EnsureSkyGenerated()`幂等，各个Actor先找关卡里现成的，找不到再`SpawnActor`(环境光总是新Spawn)。
3. **每帧瞬时重算直接赋值，没有插值**——视觉平滑纯粹来自`chronode`连续变化(天气缩放的平滑在Weather组件里做)。
4. **没有月亮/星星/`UCurveFloat`**，全部是硬编码的三角函数和关键帧。

## 昼夜数学

- `chronode`(当天进度，`[0,1)`)：`(h*3600+m*60+s+ms/1000)/86400`，在`UpdateSky`里算。
- 太阳：俯仰角`Lerp(90°, -270°, chronode)`；强度=`SunIntensityFactor×100000×weatherLightScale`，`SunIntensityFactor=relu(-cos(2π·chronode))`
  (太阳仰角的正弦，6~18点亮、地平线以下恰好为0，光不会从地下照上来)。颜色按8个关键帧分段HSV插值，0.3~0.7正午平台，首尾同为夜间色。
- 大气：`MultiScatteringFactor=Clamp(sunIntensity/50000, 0.1, 1)`，`sunIntensity`含天气缩放。
- 雾：密度=`(0.08 - (1-|chronode-0.5|*2)^2 * 0.03) * weatherFogScale`，午夜最浓0.08、正午最淡0.05。`bEnableVolumetricFog`在生成时一次性打开。
- **白天系数t**(`DayFactor`)：太阳高度正弦在0.03~0.35之间smoothstep，0=深夜、1=白天。区间必须在日出之后，否则破晓前曝光/环境光就先亮、太阳还没出现天就亮了。

## 夜晚变暗(全部按t插值，`Category="Sky|Night"`，PIE里调)

- **环境光**`ambientLight`：不投影的`DirectionalLight`，光会穿透所有几何体，解决没有窗户的封闭室内(地下室)一片漆黑——`sunLight`/`skyLight`的贡献都按"能看到多少天空"算，封闭空间天然是0，
  所以不能用SkyLight代替。强度=`Lerp(nightAmbientLux=2, dayAmbientLux=300, t)×Lerp(1, weatherLightScale, 0.5)`(天气只压一半，阴天室内不至于太暗)。
  `SetAtmosphereSunLight(false)`，不参与"谁是大气/云层的太阳"。
- **曝光**(`UpdatePostProcess`，单位EV100，越大越暗)：`AutoExposureMinBrightness=Lerp(nightExposureEV=3, dayExposureMinEV=-10, t)`，`Max=dayExposureMaxEV=20`不分昼夜
  (夜里不设上限，室内开灯等亮处才能自动压暗，否则被高灵敏度曝光爆掉)；`LocalExposureShadowContrastScale`夜1.0、昼0.8。
  `nightAmbientLux`和`nightExposureEV`要配合调：环境光越亮，EV要越大才压得住；夜里太亮先加大`nightExposureEV`(每+1暗一档)。
- **SkyLight**：Movable+`SetRealTimeCaptureEnabled(true)`，跟着SkyAtmosphere变暗，不手动改强度。
- `GetCurrentExposureEV()`/`GetReferenceDayEV()`(15)：识别名牌这类固定亮度的世界UI按它换算亮度(`ForeverIdentifyComponent`)。室内夜里实际曝光在Min~Max之间浮动，名牌会偏暗一些。
- `PostProcessVolume`：对找到/生成的体积总是设`bUnbound`、`Priority=100`、三个曝光`bOverride_*`和Lumen刷新速度(`lumenSceneLightingUpdateSpeed`/`lumenFinalGatherUpdateSpeed`，默认3)。
  override不开则赋值无效。夜里仍太亮时先查关卡里是否手放了别的后处理体积或相机后处理覆盖。

## 天气缩放

`SetWeatherScales(lightScale, fogScale)`由`UForeverWeatherFrameworkComponent`每帧写入(已平滑)：太阳强度和大气散射因子乘`lightScale`，环境光只吃一半，雾浓度乘`fogScale`。晴天都是1。见`ForeverWeatherFrameworkComponent.md`。

## 引擎坑

- **代码`SpawnActor`出来的灯光`Mobility`默认Static**(编辑器拖进关卡的才是Movable)。Static灯光运行时改旋转不会重新渲染，现象是"数值/日志全对但太阳纹丝不动"；
  未烘焙的运行时几何体上Static灯光也可能不生效。所以两盏方向光、SkyLight都显式`SetMobility(Movable)`。
- **两个方向光抢forward shading**：`ForwardShadingPriority`默认都是0会报`MultipleDirLightsConflictForForwardShading`，体积雾/半透明/水面会按亮度二选一。
  `sunLight`显式设成1(数值高的赢)，`ambientLight`保持0。
- `ADirectionalLight::GetLightComponent()`返回基类，要`Cast<UDirectionalLightComponent>`；`ASkyAtmosphere`有`GetComponent()`，`AExponentialHeightFog`没有，要`GetComponentByClass<UExponentialHeightFogComponent>()`。

## 依赖关系

- 依赖：`common/utility.h`(`Time`，只读不持有)。
- 被谁依赖：`AForeverFrameworkActor`(`skyFramework`，`Tick()`里`UpdateSky(*player->GetTime())`)；`UForeverWeatherFrameworkComponent`(`SetWeatherScales`)；`ForeverIdentifyComponent`(曝光EV)。

## 待办/后续阶段

- 强度/散射基准/雾浓度区间等常数都是占位值，没针对地形和建筑规模调过，偏暗/偏亮/雾浓时优先调常数，不需要改公式结构。
- 没有月光和星空；以后加月光时夜间`ambientLight`强度可让给月光那盏灯。
