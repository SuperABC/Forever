# basic.cpp

## 职责

`Basic.dll`的**运行时导出入口**：把Basic里所有默认mod(地形/路网/分区/建筑/组合/房间/姓名/调度/工作/组织/脚本/产品/仓库/工坊/线路/站点/载具/资产/应用/小游戏/武器，共21个concept)按concept注册进各自的`Factory`。没有对应的`basic.h`。

## 关键设计

- **Basic是运行时DLL**：编译为`DynamicLibrary`(`Basic.dll`)，由`Config`/`ModLoader`根据`config.json`的`dll_paths`在运行时扫描加载，**不静态链进`Forever.Build.cs`**，只`#pragma comment(lib, "Dependence.lib")`。和`Forever_Mod/*`的外部mod走同一套加载机制，见`Source/Basic/README.md`。
- **每个concept三个导出函数**(`extern "C" __declspec(dllexport)`)：`GetMod<Concepts>()`返回本DLL提供的mod id列表(`static vector<string>`)，`RegisterMod<Concepts>(Factory*)`把creator/deleter(以及`Assign`/`GetPower`/`GetCategory`/`AssignRoads`这类static函数指针)注册进Factory，`FinishMod<Concepts>(Factory*)`是注册完成后的收尾钩子(目前全为空)。
- **creator接收参数字符串**：签名是`(const std::string&)`，目前只有`CheckboardRoadnet`用它(`new CheckboardRoadnet(args)`)，其余忽略；参数传递约定见`CONVENTIONS.md`第5节。
- **`GetModDependencies()`**：每个mod id自己声明依赖的mod id列表。依赖id按字符串匹配、不区分concept(所有mod id都带自己concept前缀，不会撞名)。**不要声明成环**：A依赖B、B又依赖A会让两个都启用后互相锁定，配置界面里哪个都关不掉。会创建`Script`的mod(门/电梯/Scheduler/Job/Organization/Vehicle)依赖`script_empty`，住宅楼的占位房间槽依赖`room_empty`(都在`Forever_Mod/Empty`里)。`ShopStorage`依赖`WarehouseRoom`(`storageType="storage_shop"`，仓库只从房间生成)，`RestaurantStorage`同理依赖`RestaurantRoom`。依赖表只覆盖了需要联动启用的mod，小游戏和各App不引用其他mod，没有列依赖。

  **判定标准**：只有代码里确实点名了另一个mod才算依赖——字符串id或`Xxx::GetId()`(`AssignRoom`/`AssignElevatorCabin`的room、component、script id，`stationMod`/`storageType`/`manufactureTypes`/`productType`/`weaponType`/`ammoType`/`vehicleType`/`scriptModName`，组织按`component_*`/`room_*`类型生成岗位，岗位id，配方里的`product_*`，园区的内部建筑id)；线路按`stationType`匹配站点、仓库按`categories`匹配产品也各保留一条。**依赖方向分两种**：①默认按引用方向，A点名B就是A依赖B(楼→Room/Component/Script，Zone→内部楼，组织→岗位/Component/Room，线路→站点/车辆，Asset→武器/产品)；②**托管型反向**：占用宿主空间、没有宿主就不会出现的依赖宿主——火车站/机场站点只从带`stationMod`的楼生成，所以站点依赖楼(楼没有站点mod只是少个站点；公交站是路边站，不依赖楼)；`*Manufacture`只从房间的`manufactureTypes`生成、`ShopStorage`只从房间的`storageType`生成，所以它们依赖房间。Room/Component只是楼选用的实现，不反向；组件只是个标记类、没有任何引用，不依赖房间(房间由楼指定)；`*Manufacture`依赖自己的房间和产出的Product(不依赖Storage，工坊用内部cache)，`BurgerProduct`依赖配方里的`WheatProduct`/`BeefProduct`。
  **没有列入的软引用**：`Ocean/River`、两个Roadnet、`TrainRoute`会按字符串比较`terrain_mountain`/`terrain_ocean`/`terrain_river`来绕开或穿越，对应地形不存在时只是比较不命中、不影响运行，所以不算依赖(`RiverTerrain`没有山就没有起点，所以依赖`MountainTerrain`)。`milestoneNames`里的`asset_test`/`entity_event`/`job_*`等是`Resource/Story`里的脚本名，不是mod id。
  **闭环**：`Basic`与`Empty`互相只依赖对方的占位id(`script_empty`/`room_empty`)，不引用`Test`/`Wxdj`/`Yuanshen`里的任何id(已全文核对)；`Empty`里20个占位mod互不依赖。
- **每个mod单独一组`.h/.cpp/.md`，文件名=mod id**(如`building_residence_high.*`、`zone_residence_high.*`、`room_shop.*`)，放在对应domain目录下。只有占位mod用`<concept>_basic`(`script_basic`/`scheduler_basic`)。mod id不能等于`<concept>_factory`(如曾经的`component_factory`，已改名`component_manufacture`/`organization_manufacture`)，否则`basic.cpp`里`#include "map/component_factory.h"`会优先命中同目录的这个mod头文件、而不是Dependence的Factory头文件。多个mod共用的辅助函数放`<concept>_common.h/.cpp`(`route_common`/`station_common`/`organization_common`)；各mod自己的`#define`测试配置不共用，各存一份(如`ZONE_KIT_*`在三个园区文件里各一份)，以后各自改。
- **creator是无捕获lambda**(转成裸函数指针)，deleter同理，见`Dependence/*/*_factory.md`的跨DLL约束。
- 新增一个默认mod要改这里三处：依赖表(如有)、`GetMod<Concepts>`列表、`RegisterMod<Concepts>`；还要在`config.json`对应的`<concept>_mods`数组里启用。载具的完整流程见`Forever_UE/Tools/VehicleRig/README.md`。

## 依赖关系

- 依赖：`Dependence`下所有`*_factory.h`、`Basic`下各mod自己的头文件(`<mod id>.h`)。
- 被谁依赖：`Core/common/loader`在运行时按符号名(`GetMod*`/`RegisterMod*`/`FinishMod*`/`GetModDependencies`)`GetProcAddress`调用它。

## 待办/后续阶段

- 无。
