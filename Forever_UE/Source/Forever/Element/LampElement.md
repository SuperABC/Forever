# LampElement.h / LampElement.cpp

## 职责

`ALampElement`：一盏灯的基类Actor(不只路灯)。Mesh、灯头动态材质、光斑Decal都在Blueprint子类里摆(父类选`LampElement`，路径见
`roadnet_basic.cpp`的`LAMP_BLUEPRINT_PATH`)。C++负责三个状态入口，以及近处真实光源的创建/销毁。由
`UForeverLampFrameworkComponent`生成并驱动。

## 关键设计

- **三个状态入口**：
  - `ApplyBrightness(float)`(C++调用)：昼夜亮度，目前只取0(关)/1(开)。记下亮度，触发蓝图事件`SetBrightness`，再`RefreshLampEffects()`。
  - `SetBrightness(float)`(`BlueprintImplementableEvent`)：**在蓝图里实现**，对灯头材质的动态实例设`EmissiveStrength`
    (资产`M_Street_Light_Emission`的参数)，倍率在蓝图里调。
  - `SwitchNear()`/`SwitchFar()`(`BlueprintNativeEvent`)：玩家走近/走远。C++默认实现=设置`bNear`后`RefreshLampEffects()`，蓝图重写时要调父类。
    `IsNear()`就是当前状态，框架组件直接用它判断要不要切换。
- **近处用真实光源，远处用Decal，二者只生效一个**(`RefreshLampEffects`统一对齐，三个入口都走它)：

  | | 远 | 近 |
  |---|---|---|
  | 亮度>0 | Decal显示、无真实光源 | 真实光源存在、Decal隐藏 |
  | 亮度=0 | 都没有 | 都没有 |

- **真实光源由C++现场创建/销毁**，不在蓝图里预放Light组件(几千盏灯每盏一个已注册的光源组件开销太大)。参数是蓝图Class Defaults里的
  `lights`数组(`FLampLightSettings`：点光/聚光、颜色、强度(cd)、衰减半径、聚光内外锥角、相对位置/旋转、是否投影，默认关投影)，
  一盏灯可以有多个。创建=`NewObject`+Movable+挂到根+`RegisterComponent`，销毁=`DestroyComponent`。`lights`为空=这种灯没有真实光源。
- **Decal是可选组件**：`ULampDecalComponent`(见`LampDecalComponent.md`)，蓝图里按需添加；`RefreshLampEffects`用`GetComponents`找到全部并调`SetLampActive`，
  蓝图里不用写Decal的节点。`BeginPlay`里刷新一次，让Decal初始隐藏。
- 根组件是空`SceneComponent`，Tick关闭(状态只在变化时由框架组件推送)。灯不持有任何Core对象，没有`Init`。

## 依赖关系

- 依赖：`Element/LampDecalComponent`、UE `UPointLightComponent`/`USpotLightComponent`。
- 被谁依赖：`Framework/ForeverLampFrameworkComponent`。

## 待办/后续阶段

- 亮度改为连续值(黄昏渐变)时只需框架组件改推送的值，`RefreshLampEffects`按`brightness>0`判断，光源强度可再乘亮度。
- 近处真实光源数量目前只受`nearDistance`限制，灯很密时可加数量上限(取最近的N盏)。
