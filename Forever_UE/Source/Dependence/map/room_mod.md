# room_mod.h / room_mod.cpp

## 职责

`RoomMod`：一个房间类型的标签(比如"residence"/"office")，以及这类房间具备的**能力声明**(住宅/工位/仓库/工坊)、门和脚本。

## 关键设计

- **不参与地块竞争**：`Room`永远是`Building`在`Layout()`里通过`AssignRoom`/`ArrangeRow`显式创建的，不需要`Assign`/`RandomAcreage`/`GetPower`那套static机制，接口保持`GetType()`/`GetName()`。
- **`SetProperty()`两段式**：具体子类在这里填下面所有字段，不在构造函数里赋值，Core创建完实例后立刻调一次。
- **能力声明字段**：`isResidence`+`residenceCapacity`(床位数)、`isWorkspace`+`workspaceCapacity`(工位数)、`isStorage`+`storageType`(StorageMod id)、`rent`(每日房租，分)、`isManufacture`+`manufactureTypes`(产线类型)。各域按这些字段消费(Populace分配住所、Society分配工位、Industry挂仓库/工坊)。
- **停车位不在这里**：`RoomMod`不声明车位。车位几何来自layout的`parkings`，由`BuildingMod::AssignParking`指定挂到哪个房间(见`building_mod.md`)，Core在`Building::Layout`里生成`ParkingSlot`(见`Core/map/room.md`)；`room_parking`只是语义类型。
- **脚本**：`scriptModName`(默认空=脚本没有ScriptMod)+`milestoneNames`；`milestoneNames`非空才创建`Script`。玩家进出这个房间的enter/leave事件只发给它自己的`Script`，不广播，见`Core/map/map.md`"实体脚本"一节。
- **门**：`doorSpecs`(tag -> `DoorSpec`)，负责`single`/`row`实例化出的房间的门，同一个`RoomMod`类型的所有房间共用这张表，查找规则和建筑门一致，见`feature_spec.h`/`Core/map/door.md`。
- **家具布局 `Layout(const RoomLayoutInfo&)`**：虚函数，默认空(不影响其他room mod)。Core在`Building::AssignRoom`/`ArrangeRow`里、房间位置/朝向/门窗都设好之后调用，`RoomLayoutInfo`给出房间局部系(u沿正面墙、v朝正面)的宽/深/层高(米)和门窗洞(`RoomOpening`：哪面墙、沿墙起止、离地高度、tag，见`feature_spec.md`)。mod里调**非虚**`AddFurniture(FurnitureSpec)`登记摆放(网格软路径+房间局部u/v/z米+yaw度+缩放)，和`BuildingMod::AddDecoration`同一模式；`furniture`成员在mod里，Core只读(换算成世界坐标存到`Room`)，**Core不得清空/改写这个vector**(跨DLL堆，见`CONVENTIONS.md`第4节)。Core在`RandomScope(RandomRegistry::GetInstanceStream(mod))`里调`Layout`，所以`GetRandom`可`--seed`复现。结果随地图存档，读档不再重新布局。
- 带存档钩子，默认不存任何东西，见`common/mod_state.md`。

- **在房间里放物品**(2026-11-26)：`Layout`里除了`AddFurniture`，还有两个同样写法的非虚helper，各写进自己的vector(`placedAssets`/`placedFixtures`)，Core只读：
  - `PlaceAsset(RoomAssetSpec)`：放一件由`AssetMod`生成的普通物品——带物理、能被捡走。要N个独立的物品就调N次(`count`是一件的堆叠数量)。物品mod没启用时Core跳过并打警告。
  - `PlaceFixture(RoomFixtureSpec)`：放一件**没有AssetMod的固定物品**——只有显示网格、弹选项的检测盒和一组脚本描述；不能移动、不做物理模拟，但像家具一样挡人。用它可以让房间里的摆设(床之类)能交互，又不用为每样摆设写一个物品mod。
  - 做成固定物品的东西不要再`AddFurniture`，否则会画两遍。坐标约定和家具完全一样。Core怎么用见`Core/map/room.md`"房间mod放的物品"。

## 依赖关系

- 依赖：`common/mod_state.h`、`map/feature_spec.h`、`map/general_geometry.h`。
- 被谁依赖：`room_factory.h`创建它；`Core/map/room.h`/`building.h`消费它；具体实现在`Source/Basic/map/room_*.h/.cpp`(每个房间类型一组文件)和外部mod里。
