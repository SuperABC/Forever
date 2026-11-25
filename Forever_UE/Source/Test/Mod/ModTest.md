# ModTest

## 职责

给**mod作者**用的mod自测程序(`x64/Release/ModTest.exe`)。只依赖Dependence(头文件+`Dependence.lib`)，不需要Core，也不需要UE——
mod作者手里只有这些。它把mod dll加载进来，像游戏那样注册进23个`<Concept>Factory`，逐个创建mod实例跑检查。

和CoreTest的分工：

- **ModTest**：测mod本身。mod作者写完mod跑一遍，通过了再提交。
- **CoreTest**(`Source/Test/Core`)：测Core。要完整的Core和一套能生成世界的mod，是项目自己用的。它的mods套件只查"mod接进Core之后"的事。

## 流程

1. **mod作者**：写好mod → 对自己的dll跑ModTest(引用/依赖了别人的mod就用`--with`带上那些dll) → 全部通过 → 提交。
2. **项目这边**：定期收集新提交的mod → 把**全部**mod dll一起交给ModTest跑一遍 → 通过后才开放给玩家。
   **mod id重名只能在这一步查出来**：作者手里没有别人的mod，拿不到别人的id。

## 用法

```
ModTest <mod.dll|目录>... [--with <mod.dll|目录>]... [--config config.json] [--seed N] [--out 目录]
```

- `<mod.dll|目录>`：要测的mod dll。给目录就递归扫描里面的dll(跳过带`Debug`的目录段；不导出任何`GetMod<Concepts>`的dll不是mod，忽略)。
- `--with`：只加载、不测的dll。它们的mod id用来解析被测mod的引用和依赖声明，也参与重名检查。
- `--config`：从这份`config.json`的`"<concept>_mods"`数组里取每个mod的参数(没写的mod参数为空串)；没给任何dll/目录时，
  测它的`dll_paths`下的全部dll。
- `--seed`：mod随机流的种子，默认11。
- `--out`：把报告写成`modtest.json`/`modtest.txt`。
- 退出码：0=全部通过，1=有检查失败，2=参数不对。

例子：

```
rem mod作者：测自己的mod，它引用了Basic和Empty里的mod
ModTest MyMod.dll --with Basic.dll --with Empty.dll

rem 项目这边：全量(Basic + Forever_Mod下所有mod)，重名检查覆盖全部
ModTest x64\Release ..\Forever_Mod

rem run_tests.bat里的那一步：按config.json的dll_paths和参数
ModTest --config Resource\Config\config.json --out Saved\TestReports\<时间>\mod
```

## 检查项

对加载进来的**全部**dll(被测的和`--with`的)：

- **mod id全局唯一**：不管属于哪个concept、由哪个dll提供，两个mod不能用同一个id。随机流、依赖声明、脚本的`mod_dependences`
  都只按id找mod，重名就会串。失败信息会列出这个id被哪几个"dll / concept"提供。

对每个被测的dll：

- 能`LoadLibrary`、是mod dll(命令行直接点名的文件才报，目录里扫到的非mod dll直接忽略)。
- 导出`BindRandomHost`(没有的话这个dll里的`GetRandom`不受种子控制，见`Dependence/common/random.h`)。
- 至少提供一个mod。
- `GetModDependencies`声明的依赖：声明方必须是这个dll自己提供的id；被依赖的id必须由加载进来的某个dll提供(否则用`--with`带上)。
- **依赖不能成环**(含自己依赖自己)：配置界面把"被某个已启用的mod依赖"的mod锁成不能取消，成环的几个mod一旦都启用就互相锁死。
  环按加载进来的全部dll的声明来找，失败信息里列出`a -> b -> a`。

对每个被测的mod：

- **id合法**：非空、不含空白和控制字符。`config.json`里每个条目是`"id 参数..."`，按第一个空格切开，id里有空白就配不出来。
  除此之外不限制格式(`<concept>_<name>`只是弱约定)。
- `GetMod<Concepts>`列出的id确实被`RegisterMod<Concepts>`注册了，并且能创建出实例。
- `UnitTest`返回true(在mod自己的随机流里跑，和Core包装类调mod虚函数时一样)，不抛异常。
- `UnitTest`期间没有漏包随机(没有在任何`RandomScope`之外调`GetRandom`)。
- 连建两个实例，`GetName()`不同(名字全局唯一的约定见`CONVENTIONS.md`第5节)。`Empty.dll`里的占位模板名字固定，不查这条。
- **按concept的字段检查**(`mod_checks.cpp`的`ValidateConcept`)：
  - 车辆：尺寸为正、载客量不为负、能搭乘的车载客量不为0、可驾驶的车要有蓝图或网格路径。
  - 产品：价格满足 零售 > 进口 > 出口 > 0、批量为正、原料/副产品有类型和正数量。
  - 建筑：国有概率在[0,1]、电梯楼层范围不颠倒。园区：内部建筑要有类型。门：有名字(会建脚本)就必须填`scriptModName`。
  - 站点：声明了端口就必须有`stationType`。
  - 房间：容量/房租不为负、住宅有床位、工位房有工位；家具布局见下。
  - 仓库：容量不为负、至少能取或能放。工坊：目标有产品类型和正的批数。地形：`SetupTexture`后有地图颜色。武器：弹匣/射程/缩放为正
    (弹匣和射程都是0的视为不开火的工具，不查)。
- **引用的mod必须存在**(检查名`referenced mods are provided by a loaded dll`)：mod在字段里写了别的mod的id就是引用，
  被引用的id必须由加载进来的某个dll提供(写错id、忘了用`--with`带dll都在这里暴露)。能看到的引用：
  - 脚本：`scriptModName`。只在Core真的会建Script时才算——园区/建筑/房间/电梯是`milestoneNames`非空，门是`name`非空，
    岗位/调度/组织/资产/武器/载具是`scriptModName`或`milestoneNames`任一非空。会建而`scriptModName`留空时Core用默认
    ScriptMod(`Dependence/story/script_mod.h`的`kDefaultScriptModId`，即`script_empty`)，按引用了它算。
  - 园区：内部建筑的类型、大门的脚本。建筑：`AssignRoom`/`ArrangeRow`/`ArrangeRowEach`用到的房间和组件、`stationMod`、
    电梯和门的脚本。这两个concept的引用是`Layout`里填的，ModTest拿8块样例地块各`Layout`一次把引用并起来
    (没被抽到的分支看不到)。
  - 房间：`storageType`(`isStorage`时)、`manufactureTypes`(`isManufacture`时)、门的脚本。
  - 组织：`GetRequirements`要求的组件；岗位类型——拿要求的组件×加载进来的全部房间类型各调一次`DesignJobsForRoom`，
    排出来的岗位类型都算。
  - 资产：`weaponType`、`productType`。武器：`ammoType`。线路：`vehicleType`。
  - 产品：`ingredients`/`byproducts`的产品类型。工坊：`targets`的产品类型。仓库：`productDemand`的产品类型。

  **只查"存在"，不查"有没有声明依赖"**：引用方向不等于依赖方向。楼用`stationMod`点名站点、房间用`storageType`/
  `manufactureTypes`点名仓库和工坊，但语义上是站点依赖楼、仓库和工坊依赖房间(它们只从宿主生成)。谁依赖谁由作者按语义
  写在`GetModDependencies`里，测试推不出来。
- **房间家具布局**：对12种尺寸×8轮调`Layout`。每件家具的锚点必须在房间里、yaw是90度的倍数。家具的模型名在内置尺寸表
  (`Basic/map/room_furniture_catalog.h`)里查得到时再查占地：整件在房间内、互不重叠、吊柜之间不重叠、不压门前、不挡窗。
  尺寸表里没有的家具(mod自带的模型)只查锚点，报告里记一条`note:`。

环境变量`FURNITURE_DUMP=<路径>`时把一个房间mod每个样本的布局写成json给`Tools/Furniture/preview.html`看；`FURNITURE_DUMP_MOD`
指定导出哪个mod(不设是`room_residence`)。

## 关键设计

- **只依赖Dependence**：工程只链接`Dependence.lib`，include路径没有`Core`。所以它不能用Core的`Config`/`ModLoader`/`Registry`，
  自己做了一份很小的加载：`LoadLibrary`、按`Dependence/common/mod_concepts.h`的符号表`GetProcAddress`、把随机host交给dll
  (`BindRandomHost`)、调`RegisterMod<Concepts>`。23个Factory是ModTest自己的实例(`ModTestHost`)。
- **发现的mod一律视为启用**：ModTest把所有发现的id都放进Factory的参数表(参数来自`--config`或空串)，不受`config.json`里
  启用/禁用的影响。`--seed`参数像游戏里一样被剥掉，不传给mod的构造函数。
- **引用按"加载进来的id集合"校验**，不是按"是否启用"：作者测单个mod时，把它引用的mod所在的dll用`--with`带上即可。
- **引用是"收集再统一核对"**：各concept的`ValidateConcept`只管用`Refer`/`ReferScript`/`ReferDoor`把看到的id记进
  `ModOutcome::references`，`RunMod`最后由`CheckReferences`统一判"有没有dll提供"。新加一个引用字段只要多写一行`Refer`。

## 依赖关系

- 依赖：`Dependence`(各`<concept>_mod.h`/`<concept>_factory.h`、`common/mod_concepts.h`、`common/random.h`、`common/json.h`)、
  `Source/Test/Core/test_framework.*`、`Basic/map/room_furniture_catalog.h`(只是一张内置家具尺寸表)。
- 被谁依赖：`Source/Test/run_tests.bat`(`core`管线先跑ModTest再跑CoreTest)。

## 文件分工

- `main.cpp`：命令行、dll扫描和加载、id合法性/重名/依赖声明/依赖成环检查、逐mod驱动、汇总和报告。
- `mod_checks.h/.cpp`：`ModTestHost`(23个Factory和每个concept的操作表)、各concept的字段检查和引用收集、引用核对、
  房间家具布局检查、`RunMod`。

## 待办/后续阶段

- 新增concept时：`AllFactories`加一个成员、`ModTestHost`构造函数里加一行(启动时会核对和`mod_concepts`的表有没有对上)，
  需要字段检查就加一个`ValidateConcept`重载。新增引用别的mod的字段时：在对应的`ValidateConcept`里加一行`Refer`。
- 打包给mod作者的SDK(Dependence头文件+`Dependence.lib`+`ModTest.exe`+`Basic.dll`/`Empty.dll`)还没有脚本，目前要手工拷。
- 引用检查只覆盖"字段里写出来的id"。写在mod代码判断条件里的id(组织的`if (roomType == "room_shop")`)查不到。
- 不检查依赖声明是否完整(2026-11-25试过"引用了就必须声明依赖"，因为引用方向不等于依赖方向而撤掉)。
