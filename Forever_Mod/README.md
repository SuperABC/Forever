# Forever_Mod

阶段3示例Mod目录,验证"Mod发现/加载/注册(含参数传递)"链路可用。当前有三个mod组,均只
实现`GetId`/`GetType`/`GetName`三个身份接口,不含真正玩法逻辑(留给阶段4对应系统迁移时
补上):

- `Test/Cpp/Xiaohua/`——`PengzhanBuilding`(id `pengzhan`)、`YizhongBuilding`
  (id `yizhong`),均继承`BuildingMod`。
- `Test/Cpp/Yuanshen/`——`YuanshenBuilding`(id `yuanshen`),继承`BuildingMod`。
- `Wxdj/Cpp/Wxdj/`——`WxdjScript`(id `wxdj`),继承`ScriptMod`。
- `Empty/Cpp/Empty/`——21个concept各一个`Empty<Concept>`(id统一为`empty`),纯粹用来验证
  `config.json`按mod id配置的命令行式参数能一路传到mod:重写`ApplyArgs`把收到的参数字符串
  拼进`GetName()`,详见`Forever_UE/Source/Forever/Mod/ForeverModSubsystem.md`。

## 目录结构约定

每个mod组(`Test`、`Wxdj`)是`Cpp/`(每个具体mod一个`.vcxproj`,产出`DynamicLibrary`)+
`Resource/`(资源文件,阶段3为空占位)两部分,一个`.sln`引用该组下所有`.vcxproj`。**不复刻**
旧工程`Forever_Mods/<ModName>/UE/<ModName>/`那样的每mod独立UE壳工程——阶段3不需要。

## 每个mod `.vcxproj`必须遵守的约定

- `ConfigurationType`为`DynamicLibrary`,只配置`Debug|x64`/`Release|x64`两个配置(不像旧
  工程那样保留Win32配置)。
- `IncludePath`只指向`Forever_UE/Source/Dependence`及其`common`子目录,**不指向
  `Forever_UE/Source/Core`**——Mod只依赖Dependence接口层,这也是为什么
  `<Concept>Factory`必须和`<Concept>Mod`同放在Dependence(见
  `Forever_UE/Source/Core/README.md`"实现期修正"一节),不能放在Core。
- `LibraryPath`指向`Forever_UE/x64/<Config>`,`.cpp`里`#pragma comment(lib, "Dependence.lib")`。
- **`RuntimeLibrary`必须和`Forever_UE/Source/Dependence/Dependence.vcxproj`一致**
  (`MultiThreadedDebugDLL`/`MultiThreadedDLL`)——CRT不匹配是"`LoadLibrary`成功但跨DLL调用
  崩溃"的典型原因,阶段3的三个示例mod已经对齐这个设置。
- `OutDir`显式设为`$(ProjectDir)..\x64\$(Configuration)\`,即
  `Forever_Mod/<ModGroup>/Cpp/x64/<Config>/<ModName>.dll`——`Forever_UE/Resource/Config/
  config.json`的`dll_paths`按mod组根目录配置(相对config.json自身路径,如
  `"../../../Forever_Mod/Test"`),由`Config::AddDllPath`递归扫描到这个子目录。

## 每个mod DLL必须导出的三个函数

以`Building`概念为例(其余概念符号名把`Buildings`换成对应复数,参见
`Forever_UE/Source/Core/common/loader.md`的21符号表):

```cpp
extern "C" __declspec(dllexport) void* GetModBuildings();                    // 返回该dll提供的static id列表
extern "C" __declspec(dllexport) void RegisterModBuildings(BuildingFactory*); // 把自己注册进传入的Factory
extern "C" __declspec(dllexport) void FinishModBuildings(BuildingFactory*);   // 注册收尾(目前只调用CleanTemp)
```

`RegisterModBuildings`里注册的creator/deleter必须成对来自同一个mod DLL(如
`[]() -> BuildingMod* { return new PengzhanBuilding(); }`配
`[](BuildingMod* b) { delete b; }`)——宿主的`Factory::Destroy<Concept>`会通过这对函数指针
释放对象,保证分配和释放发生在同一模块,不能宿主直接`delete`一个mod `new`出来的对象。

**mod不需要自己接收参数**——`config.json`里`"building_mods"`数组按id配置的命令行式参数
(如`"pengzhan --density 1.0"`)由`BuildingFactory`在创建实例时自动调用
`instance->ApplyArgs(...)`,`RegisterModBuildings`导出函数签名不需要、也不会带参数;
mod只需要在自己的`<Concept>Mod`子类里重写`virtual void ApplyArgs(const std::string&)`即可
接收,不重写就是默认空实现(忽略参数),详见`Forever_UE/Source/Dependence/README.md`。

**creator lambda必须显式标注`-> BuildingMod*`返回类型**(不能让编译器推导成
`PengzhanBuilding*`)——`<Concept>Factory::CreateFunc`是裸函数指针类型
(`BuildingMod*(*)()`,不是`std::function`,原因见`Forever_UE/Source/Core/README.md`
"实现期修正"),裸函数指针要求签名**完全一致**,没有协变:一个返回`PengzhanBuilding*`的
无捕获lambda只能转换成`PengzhanBuilding*(*)()`,不能直接转换成`BuildingMod*(*)()`,必须在
lambda里显式写`-> BuildingMod*`让返回值在lambda内部完成到基类指针的隐式转换。
