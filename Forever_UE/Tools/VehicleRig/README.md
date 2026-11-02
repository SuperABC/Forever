# 载具制作流程（静态网格 → 可驾驶 Chaos 载具）

把一个**静态网格**载具模型（轮子和车身在同一个网格里）做成能被操控、轮子会转的载具。公交（PAZ_652）、货车（ZIL-130）、轿车（Cars_for_Arcade_Demolition_Racing_Games）都是这么做的，以后新载具照这个流程。

## 总览

```
静态网格(UE)  --导出FBX-->  make_vehicle_glb.py  --生成带权重GLB-->  导入成骨骼网格体
   --> 物理资产 --> 载具蓝图(继承AVehicleElement) --> Basic里注册VehicleMod --> config/停车位
```

整个过程**不需要 Blender，也不需要手刷权重**：轮子靠材质和连通性自动识别，由脚本写权重。

## 前置条件（模型要满足）

- 轮胎和轮毂用**不同材质**，且轮胎材质只用在轮胎上（干净的几个轮子）。
- 车头有可区分的材质（散热格栅、前大灯），用来自动判断车头朝向。
- 有几个轮子都行：脚本固定输出 4 根轮骨 `Phys_Wheel_FL/FR/BL/BR`，要求前后各一根轴；后桥双胎会合并成一根骨骼（骨骼放在两胎中间）。
- 备胎如果也用轮胎材质，脚本按“离地”识别并留在 root 上不转。

## 一、导出 FBX（UE 里）

输出日志底部切到 **Python** 模式，执行（改资产路径和文件名）：

```python
import unreal; t=unreal.AssetExportTask(); t.object=unreal.load_asset('/Game/3rdParty/XXX/StaticMeshes/Car'); t.filename='C:/Users/13630/xxx_export.fbx'; t.automated=True; t.prompt=False; t.replace_identical=True; unreal.Exporter.run_asset_export_task(t)
```

## 二、生成带权重的 GLB（本目录脚本）

```bash
python Tools/VehicleRig/make_vehicle_glb.py <export.fbx> <out.glb> --mesh <网格名> --hub-mat <轮毂材质> --tire-mat <轮胎材质> --front-mats <车头材质,逗号分隔> [--drop-in-wheel <材质,...>] [--forward auto|+X|-X|+Y|-Y] [--no-recenter]
```

| 参数 | 含义 |
|---|---|
| `--mesh` | FBX 里网格（Geometry）的名字，和 UE 里静态网格资产同名（`Car`、`zil_130_body_lowpoly`） |
| `--hub-mat` / `--tire-mat` | 轮毂 / 轮胎材质名（UE 材质槽名） |
| `--front-mats` | 车头一侧才有的材质，用来判断车头朝向 |
| `--drop-in-wheel` | 落进轮子圆柱范围内的这些材质的面**直接删除**（比如轮拱内衬片被误当成轮子时用） |
| `--forward` | 默认 `auto`；自动判断错了才手填（UE 坐标下车头方向） |
| `--no-recenter` | 不重新定原点/朝向，保持 UE 里的坐标 |

脚本做的事：

1. 解析二进制 FBX（`fbxparse.py`），读顶点、三角形、材质分组。
2. 轮胎/轮毂材质的面按顶点位置做**连通分块**，每个轮子一块。
3. 直径大于最大值 0.7 倍的轮胎候选里，底部离地低于 0.15 倍直径的是触地轮，其余是备胎（留 root）。
4. 触地轮胎按 x 分成前后两轴、按 y 分左右（车头 +X 时左 = -Y），归到四根骨骼；后桥双胎共用一根，骨骼放在均值位置。
5. 轮毂等小块归最近的轮子；距离超过 0.8 倍轮径的留 root（比如车头灯旁的同材质件）。
6. 其他材质的面落在轮子圆柱里（半径 0.92 倍、y 方向在轮宽内）也归该轮，除非在 `--drop-in-wheel` 里。
7. 车头朝向由 `--front-mats` 材质重心偏移判断，整车旋转成车头 +X，x/y 居中，地面 z=0。
8. 写出 GLB：`root` + 4 根轮骨，每个面只受一根骨骼 100% 权重。

脚本会打印：识别到的轮子数、各骨骼位置和半径、每根骨骼的面数、车身包围盒。**先读这些输出再继续**，要核对：

- `tires: 4 on ground`（或有备胎时的 `N off ground`）；
- 骨骼位置对称、半径合理；
- `tris per bone` 里四个轮子面数接近；
- 脚本打印的轮子相关小块清单里，没有明显不是轮子的东西被归进去（否则用 `--drop-in-wheel` 删或调整）。

坐标约定：UE 里 (x, y, z) 写进 GLB 时为 `(x, z, y)/100`，导入 UE 后还原。骨骼 Z = 轮子半径，原点在车身中心的地面。

## 三、导入 GLB 为骨骼网格体（UE 里）

1. 把 GLB 拖进内容浏览器，对话框设置：
   - 「骨骼网格体」页签：**骨架一项清空**（不要沿用已有骨架，否则骨骼对不上），强制类型选**骨骼网格体**。
   - 材质：Interchange 里 Material Import 选 **Do Not Import**（该项在 GLTF Assets Pipeline 下，找不到就跳过，导入后手动指）。
2. 导入后检查材质槽，**没对上的手动指回原材质**（槽名就是原材质名）。
3. 骨骼编辑器检查：骨骼树是 `root` 下挂 `Phys_Wheel_FL/FR/BL/BR`；逐根点中轮骨按 **E**，拖 **Y 轴（绿）**，对应轮子应绕车轴原地滚动，车身、其他轮子不动；四个轮位都试一次。
   - 骨骼局部坐标 Y 是车轴、Z 是转向轴（`UVehicleWheelAnimInstance` 按这个约定叠加滚动/转向旋转）。

## 四、物理资产（UE 里）

1. 右键骨骼网格体 → 创建 → Physics Asset，默认选项。
2. 打开后**删掉四个轮骨上的刚体**，只保留 `root` 上**一个 Box**：尺寸取脚本输出的车身包围盒（长×宽×高），中心大约在 Z = 高度的一半；Query and Physics。
3. 底盘高度由 Box 底面、轮子半径、悬挂行程共同决定；Box 底面不要低于车身。

## 五、载具蓝图（UE 里）

右键 → 蓝图类 → 父类 **`VehicleElement`**（`AVehicleElement`）。基类已经配好 CarMesh、VehicleMovement、4 个轮子的 WheelClass（前/前/后/后）、CameraBoom、FollowCamera、轮子动画实例、输入，**只填车型特有的值**：

| 位置 | 设置 |
|---|---|
| CarMesh → Skeletal Mesh Asset | 新骨骼网格体 |
| CarMesh → Physics Asset | 新物理资产（骨骼网格体本身关联了就留空） |
| VehicleMovement → Wheel Setups | 4 项只改 **Bone Name**：0 `Phys_Wheel_FL` / 1 `Phys_Wheel_FR` / 2 `Phys_Wheel_BL` / 3 `Phys_Wheel_BR`。**不要增删、不要改顺序，不用改 Wheel Class** |
| VehicleMovement → Mass | 按车重（货车 4000，轿车用默认即可） |
| VehicleMovement → Engine Setup → Max Torque | 基类默认 500（按轿车调）；重车往上加，货车 1800 |
| Class Defaults → Wheels → **Wheel Radius** | 轮子半径，取脚本输出的半径（前后不同取平均）；0 = 沿用默认 35 |
| Class Defaults → Mass → **Center Of Mass Offset** | 重心偏移（车身局部）；高车 Z 取负下移防侧翻，公交 `(0,0,-60)`；全 0 = 不动 |
| CameraBoom | Z 偏移按车高调（轿车 100、货车 130）、Target Arm Length（货车 800） |

`Wheel Radius` 和 `Center Of Mass Offset` 在基类 `BeginPlay` 里应用（用 `SetWheelRadius` 按轮位覆盖，不用为每种车型建轮子蓝图子类）。基类里没有任何车型特判。

## 六、在 Basic 里注册车型（C++）

照 `Source/Basic/traffic/vehicle_basic.{h,cpp}` 里 `VehicleBasic`/`BusVehicle`/`TruckVehicle` 的写法：

1. `vehicle_basic.h/.cpp` 新加一个 `XxxVehicle : VehicleMod`：
   - `GetId()`（`vehicle_xxx`，id 必须 `<concept>_<name>`）、`GetCategory()`（种类，停车位按种类挑车型）、`GetType()`、`GetName()`（static 计数器 + id）。
   - `SetProperty()` 里填：`blueprintPath`（`/Game/.../BP_XxxElement.BP_XxxElement_C`）、`sizeX/Y/Z`（包围盒）、`scriptModName="script_empty"` 和 `milestoneNames`（可驾驶的车复用 `vehicle_basic`，即靠近弹出"上车"）、`exitOffsetX/Y/Z`（下车点，车身局部坐标，Z 默认 100，站不下人会拒绝下车）、`drivable=true`。
2. `basic.cpp` 三处：依赖表里加 `{ XxxVehicle::GetId(), { kScriptEmpty } }`；`GetModVehicles` 的列表；`RegisterModVehicles` 里 `factory->RegisterVehicle(...)`。
3. `Resource/Config/config.json` 的 `vehicle_mods` 里加 `"vehicle_xxx"`。
4. 想让它在停车场刷出来：`Source/Basic/map/room_basic.cpp` 里 `ParkingRoom` 的 `parkingSpots` 种类列表加上这个种类（每个车位在列表里等概率随机挑种类，再从该种类的车型里随机挑一个；该种类没有已启用车型时这个车位本轮落空）。
5. `build.bat` 全量编译（Basic 是运行时 DLL）。重启编辑器。

## 七、测试

1. 重启编辑器，进游戏，到有停车位的建筑里找到这种车，试"上车"和下车位置。
2. 看：W/S 油门刹车、A/D 转向；前轮转向、四个轮子随车速滚动；轮胎陷不陷地、拖不拖底；转弯侧不侧翻；下车点站不站得下人。

## 常见问题

| 现象 | 原因 / 处理 |
|---|---|
| 轮子不转或整车跟着动 | 导入时骨架没清空，沿用了旧骨架；清空后重新导入 |
| 轮子绕偏心转 | 骨骼位置不在轮心：检查脚本输出的骨骼位置 |
| 轮胎陷进地里 | `Wheel Radius` 没填或填小了；已填还陷多半是悬挂被车重压缩（重车）。只改半径，悬挂行程仍读轮子类默认值 |
| 车一出生就散架/弹飞 | 物理资产 Box 尺寸/位置不对，或轮骨上还留着刚体 |
| 蓝图里看不到 `Wheel Radius` 等新变量 | 改了 C++ 头文件后要重启编辑器 |
| 改了 Basic 但游戏里没变 | Basic 是 DLL，要 `build.bat` 编译后重启编辑器 |
| 某个误识别成轮子的竖片/内衬 | 用 `--drop-in-wheel <材质>` 删掉后重新生成 |
| 轮子的小件（灯罩、饰片）也跟着转了 | 它和轮毂同材质、离轮子太近；调整模型材质，或看脚本输出里该块的位置再决定 |
| 直接把蓝图拖进关卡测试 | 能开：半径和重心在 `BeginPlay` 应用；但没有 `vehicle` 实例，脚本选项（上车/驾驶）和下车点不生效 |

## 已知局限

- 轮子半径只有一个值（前后桥半径不同时取平均）。
- 前轮最大转向角、悬挂行程读轮子类（`UVehicleWheelFront/Rear`）默认值，没法按车型改；真需要时仿照 `wheelRadius` 加蓝图变量按轮位覆盖。
- 固定 4 个轮子；飞机是单独的 `APlaneElement`（3 个轮子），见 `Element/PlaneElement.md`。
