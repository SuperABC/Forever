# Config (config.h / config.cpp)

## 职责

读取`Forever_UE/Resource/Config/config.json`,并递归扫描其中`dll_paths`列出的目录,找出
实现了任意一个已知Mod接口(`GetMod<Concept>`符号)的`.dll`文件,供`ModLoader`真正加载/注册
使用。是旧工程`Core/common/config.h`的`Config`类的阶段3裁剪版。

## 关键设计

- **移植但大幅裁剪**——旧工程`Config`类还管理启用禁用状态(`GetChecks`/`CheckMod`/
  `GetEnables`/`modEnables`)、老式"一个资源目录桶四种后缀分流"的资源目录扫描
  (`GetResourcePaths`/`GetScripts`/`GetPlugins`/`GetPakFiles`/`AddResourcePath`/
  `RemoveResourcePath`)、全局设置(`GetGlobalSettings`)、主剧情路径(`GetStories`/
  `AddScript`/`RemoveScript`)、运行时写回(`WriteConfig`)。这些仍未迁移，等对应机制/系统
  在阶段4落地时再按需加回。
- **`AddLayoutPath`/`GetLayouts`(Building内部布局落地时补回)**：和`AddDllPath`同一个
  `std::filesystem::recursive_directory_iterator`扫描手法，但不探测/加载任何东西
  (`.layout`是纯文本模板文件，不是dll)，扫到就收。`config.json`新增`"layout_paths"`数组，
  相对路径解析规则和`dll_paths`完全一样(相对`configDir`)。`ForeverModSubsystem::
  Initialize`里`Config::GetLayouts()`为空时回退扫描默认的`Resource/Layouts/`目录，和
  `dll_paths`回退扫描`../Forever_Mod`同一个容错风格。不是恢复旧工程"一个资源目录桶四种
  后缀分流"那套`GetResourcePaths`/`AddResourcePath`设计——这次直接一个独立的
  `layout_paths`数组更清楚，也不需要`GetScripts`/`GetPlugins`/`GetPakFiles`这些还没有
  消费方的其它资源类型。
- **用旧工程`Dependence/common/json.h`而不是UE自带Json模块解析`config.json`**——UE的
  `FJsonSerializer`是严格JSON,不支持注释;旧工程的解析器是JsonCpp衍生的宽松版本,支持
  `//`/`/* */`注释,更适合手写维护的配置文件。这是本次会话用户明确要求的决定。
- **`dll_paths`里的相对路径,相对config.json自己所在目录(`configDir`)解析**——纯文件系统
  概念,不依赖UE的`FPaths::ProjectDir`,保持`Config`engine-agnostic。`Forever_UE/Resource/
  Config/config.json`里的`dll_paths`因此写成`"../../../Forever_Mod/Test"`这种相对本文件
  路径的写法,而不是相对仓库根目录或相对可执行文件当前工作目录——后两者在`Config`这层根本
  拿不到。
- **`ReadConfig`找不到文件或JSON语法错误时静默留空,不抛异常**——旧工程`Config::ReadConfig`
  在这两种情况下都会`THROW_EXCEPTION`,但阶段3的约定是"缺配置就用硬编码默认值,不报错"
  (和`UForeverKeyBindingSubsystem`读`KeyBindings.ini`的容错风格一致),由调用方
  (`ForeverModSubsystem`)决定要不要回退到扫描默认目录。
- **`AddDllPath`探测完立即`FreeLibrary`,不长期持有句柄**——探测阶段只是为了确认"这是不是
  一个合法mod dll"(是否存在任意一个`GetMod<Concept>`符号),不需要长期持有;真正常驻加载
  是`ModLoader`(`loader.h`)的职责,`Config`和`ModLoader`各自独立`LoadLibraryA`同一个
  dll两次(一次探测、一次注册)在Windows上是安全的(`LoadLibraryA`内部按路径做引用计数)。
- **`AddDllPath`探测用的20个`GetMod<Concept>`符号名来自`loader.h`的
  `GetModConceptDescriptors()`**,不在`config.cpp`里重复维护一份列表,避免两处列表不同步。
- **`<concept>_mods`数组按mod id配置参数,格式和旧工程一致**——`ReadConfig`扫描所有以
  `_mods`结尾的顶层key(不硬编码20个concept名字,配置文件本身决定内容有哪些),每个数组
  元素是`"id"`或`"id 参数..."`(如`"pengzhan --density 1.0"`,和旧工程
  `"test ---name value"`的写法一致),按**第一个空格**切成`(id, 参数字符串)`存进
  `conceptMods[jsonKey]`。这条路径最初的设计是按`dll_paths`根目录配一份共用参数(通过
  `dll_args`),后来发现和"每个mod id有自己的参数"这个真实需求对不上而改成现在这样——
  `dll_args`概念已经废弃,不要再往`config.json`里加这个key。
- **`GetConceptMods`只做字符串切分,不做任何格式校验/解析**——参数部分(`"pengzhan
  --density 1.0"`里`"pengzhan"`之后的部分)原样返回,是`--key value`风格还是别的格式,
  完全由消费方(mod自己注册的creator函数,创建实例时直接收到这个字符串)决定。`Config`/
  `ModLoader`全程不关心参数内容,只负责"从配置文件读出来、按id传给对的Factory"这件事,详见
  `Source/Dependence/README.md`里`<Concept>Factory::SetModArgs`的说明。

## 依赖关系

- 依赖:`Dependence/common/json.h`(解析JSON)、`Core/common/loader.h`
  (`GetModConceptDescriptors()`探测符号表)、`<windows.h>`(`LoadLibraryA`/`GetProcAddress`/
  `FreeLibrary`,只在`.cpp`里出现,头文件不引入)。
- 被谁依赖:`Source/Forever/Mod/ForeverModSubsystem.cpp`。

## 待办/后续阶段

- 阶段4:按需加回启用禁用状态、`GetScripts`/`GetPlugins`/`GetPakFiles`、全局设置、剧情
  路径、运行时写回,恢复`config.json`里对应的key(`layout_paths`已经在Building内部布局
  落地时补回，见上)。
- 阶段4:如果发现同一个dll在多次`AddDllPath`调用(如`Debug`/`Release`两个目录都被扫描到)
  下被重复记录,需要处理去重——目前`GetMods()`已经按dll绝对路径去重,但如果同一个mod id
  被两个不同dll路径各自注册一次,`ModLoader::RegisterConcept`会调用两次
  `RegisterMod<Concept>`,后一次覆盖前一次,这是已知的、可接受的阶段3简化行为。
