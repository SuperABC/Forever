# ForeverLampFrameworkComponent.h / ForeverLampFrameworkComponent.cpp

## 职责

全图路灯的生成与驱动：`GenerateLamps(map)`把Core(`Map::GetLamps()`)里的每盏灯生成成一个`ALampElement`蓝图Actor；
`UpdateLamps(time, deltaTime)`每帧按游戏时间开关灯，并按玩家相机距离切换每盏灯的近/远。

## 关键设计

- **灯Actor + 全城共享灯杆ISM**：每盏灯仍是一个`ALampElement`蓝图Actor(光源、光斑Decal、近/远切换都在Actor上)，但灯杆网格不再由Actor渲染：
  `RegisterLampMesh`读灯Actor蓝图里的静态网格组件，把它的世界变换排进全城共享的`UInstancedStaticMeshComponent`(每种"灯蓝图类+网格组件名"一个，存在`meshParts`)，并隐藏Actor自己的网格组件、关掉它的碰撞(碰撞用ISM默认值，只关了重叠事件)。
  `GenerateLamps`期间只攒进`pending`，结束时`CommitLampInstances`一次性`AddInstances`；运行时`AddLamp`直接`AddInstance`。多种路灯只要是不同的灯蓝图，就各自得到自己的ISM。
  蓝图类按路径各`LoadClass`一次并缓存(`classByPath`)，加载失败只打一次Warning并跳过这些灯。位置`(x*1000, y*1000, SampleHeight*1000)`，yaw由弧度转度，`AlwaysSpawn`。必须在地形网格生成之后调用(用`SampleHeight`)。
- **每种灯的ISM参数来自基类**：建ISM时读灯Actor的`ALampElement::meshSettings`(`FLampMeshSettings`，见`LampElement.md`)：灯头发光槽/参数名/亮灭值、逐实例剔除距离、是否投影、是否有碰撞。组件自己不再有这些全局参数，同一种灯的所有实例共用。
- **灯头发光**：原先在灯蓝图BeginPlay里对StaticMesh建动态材质、SetBrightness事件里把`EmissiveStrength`设成亮度×100。ISM里实例共用材质，所以改由框架做：每个ISM的灯头槽(`emissiveMaterialSlot`)建一个共享动态材质，昼夜变化时`ApplyEmissive`把参数写成`Lerp(off, on, 亮度)`。蓝图里那两处现在是多余的(不影响结果)，可以删。
  灯材质要支持ISM：`Content/3rdParty/Stylized_street_light/Materials/M_StreetLight`是新建的简单材质(`Used with Instanced Static Meshes`+Nanite)，两个灯材质实例`M_Street_Light_Emission/Pole`改继承它(原来继承引擎glTF插件里的`MI_Default_Opaque_DS`，那个材质不能改、没开ISM)。参数名沿用glTF那套(BaseColorTexture/MetallicRoughnessTexture/NormalTexture/EmissiveTexture/EmissiveFactor/EmissiveStrength/RoughnessFactor/SpecularFactor)。
- **开关灯规则**：游戏时间`hour >= onHour(17) || hour < offHour(7)`为亮(1)，否则灭(0)，两个小时数是`EditAnywhere`。
  只在目标亮度变化时遍历推送(`ApplyBrightness`)；`GenerateLamps`后`currentBrightness`重置为-1，保证新生成的灯下一帧同步。
  运行中新增的灯(`AddLamp`)立即同步当前亮度。
- **灯杆距离剔除**：灯杆ISM用逐实例剔除(`SetCullDistances(0, meshSettings.cullDistance)`，≤0不剔除)，**默认0=不剔除**：灯杆网格开Nanite(2026-11-18用户决定)，Nanite不认最大绘制距离，也不产生传统绘制调用。网格若改回非Nanite，才需要把它设成正数(如100000cm=1km)。背景：按场景图元统计(临时调试命令，已删)实测改ISM前灯杆是21290个独立组件、4.2万个材质段，是空中Draws过万的主要来源。
- **近/远切换**：每`updateInterval`(0.2秒)用玩家相机位置(`PlayerCameraManager::GetCameraLocation`，第一人称/载具都以玩家看到的位置为准)遍历全部灯：
  远处的灯距离<`nearDistance`(40m)调`SwitchNear()`，近处的灯距离>`farDistance`(50m)调`SwitchFar()`，两者之差是滞回，避免在边界反复切换。
  状态就是灯自己的`IsNear()`，组件不另存缓存。新生成的灯默认远。真实光源/Decal的取舍在`ALampElement::RefreshLampEffects`里，见`LampElement.md`。
- **运行时`AddLamp`/`RemoveLamp`**仿`UForeverVegetationFrameworkComponent::AddTree/RemoveTree`，暂无调用方。
- 目前全部灯Actor在生成阶段一次性生成，没有按距离流式生成/销毁Actor(近/远只切换光源和Decal)。`RemoveLamp`不真删ISM实例(删除会重排其它实例的下标)，而是缩成0藏起来，下标不复用。

## 依赖关系

- 依赖：`Element/LampElement`、Core `Map`/`Lamps`、`Time`。
- 被谁依赖：`Framework/ForeverFrameworkActor`(生成阶段`生成路灯模型`、`Tick`里`UpdateLamps`)。

## 待办/后续阶段

- 灯很多时按距离流式生成/回收Actor；遍历全部灯做距离判断可改成空间网格。
- 昼夜亮度渐变、天气(阴雨提前开灯)联动。
