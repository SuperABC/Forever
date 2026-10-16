# feature_spec.h / .cpp

Dependence/map里所有"feature"的纯数据描述(spec)统一放在这一个文件里：门(`DoorSpec`)、
植被(`TreeLineRequest`/`TreeAreaRequest`)。都是mod填写、Core读取的纯值类型，不含任何UE类型，
资产只是待`LoadObject`的软路径字符串。以后map域再有新的spec也加在这里，按下面的格式各占一节，
不再一个spec一个文件。

## 门(`DoorSpec`)

### 职责

`DoorSpec`：一扇门"长什么样、怎么开"的纯数据描述，供`BuildingMod`/`RoomMod`/`ZoneMod`
共用，分别填在`doorSpecs`表（按layout标签`tag`索引）或`ZoneGateSpec.door`里。本身不含
任何UE类型，`mesh`只是一个待`LoadObject`的软路径字符串，和`BuildingMod::lodMaterial`
这类"纯数据，UE层按路径加载"的字段是同一个约定。

### 关键设计

- **`mesh`为空="只是门洞，不放门"**：这是门系统的核心设计，不是这个struct专属的——
  layout里没有标签的门洞不可选中，永远只是个洞；有标签但查不到对应`DoorSpec`、或者
  查到了但`mesh`为空，同样只是个洞。只有`mesh`非空才会真正创建Core的`Door`实例和UE的
  门组件，见`Core/map/door.h`的`ResolveDoorSpec`。
- **`randomSide`只是"要不要随机"的开关，不存随机结果本身**：具体随机出来的结果（Slide
  往哪个方向滑/Swing铰链装哪一边）存在`Door`实例上（`Door::IsFlippedSide()`），在
  `Map::CreateDoor`创建这扇门的那一刻掷一次硬币决定，之后固定不变——同一扇门以后每次
  开关都用这个固定结果。这个字段只对单扇门（`leaves==1`）生效，双扇门两扇对称镜像，
  没有"选哪一边"的歧义，不读这个字段。
- **`access`用`int`不用`DoorAccess`枚举**：`DoorSpec`是纯数据配置（mod作者在`Layout()`/
  `SetProperty()`里手写的字面值），`Door::DoorAccess`枚举定义在`Core/map/door.h`——
  `Dependence`层不能反向依赖`Core`，所以这里用`int`（0=Open/1=Owner/2=Locked，和
  `Door::DoorAccess`的枚举值顺序保持一致），`ResolveDoorSpec`/`Map::CreateDoor`负责
  转换成真正的枚举。

### 依赖关系

- 依赖：无（纯值类型，只用`std::string`/`std::vector`；文件本身因为植被那一节include了`general_geometry.h`）。
- 被谁依赖：`Source/Dependence/map/building_mod.h`（`doorSpecs`表）、
  `Source/Dependence/map/room_mod.h`（`doorSpecs`表）、
  `Source/Dependence/map/zone_mod.h`（`ZoneGateSpec::door`）、
  `Source/Core/map/door.h`（`ResolveDoorSpec`读取这个struct决定要不要放门）。

### 待办/后续阶段

- 没有做瞄准倍率/后坐力一类"手感参数"——门不需要。
- `scriptModName`/`milestoneNames`只有`name`非空时才会被`Map::CreateDoor`读取，`name`
  为空的纯自动门这两个字段即使填了也不会生效，不是bug。

## 植被(`TreeLineRequest`/`TreeAreaRequest`)

### 职责

植被请求的数据结构：`TreeLineRequest`（沿线段均匀种树）和`TreeAreaRequest`（在一个Quad里随机种树）。
mod只负责"提请求"，Core把请求展开成一棵棵树，UE层统一渲染，见`Source/Core/map/vegetation.md`。

### 关键设计

- **mod-facing的接口挂在已有Mod类里，不新增基类**——`RoadnetMod::AddTreeLine`、
  `BuildingMod::AddTreeArea`都是非虚工具方法，和`RoadnetMod::AddHatch`/
  `BuildingMod::AssignFloor`同一个写法：往mod对象自己的`treeLines`/`treeAreas`里追加，
  Mod类的继承关系和虚函数签名都不变。
- **只记请求，不在mod里展开随机**——每棵树的随机旋转(绕Z轴0~2π均匀)、随机缩放(在请求的
  `scale`基础上乘一个均值1、σ=0.1、夹到[0.8,1.2]的高斯随机数)、"不种在水面/挖洞格子上"的
  过滤全部由Core统一处理，规则只有一份，以后调整不用改任何mod。
- **跨DLL安全**——mod只往自己对象的vector里写，Core只按值拷贝/只读遍历，不跨DLL释放内存，
  符合CONVENTIONS.md §4。
- **`meshPath`是StaticMesh资产软路径**（如`/Game/3rdParty/Mossy_Tree/StaticMeshes/
  mossy-tree.mossy-tree`）——UE层每个mesh一个HISM实例化渲染，蓝图Actor不能这样实例化，
  所以不支持蓝图路径。
- **坐标约定**：都是地图element单位。`TreeLineRequest`是世界坐标；`TreeAreaRequest::quad`沿用
  发出者自己的坐标系——BuildingMod传的是`LayoutTrees()`拿到的building quad(中心是世界坐标、尺寸沿
  楼体局部轴向)，Core展开时绕quad中心旋转`building->GetRotation()`。

### 依赖关系

- 依赖：`map/general_geometry.h`（`Quad`）。
- 被谁依赖：`roadnet_mod.h`、`building_mod.h`、`Source/Core/map/vegetation.h`。
