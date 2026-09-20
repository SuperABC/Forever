# Basic 阶段3骨架总览(阶段4前修正:改为DynamicLibrary)

## 职责

为`Source/Dependence`20个`<Concept>Mod`接口各配一个`<Concept>Basic`默认实现占位,和
`Source/Dependence/README.md`的20个concept对照表一一对应(`map/terrain_basic.h`→
`TerrainBasic`……`traffic/vehicle_basic.h`→`VehicleBasic`)。共用这一份文档,不逐个配
`.md`,理由同`ForeverFrameworkComponent.md`已立下的先例。

## 架构修正:Basic是运行时加载的DLL,不是静态库

最初按`Source/Dependence`/`Source/Core`的模式把`Basic.vcxproj`也建成了`StaticLibrary`,这是
错的——核对旧工程`E:\Projects\Forever_UE\Source\Basic\Basic.vcxproj`后确认它的
`ConfigurationType`是`DynamicLibrary`,产出`Basic.dll`,和`Forever_Mod`下的Mod走**完全相同**
的机制:根目录`basic.cpp`用`extern "C" __declspec(dllexport)`导出20组
`GetMod<Concept>`/`RegisterMod<Concept>`/`FinishMod<Concept>`符号,旧工程`config.json`的
`dll_paths`里直接列了UE工程自己的构建输出目录(`.../Forever_UE/x64/Release`),让
`Config`/`ModLoader`把`Basic.dll`当成"内置默认Mod"和其他第三方Mod一起扫描加载——不是
"静态链进主程序、随时随地可调用"的普通静态库。现在已按此修正:

- `Basic.vcxproj`的`ConfigurationType`改成`DynamicLibrary`,`OutDir`不变(仍是
  `Forever_UE/x64/<Config>/`,和`Dependence.lib`/`Core.lib`同目录),`IncludePath`去掉了
  `Source/Core`(Basic和普通Mod一样只依赖`Dependence`,不需要`Core`),新增
  `LibraryPath`指向自己的输出目录(`Dependence.lib`也在那)。
- 根目录`Basic.cpp`不再是空文件,改成和`Forever_Mod/Empty/Cpp/Empty/mod_empty.cpp`同样的
  写法:`#include`20个`<domain>/<concept>_factory.h` + 20个`<domain>/<concept>_basic.h`,
  为每个concept导出`GetMod<Concept>`/`RegisterMod<Concept>`/`FinishMod<Concept>`,用
  `XxxBasic::GetId()`(新加的静态方法,返回和`GetType()`一致的占位字符串,如
  `"asset_basic"`)注册。
- `Forever_UE/Resource/Config/config.json`的`dll_paths`需要加上Basic的输出目录(相对
  `config.json`所在目录是`../../x64/Release`),这样`Config::AddDllPath`才会扫描到
  `Basic.dll`并发现它导出的`GetMod<Concept>`符号。

## 关键设计

- 每个`<Concept>Basic`都是`class XxxBasic : public XxxMod`,重写`GetType()`/`GetName()`两个
  方法返回固定占位字符串(如`"asset_basic"`/`"AssetBasic"`),外加一个静态`GetId()`
  方法(供`Basic.cpp`的`RegisterMod<Concept>`按id注册,和`Forever_Mod/Empty`里
  `EmptyXxx::GetId()`的用法一致),没有构造函数或成员,全部内联实现在头文件里,不配`.cpp`。
- `Basic`和`Forever_Mod`下的Mod一样,**只**`#include`Dependence头文件、只需要
  `Dependence.lib`,不依赖`Core`——这不是本阶段的临时简化,是Basic作为"Mod DLL"这个定位的
  必然要求(和`CONVENTIONS.md`"Mod只link Dependence.lib"的规则完全对齐)。

## 依赖关系

- 依赖:`Source/Dependence/<domain>/<concept>_mod.h`(继承对应`<Concept>Mod`)、
  `Source/Dependence/<domain>/<concept>_factory.h`(`Basic.cpp`的`RegisterMod<Concept>`要用)。
- 被谁依赖:不被任何代码静态`#include`/链接——`Basic.dll`只在运行时被`Config`/`ModLoader`
  按符号名发现、加载,和`Forever_Mod/Test`、`Forever_Mod/Wxdj`处于同一层级,不属于
  `Forever.Build.cs`的编译期依赖。

## 待办/后续阶段

- 阶段4:按系统迁移进度,把对应`<Concept>Basic`从占位实现替换成旧工程
  `Basic/<domain>/<concept>_basic.h`的真正默认内容目录(如`AssetBasic`真正的
  家具/道具等内置资产类型,届时大概率要拆成同domain下多个具体类,一个
  `<Concept>Basic`占位类会变成多个真实类各自`RegisterMod<Concept>`一次),并从共用文档升级
  为独立`.md`。Terrain(阶段4-1)是第一个这样做的,占位`TerrainBasic`已经被真实的
  `OceanTerrain`/`MountainTerrain`替换,详见`map/terrain_basic.md`,不再受这份共用文档覆盖。
- map域(阶段4-1建筑内部布局)是第二个走完阶段4的:`ZoneBasic`/`BuildingBasic`/`RoomBasic`/
  `ComponentBasic`这4个占位类进入populace域时改名成`ResidenceZone`/`ResidenceBuilding`/
  `ResidenceRoom`/`ResidenceComponent`(纯改名，`ResidenceRoom`同时第一次填了
  `isResidential`/`residentialCapacity`真实数据)，详见`Source/Core/populace/populace.md`。
  Roadnet紧随其后,占位`RoadnetBasic`已经被真实的`JingRoadnet`替换,详见`map/roadnet_basic.md`。
- Building/Room/Component这3个概念现在各自不止一个默认mod：除了`ResidenceXxx`，新增了
  商店/工厂（照抄老工程`ShopBuilding`/`FactoryBuilding`及各自的Component/Room）。这几个
  具体类型没有像Terrain的`OceanTerrain`/`MountainTerrain`那样从一开始就合并在同一份
  `<concept>_basic.h/.cpp`里，而是一度按"这个类型是哪种业务场景"拆成
  `building_residence.h`/`building_shop.h`/`building_plant.h`（同名冲突问题见下）这种
  多个文件——后来被要求统一收回成一份`map/<concept>_basic.h/.cpp`（`zone_basic`/
  `building_basic`/`room_basic`/`component_basic`），和Terrain/Roadnet保持同一个组织
  方式，不再拆文件，详见`map/building_basic.md`/`map/room_basic.md`/
  `map/component_basic.md`。拆文件那一版曾经因为`FactoryBuilding`/`FactoryComponent`/
  `FactoryRoom`要避开和`Source/Dependence/map/building_factory.h`等注册表头文件同名冲突，
  临时把文件名起成`building_plant.h`/`component_plant.h`/`room_plant.h`（类名/`GetId()`
  仍然是`Factory`开头）——合并回`<concept>_basic.h/.cpp`之后这个问题自然消失，不需要再
  避让`_factory`后缀。
- 阶段4:确认Mod和Basic在同一个Factory里注册时id冲突如何处理(目前`building_mods`等配置
  数组和`Basic`各自用独立的id空间,还没出现真正的冲突场景)。
