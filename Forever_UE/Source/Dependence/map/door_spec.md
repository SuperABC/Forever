# door_spec.h / .cpp

## 职责

`DoorSpec`：一扇门"长什么样、怎么开"的纯数据描述，供`BuildingMod`/`RoomMod`/`ZoneMod`
共用，分别填在`doorSpecs`表（按layout标签`tag`索引）或`ZoneGateSpec.door`里。本身不含
任何UE类型，`mesh`只是一个待`LoadObject`的软路径字符串，和`BuildingMod::lodMaterial`
这类"纯数据，UE层按路径加载"的字段是同一个约定。

## 关键设计

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

## 依赖关系

- 依赖：无（纯值类型，只用`std::string`/`std::vector`）。
- 被谁依赖：`Source/Dependence/map/building_mod.h`（`doorSpecs`表）、
  `Source/Dependence/map/room_mod.h`（`doorSpecs`表）、
  `Source/Dependence/map/zone_mod.h`（`ZoneGateSpec::door`）、
  `Source/Core/map/door.h`（`ResolveDoorSpec`读取这个struct决定要不要放门）。

## 待办/后续阶段

- 没有做瞄准倍率/后坐力一类"手感参数"——门不需要。
- `scriptModName`/`milestoneNames`只有`name`非空时才会被`Map::CreateDoor`读取，`name`
  为空的纯自动门这两个字段即使填了也不会生效，不是bug。
