# Forever_Mod

外部Mod目录，每个子目录是一组独立的纯Win32 DLL mod，运行时由`Config`/`ModLoader`按`config.json`的`dll_paths`发现并加载：

- `Test/`——测试mod组，详见`Test/Cpp/Yuanshen/building_yuanshen.md`、`Test/Cpp/Test/script_test.md`、`Test/Resource/README.md`：
  - `Test/Cpp/Yuanshen/`(`Yuanshen.dll`)：`YuanshenBuilding`(id `building_yuanshen`，中心lot里自适应贴角的6x6、14层)和空壳`YuanshenScript`(id `script_yuanshen`)，配`Test/Resource/ys.script`。
  - `Test/Cpp/Test/`(`Test.dll`)：`TestScript`(id `script_test`)，把`Test/Resource/test.script`的`control`占位符换成控制随机市民。
  - `Test/UE/Test/`：迷你UE工程，用于制作载具蓝图等UE资产并打成pak(见下文)。
- `Wxdj/`——五行斗诀：`WxdjBuilding`(id `building_wxdj`，中心lot唯一的6x6三层楼)，没有ScriptMod(`main_story`用`script_empty`)，剧情全在`Wxdj/Resource/Wxdj.script`；另有`WxdjPuzzle`(id `puzzle_wxdj`，回合制战斗小游戏，战斗内核在`Wxdj/Cpp/Wxdj/battle/`，招式表`Wxdj/Resource/act/xiaohua.txt`)，开局由脚本`start_puzzle`触发。
- `Empty/Cpp/Empty/`——20个concept各一个`Empty<Concept>`(id为`<concept>_empty`)，用来验证`config.json`按mod id配置的命令行式参数能一路传到mod：构造函数直接接收参数字符串，拼进`GetName()`，详见`Forever_UE/Source/Forever/Mod/ForeverModSubsystem.md`。

## 目录结构约定

每个mod组(`Test`、`Wxdj`、`Empty`)是`Cpp/`(每个具体mod一个`.vcxproj`,产出`DynamicLibrary`)+
`Resource/`(资源文件)两部分,一个`.sln`引用该组下所有`.vcxproj`。需要UE资产的mod组另带一个迷你UE工程(`Test/UE/Test`)。

## 每个mod `.vcxproj`必须遵守的约定

- `ConfigurationType`为`DynamicLibrary`,只配置`Debug|x64`/`Release|x64`两个配置(不像旧
  工程那样保留Win32配置)。
- `IncludePath`只指向`Forever_UE/Source/Dependence`及其`common`子目录,**不指向
  `Forever_UE/Source/Core`**——Mod只依赖Dependence接口层,这也是为什么
  `<Concept>Factory`必须和`<Concept>Mod`同放在Dependence(见
  `Forever_UE/Source/Core/README.md`),不能放在Core。
- `LibraryPath`指向`Forever_UE/x64/<Config>`,`.cpp`里`#pragma comment(lib, "Dependence.lib")`。
- **`RuntimeLibrary`必须和`Forever_UE/Source/Dependence/Dependence.vcxproj`一致**
  (`MultiThreadedDebugDLL`/`MultiThreadedDLL`)——CRT不匹配是"`LoadLibrary`成功但跨DLL调用
  崩溃"的典型原因。
- `OutDir`显式设为`$(ProjectDir)..\x64\$(Configuration)\`,即
  `Forever_Mod/<ModGroup>/Cpp/x64/<Config>/<ModName>.dll`——`Forever_UE/Resource/Config/
  config.json`的`dll_paths`按mod组根目录配置(相对config.json自身路径,如
  `"../../../Forever_Mod/Test"`),由`Config::AddDllPath`递归扫描到这个子目录。

## 每个mod DLL必须导出的三个函数

以`Building`概念为例(其余概念符号名把`Buildings`换成对应复数,参见
`Forever_UE/Source/Core/common/loader.md`的20符号表):

```cpp
extern "C" __declspec(dllexport) void* GetModBuildings();                    // 返回该dll提供的static id列表
extern "C" __declspec(dllexport) void RegisterModBuildings(BuildingFactory*); // 把自己注册进传入的Factory
extern "C" __declspec(dllexport) void FinishModBuildings(BuildingFactory*);   // 注册收尾(目前空实现,留作Hook)
```

`RegisterModBuildings`里注册的creator/deleter必须成对来自同一个mod DLL(如
`[](const std::string&) -> BuildingMod* { return new PengzhanBuilding(); }`配
`[](BuildingMod* b) { delete b; }`)——宿主的`Factory::Destroy<Concept>`会通过这对函数指针
释放对象,保证分配和释放发生在同一模块,不能宿主直接`delete`一个mod `new`出来的对象。

**参数直接交给creator,不经过任何"创建后"钩子**——`config.json`里`"building_mods"`数组
按id配置的命令行式参数(如`"pengzhan --density 1.0"`)由`BuildingFactory::CreateBuilding(id)`
在创建实例那一刻,作为参数直接传给注册的creator函数;`RegisterModBuildings`导出函数签名
不需要、也不会带参数——参数只在creator被调用时才出现。mod的creator决定怎么用这个参数
(最常见的做法是转发给具体子类的构造函数,如`[](const std::string& args) -> BuildingMod* {
return new PengzhanBuilding(args); }`),不用就直接忽略这个参数,详见
`Forever_UE/Source/Dependence/README.md`。**不要**改成创建实例之后再调`instance->ApplyArgs(args)`
这类钩子：构造函数已经跑完，`const`成员无法再赋值，参数到手太晚。

**creator lambda必须显式标注`-> BuildingMod*`返回类型**(不能让编译器推导成
`PengzhanBuilding*`)——`<Concept>Factory::CreateFunc`是裸函数指针类型
(`BuildingMod*(*)(const std::string&)`,不是`std::function`,原因见
`Forever_UE/Source/Core/README.md`),裸函数指针要求签名**完全一致**,没有
协变:一个返回`PengzhanBuilding*`的无捕获lambda只能转换成
`PengzhanBuilding*(*)(const std::string&)`,不能直接转换成
`BuildingMod*(*)(const std::string&)`,必须在lambda里显式写`-> BuildingMod*`让返回值在
lambda内部完成到基类指针的隐式转换。

## 用mod提供载具(含蓝图)

**结论**：mod可以带自己的载具蓝图，父类留在主程序里即可。已验证：编辑器PIE下公交从`/Test/Bus/Actor/BP_BusElement`正常生成、上车、驾驶；打包版挂载新pak未实测(挂载代码和已有的飞机静态网格同一条路径)。

**边界**(原因：安装版引擎只支持单体Game构建，见`Forever_UE/Source/Forever/Element/PlaneElement.md`与CONVENTIONS)：
- 能放进mod的：`<Name>Vehicle`(纯C++ `VehicleMod`，Dependence接口)、脚本、模型/材质/物理资产/骨架、**载具蓝图**(父类`AVehicleElement`或其蓝图子类在主程序里)。
- 必须留在主程序的：任何新的UE C++类(`AVehicleElement`、`ATransitVehicleElement`、`APlaneElement`、`UVehicleWheelAnimInstance`、键位、相机……)。新载具如果需要新行为(如飞机的飞行力学)，主程序里要先有对应的类，mod只是继承它做蓝图。

**让迷你工程(以`Forever_Mod/Test/UE/Test`为例)能创建/保存父类在主程序里的蓝图**——迷你工程原本没有Forever模块，打开这类蓝图会找不到`/Script/Forever.VehicleElement`：
1. `Test.uproject`加`"Modules": [{ "Name": "Forever", "Type": "Editor", "LoadingPhase": "Default" }]`，`Plugins`里补上和主工程一致的`Water`、`Niagara`、`ChaosVehiclesPlugin`、`ChaosModularVehicleExamples`(用Python移资产的话再加`PythonScriptPlugin`)。
2. 新建`Source/Test.Target.cs`(Game)、`Source/TestEditor.Target.cs`(Editor，`ExtraModuleNames.Add("Forever")`)、`Source/Forever/Forever.Build.cs`：依赖模块列表照抄主工程`Forever.Build.cs`，但路径不能用`ModuleDirectory`相对推算，要写到主工程`Forever_UE/Source`的绝对/相对路径(`Dependence`、`Core`、`Core/common`头文件目录，`Forever_UE/x64/Release/{Dependence,Core}.lib`，所以先编过`Framework.sln`)。`Source/Forever/Forever.cpp`第一行必须`#include "Forever.h"`(UHT要求)，内容`IMPLEMENT_MODULE(FDefaultGameModuleImpl, Forever);`，`Forever.h`拷主工程的。
3. 源码：把主工程`Source/Forever/{Controller,Element,Framework,Input,Mod,Player,UI}`下所有`.h/.cpp`在迷你工程`Source/Forever/`里建**NTFS硬链接**(PowerShell `New-Item -ItemType HardLink`)，同一份代码、不会分叉。**不能用目录junction**：UBT不扫描junction，模块只会编出一个空的Forever.cpp。主工程新增文件要补建链接；用"替换文件"方式保存的工具会断开硬链接，断了要重建。
4. 把主工程`Config/DefaultEngine.ini`的`[/Script/Engine.CollisionProfile]`整段追加到迷你工程的`DefaultEngine.ini`，否则cook报"碰撞描述文件设置不包括水体碰撞描述文件的条目"，commandlet以Failure收场(pak.bat判失败并先删掉旧pak)。
5. 编译：`Build.bat TestEditor Win64 Development -Project=...\Test.uproject -WaitMutex -FromMsBuild`(要先关掉打开着的编辑器，Live Coding开着会拒绝)。

**把主工程里已有的载具蓝图搬进mod**(以公交为例)：
1. 把蓝图的依赖闭包拷进迷你工程`Content/`，路径和主工程`/Game/...`一致(公交：`3rdParty/PAZ_652`整个文件夹，加上父蓝图`Blueprint/Vehicle/BP_VehicleElement.uasset`)。依赖可以用`strings`式扫描uasset里的`/Game/...`得到。
2. 父蓝图(`BP_VehicleElement`)留在迷你工程`Content/`里就行，**不要**放进`Plugins/<Mod>/Content`：它不会进pak，运行时用的是主程序里的那份，子蓝图引用的仍是`/Game/Blueprint/Vehicle/BP_VehicleElement`。
3. 用迷你工程的`UnrealEditor-Cmd.exe Test.uproject -ExecutePythonScript=move.py -unattended`把子蓝图和它的网格/材质/贴图一次性搬到`/<Mod>/<Name>/...`。**必须用`AssetToolsHelpers.get_asset_tools().rename_assets([AssetRenameData(asset, 新文件夹, 新名字), ...])`整批改名**；逐个调`EditorAssetLibrary.rename_asset`会让蓝图里网格引用丢成None(先搬蓝图再搬网格，蓝图已存盘的引用不会被改写)。搬完用Python加载蓝图，检查`CarMesh`的`skeletal_mesh`指向新路径、`WheelSetups`的骨骼名都在。
4. 主程序里`<Name>Vehicle::SetProperty`的`blueprintPath`改成`/<Mod>/<Name>/Actor/BP_xxx.BP_xxx_C`。编辑器PIE下`ForeverModSubsystem::EnsureModsRegistered`直接挂载插件`Content`目录(不用cook)；打包版跑mod自己的`pak.bat`(CookAll，实测约7分钟，pak因为带了公交贴图从22MB涨到131MB)。

**无害的现象**：迷你工程的commandlet日志里有大量`CDO Constructor (...): Failed to find /Game/...`(主工程的IA_*、材质、网格不在迷你工程里)，只是类的默认对象里引用为空；蓝图只存与父类默认值的差异，所以不影响搬进来的蓝图。名字末尾带`_652`这类数字的资产，在uasset名字表里会被拆成`SKM_paz`+数字后缀，扫描依赖时别被误导。

**未做**：`BusVehicle`这个C++类本身没搬进`Forever_Mod`(它是纯C++ mod，搬法和`YuanshenBuilding`一样，没有技术障碍)；线路公交用的`transitMeshPath`、脚本也没搬。

## 图片资源约定

mod自带的图片放在`<ModGroup>/Resource/`(内置Basic放`Forever_UE/Resource/Mod/Basic/`)，**文件名必须以所属puzzle的id加下划线开头**(如`puzzle_anykey_icon.png`、`puzzle_dodge_bg.png`)——图片靠`Canvas`的`LoadModImage(post, 文件名, bitmap)`按文件名在`config.json`的`resource_paths`里递归查找，前缀保证全局唯一。支持PNG/JPEG/BMP/GIF/TGA等常见格式(stb_image)，详见`Forever_UE/Source/Dependence/player/canvas.md`。
