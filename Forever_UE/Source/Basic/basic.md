# basic.cpp

## 职责

`Basic.dll`的**运行时导出入口**：把Basic里所有默认mod(地形/路网/分区/建筑/组合/房间/姓名/调度/工作/组织/脚本/产品/仓库/工坊/线路/站点/载具/资产/应用/小游戏/武器，共21个concept)按concept注册进各自的`Factory`。没有对应的`basic.h`。

## 关键设计

- **Basic是运行时DLL**：编译为`DynamicLibrary`(`Basic.dll`)，由`Config`/`ModLoader`根据`config.json`的`dll_paths`在运行时扫描加载，**不静态链进`Forever.Build.cs`**，只`#pragma comment(lib, "Dependence.lib")`。和`Forever_Mod/*`的外部mod走同一套加载机制，见`Source/Basic/README.md`。
- **每个concept三个导出函数**(`extern "C" __declspec(dllexport)`)：`GetMod<Concepts>()`返回本DLL提供的mod id列表(`static vector<string>`)，`RegisterMod<Concepts>(Factory*)`把creator/deleter(以及`Assign`/`GetPower`/`GetCategory`/`AssignRoads`这类static函数指针)注册进Factory，`FinishMod<Concepts>(Factory*)`是注册完成后的收尾钩子(目前全为空)。
- **creator接收参数字符串**：签名是`(const std::string&)`，目前只有`CheckboardRoadnet`用它(`new CheckboardRoadnet(args)`)，其余忽略；参数传递约定见`CONVENTIONS.md`第5节。
- **`GetModDependencies()`**：每个mod id自己声明依赖的mod id列表。依赖id按字符串匹配、不区分concept(所有mod id都带自己concept前缀，不会撞名)。**不要声明成环**：A依赖B、B又依赖A会让两个都启用后互相锁定，配置界面里哪个都关不掉。会创建`Script`的mod(门/电梯/Scheduler/Job/Organization/Vehicle)依赖`script_empty`，住宅楼的占位房间槽依赖`room_empty`(都在`Forever_Mod/Empty`里)。`WarehouseRoom`依赖`ShopStorage`(`storageType="storage_shop"`)。依赖表只覆盖了需要联动启用的mod，小游戏和各App不引用其他mod，没有列依赖。
- **同一concept下多个具体类型合并在同一份`<concept>_basic.h/.cpp`**(如`ResidenceBuilding`/`ShopBuilding`/`FactoryBuilding`都在`building_basic.*`)，不按业务场景拆文件，见`map/`下各md。
- **creator是无捕获lambda**(转成裸函数指针)，deleter同理，见`Dependence/*/*_factory.md`的跨DLL约束。
- 新增一个默认mod要改这里三处：依赖表(如有)、`GetMod<Concepts>`列表、`RegisterMod<Concepts>`；还要在`config.json`对应的`<concept>_mods`数组里启用。载具的完整流程见`Forever_UE/Tools/VehicleRig/README.md`。

## 依赖关系

- 依赖：`Dependence`下所有`*_factory.h`、`Basic`下各concept的`*_basic.h`(以及`roadnet_checkboard.h`、`puzzle_*.h`)。
- 被谁依赖：`Core/common/loader`在运行时按符号名(`GetMod*`/`RegisterMod*`/`FinishMod*`/`GetModDependencies`)`GetProcAddress`调用它。

## 待办/后续阶段

- 无。
