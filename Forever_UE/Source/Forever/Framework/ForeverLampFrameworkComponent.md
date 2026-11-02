# ForeverLampFrameworkComponent.h / ForeverLampFrameworkComponent.cpp

## 职责

全图路灯的生成与驱动：`GenerateLamps(map)`把Core(`Map::GetLamps()`)里的每盏灯生成成一个`ALampElement`蓝图Actor；
`UpdateLamps(time, deltaTime)`每帧按游戏时间开关灯，并按玩家相机距离切换每盏灯的近/远。

## 关键设计

- **每盏灯一个Actor**(不是HISM)：灯头的emissive要在蓝图里对每盏灯的动态材质单独调。蓝图类按路径各`LoadClass`一次并缓存
  (`classByPath`)，加载失败只打一次Warning并跳过这些灯。位置`(x*1000, y*1000, SampleHeight*1000)`，yaw由弧度转度，
  `AlwaysSpawn`。必须在地形网格生成之后调用(用`SampleHeight`)。
- **开关灯规则**：游戏时间`hour >= onHour(17) || hour < offHour(7)`为亮(1)，否则灭(0)，两个小时数是`EditAnywhere`。
  只在目标亮度变化时遍历推送(`ApplyBrightness`)；`GenerateLamps`后`currentBrightness`重置为-1，保证新生成的灯下一帧同步。
  运行中新增的灯(`AddLamp`)立即同步当前亮度。
- **近/远切换**：每`updateInterval`(0.2秒)用玩家相机位置(`PlayerCameraManager::GetCameraLocation`，第一人称/载具都以玩家看到的位置为准)遍历全部灯：
  远处的灯距离<`nearDistance`(40m)调`SwitchNear()`，近处的灯距离>`farDistance`(50m)调`SwitchFar()`，两者之差是滞回，避免在边界反复切换。
  状态就是灯自己的`IsNear()`，组件不另存缓存。新生成的灯默认远。真实光源/Decal的取舍在`ALampElement::RefreshLampEffects`里，见`LampElement.md`。
- **运行时`AddLamp`/`RemoveLamp`**仿`UForeverVegetationFrameworkComponent::AddTree/RemoveTree`，暂无调用方。
- 目前全部灯在生成阶段一次性生成，没有按距离流式生成/销毁Actor(近/远只切换光源和Decal)。

## 依赖关系

- 依赖：`Element/LampElement`、Core `Map`/`Lamps`、`Time`。
- 被谁依赖：`Framework/ForeverFrameworkActor`(生成阶段`生成路灯模型`、`Tick`里`UpdateLamps`)。

## 待办/后续阶段

- 灯很多时按距离流式生成/回收Actor；遍历全部灯做距离判断可改成空间网格。
- 昼夜亮度渐变、天气(阴雨提前开灯)联动。
