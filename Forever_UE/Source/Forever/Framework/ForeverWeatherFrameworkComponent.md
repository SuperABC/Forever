# ForeverWeatherFrameworkComponent.h / .cpp

## 职责

天气的UE表现层。Core的`Weather`(`Player::GetWeather()`，见`Source/Core/player/weather.md`)只决定当前天气，这里把它
翻译成云量、阳光/雾、树的风、雨雪粒子，并在天气变化时按现实时间平滑过渡。**组件自己不开Tick**，由
`AForeverFrameworkActor::Tick`在`player->Tick`之后调用`UpdateWeather(dt, weather, playerIndoors)`(先广播
`weather_change`事件，再调它，再调`vegetation->UpdateWind`——风的倍数由它先写入)。

## 7种天气的目标参数

`weatherParams`(`EditAnywhere`，下标=`Weather::Type`)，每种一组`FForeverWeatherParams`：云量/云密度、阳光缩放、雾缩放、
风力/风速倍数、降雨/降雪强度。构造函数里的默认值是**粗估，要在PIE里调**：

| 天气 | 云量 | 阳光 | 风力×/风速× | 雨 | 雪 |
|---|---|---|---|---|---|
| 晴 | 0.15 | 1 | 1/1 | 0 | 0 |
| 多云 | 0.55 | 1 | 1/1 | 0 | 0 |
| 阴 | 1 | 0.35 | 1.6/1.2 | 0 | 0 |
| 小雨 | 1 | 0.35 | 1.6/1.2 | 0.4 | 0 |
| 大雨 | 1 | 0.30 | 3/1.5 | 1 | 0 |
| 小雪 | 0.55 | 1 | 1/1 | 0 | 0.4 |
| 大雪 | 0.55 | 1 | 3/1.5 | 0 | 1 |

规格对应：晴天微风(风倍数1=`UForeverVegetationFrameworkComponent`基准)；多云只加云，阳光/风不变；阴天云满、阳光变暗、风大一点；
小雨/大雨=阴天+雨粒子(大雨风大)；小雪/大雪=多云+雪粒子(大雪风大)。

**雾**(`fogScale`，乘到天空组件的基准雾浓度上)：晴1/多云1.2=薄雾(基准不能为0，远处LOD靠它遮)，阴/小雨/小雪2.5=中雾，大雨/大雪5=大雾。下表的雾倍数是旧值，以代码为准。

## 平滑过渡

当前参数每帧按现实时间指数趋近目标(`transitionSeconds`，默认20秒走完约95%)，换天气不突变。**第一帧直接到位**
(新开局/读档不从晴天慢慢变)。

## 光照/雾/风(借力已有组件)

- `UForeverSkyFrameworkComponent::SetWeatherScales(lightScale, fogScale)`：太阳强度(和大气散射因子)乘`lightScale`、环境光只吃一半
  (`Lerp(1,lightScale,0.5)`，阴天室内不至于暗太多)，雾浓度乘`fogScale`。
- `UForeverVegetationFrameworkComponent::SetWindScale(strengthScale, speedScale)`：风力/风速倍数，写进`MPC_Wind`，树的摆动跟着变，
  不需要改材质。

## 云

`EnsureWeatherGenerated`里关卡里有`AVolumetricCloud`就复用，没有就Spawn，材质用引擎自带
`/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst`(`WEATHER_CLOUD_MATERIAL_PATH`)的`UMaterialInstanceDynamic`。
驱动的标量参数名是`cloudCoverageParameter`=`Cloud_GlobalCoverage`、`cloudDensityParameter`=`Cloud_GlobalDensity`
(从引擎材质资产里读出来的名字)。**参数名对不上时只警告一次并列出这个材质实际有的标量参数**，改这两个属性即可。云量的具体效果
(0~1是否对应"晴空~满天")没验证，要PIE里看。

## 雨/雪粒子(Niagara)

- 工程新增依赖：`Forever.uproject`启用`Niagara`插件、`Forever.Build.cs`加`Niagara`模块。
- 雨/雪各一个`UNiagaraComponent`，资产路径`WEATHER_RAIN_SYSTEM_PATH`=`/Game/Asset/Effects/Weather/NS_Rain`、
  `WEATHER_SNOW_SYSTEM_PATH`=`/Game/Asset/Effects/Weather/NS_Snow`。**需要你在编辑器里做**，并暴露两个用户参数：
  `User.Intensity`(float，0~1)和`User.Wind`(vector3，cm/s，水平风速)。资产不存在只警告一次，对应的雨/雪就不显示，不影响别的。
- 每帧把组件移到玩家相机位置(Niagara系统里的发射范围要围着原点)，强度<=0.01就`Deactivate`省性能。
- **室内不下雨雪**：玩家在室内(`Player::GetCurrentRoom()!=nullptr`)时`outdoorFactor`在`indoorFadeSeconds`(1.5秒)内趋近0，出门再回来。
- 风向借用树的风向(`UForeverVegetationFrameworkComponent::GetWindDirection`)，水平风速=`precipitationWindSpeed`(150 cm/s)×风力倍数。

## 控制台命令

`Forever.SetWeather <name>`(PIE里测试)：走和脚本`change_weather`同一条路`Weather::SetByName`(会重置自动换天气的倒计时)，名字非法时提示可选值。

## 依赖关系

- 依赖：`player/player.h`+`player/weather.h`(Core)、`ForeverSkyFrameworkComponent`、`ForeverVegetationFrameworkComponent`、Niagara、
  `AVolumetricCloud`。
- 被谁依赖：`AForeverFrameworkActor`(`weatherFramework`成员，`Tick`里调用)。


## 地面湿润/积雪贴花(2026-11-01)

`UpdateGround`：一个`UDecalComponent`跟随相机(水平按`decalRadius/100`格子对齐，pitch=-90朝下投影)，盒子半径`decalRadius`(15000cm)、半高`decalHalfHeight`。
- 材质路径`WEATHER_GROUND_DECAL_PATH`=`/Game/Asset/Effects/Weather/M_WeatherGround`(**需要你在编辑器里做**)：Deferred Decal，标量参数`Wetness`/`SnowCover`(0~1)。缺失只警告一次。
- `wetness`/`snowCover`向当前雨/雪强度靠拢，积累快消退慢(`wetSeconds`30/`drySeconds`120/`snowSeconds`60/`meltSeconds`180，现实秒)，**不受室内影响**；都≈0时贴花隐藏。
- 材质要自己限制只盖朝上的面(法线Z)，雨=压低粗糙度+变暗，雪=白色覆盖。

## 待办/后续阶段

- 雨雪的音效、闪电、雷声都没做。
- 多云/阴天的云量映射、阳光衰减系数都是第一版粗估。
