# room_mod.h / room_mod.cpp

## 职责

`RoomMod`：一个房间类型的标签(比如"residential"/"office")，以及这类房间具备的**能力声明**(住宅/工位/仓库/工坊/停车位)、门和脚本。

## 关键设计

- **不参与地块竞争**：`Room`永远是`Building`在`Layout()`里通过`AssignRoom`/`ArrangeRow`显式创建的，不需要`Assign`/`RandomAcreage`/`GetPower`那套static机制，接口保持`GetType()`/`GetName()`。
- **`SetProperty()`两段式**：具体子类在这里填下面所有字段，不在构造函数里赋值，Core创建完实例后立刻调一次。
- **能力声明字段**：`isResidential`+`residentialCapacity`(床位数)、`isWorkspace`+`workspaceCapacity`(工位数)、`isStorage`+`storageType`(StorageMod id)、`rent`(每日房租，分)、`isManufacture`+`manufactureTypes`(产线类型)。各域按这些字段消费(Populace分配住所、Society分配工位、Industry挂仓库/工坊)。
- **停车位**：`isParking`+`parkingSpots`(`ParkingSpot`，见`general_geometry.h`：房间局部坐标位置、旋转、接受的载具种类`categories`)。`Traffic::InitParkedVehicles`遍历所有`isParking`的房间，对每个车位在`categories`里随机挑一个种类、再从该种类的车型里随机挑一个，生成预置车辆，见`Core/traffic/traffic.md`。
- **脚本**：`scriptModName`(默认`script_empty`)+`milestoneNames`；非空才创建`Script`。玩家进出这个房间的enter/leave事件只发给它自己的`Script`，不广播，见`Core/map/map.md`"实体脚本"一节。
- **门**：`doorSpecs`(tag -> `DoorSpec`)，负责`single`/`row`实例化出的房间的门，同一个`RoomMod`类型的所有房间共用这张表，查找规则和建筑门一致，见`feature_spec.h`/`Core/map/door.md`。
- 带存档钩子，默认不存任何东西，见`common/mod_state.md`。

## 依赖关系

- 依赖：`common/mod_state.h`、`map/feature_spec.h`、`map/general_geometry.h`。
- 被谁依赖：`room_factory.h`创建它；`Core/map/room.h`/`building.h`消费它；具体实现在`Source/Basic/map/room_basic.*`和外部mod里。

## 待办/后续阶段

- 无。
