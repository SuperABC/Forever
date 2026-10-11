# Config (config.h / config.cpp)

## 职责

读取/维护`Forever_UE/Resource/Config/config.json`的静态类：`dll_paths`/`layout_paths`/
`resource_paths`三类根目录扫描、每个concept的mod参数与启用状态、主线剧情脚本路径与
Story screen的`.script`列表，并能把当前内存状态写回文件（`WriteConfig`），供游戏启动
配置界面（Story/Mod/Resource三个screen）使用。

## 关键设计

- **用`Dependence/common/json.h`而不是UE自带Json模块解析`config.json`**——UE的
  `FJsonSerializer`是严格JSON,不支持注释;`json.h`是JsonCpp衍生的宽松解析器,支持
  `//`/`/* */`注释,更适合手写维护的配置文件。
- **`dll_paths`/`layout_paths`/`resource_paths`/`story_scripts`里的相对路径都相对
  `config.json`自己所在目录(`configDir`)解析**——纯文件系统概念,不依赖UE的
  `FPaths::ProjectDir`,保持`Config`engine-agnostic。
- **`configDir`必须用`filesystem::canonical()`而不是直接`parent_path()`**——打包产物里
  `Resource/`是用目录连接(`mklink /J`)指向开发树真实位置的(见`Forever_UE/pak.bat`)，
  Windows路径解析对嵌入的`".."`是纯文本词法化处理，不会跟随reparse point——如果只取
  `parent_path()`，后续`"../.."`这类相对写法会在连接点所在的归档目录树内部兜圈子，走不到
  真实开发树。`canonical()`强制做一次真正的OS级解析，把`configDir`换成连接目标那一侧的
  真实路径；PIE/未打包环境下`Resource`本来就是真目录，这一步是无副作用的空操作。
- **`WriteConfig`写回绝对路径，不转成相对`configDir`的路径**——`config.json`不是跨设备
  使用的文件（每台开发机上mod/资源的实际存放位置经常不一样），配置界面的Add按钮拿到的
  本来就是绝对路径；`ReadConfig`仍然兼容解析相对路径写法，只是不会再有代码主动生成它。
- **`ReadConfig`找不到文件或JSON语法错误时留空，不抛异常**，但会打一条警告日志区分"文件
  打不开"（可能路径错了，也可能另一个进程正独占持有，比如正在`WriteConfig`）和"JSON语法
  错误"这两种根因不同的情况——否则排查"为什么回退扫描了默认目录"时无法区分。调用方
  （`ForeverModSubsystem`）据此决定要不要回退扫描默认目录(和`UForeverKeyBindingSubsystem`
  读`KeyBindings.ini`的容错风格一致)。
- **`AddDllPath`探测完立即`FreeLibrary`,不长期持有句柄**——探测阶段只需要确认"这是不是
  一个合法mod dll"，真正常驻加载是`ModLoader`的职责；`Config`和`ModLoader`各自独立
  `LoadLibraryA`同一个dll两次在Windows上是安全的(按路径做引用计数)。探测时会真正调用一次
  `GetMod<Concept>`读取返回的`vector<string>*`内容（不只是探测符号是否存在），拿到这个dll
  实际提供哪些`(concept, modId)`，供`GetModEnables()`等按id粒度展示用；本工程统一动态CRT
  (MultiThreadedDLL/MultiThreadedDebugDLL)，调用方只读不释放这份mod内static数据是安全的。
- **`AddDllPath`跳过路径里带`Debug`目录段的dll**——Debug配置用`/MDd`调试CRT编译，
  `std::string`内存布局和Release/Development不兼容，跨DLL读取它返回的`vector<string>`会
  按错误的布局解释内存直接崩溃（`EXCEPTION_ACCESS_VIOLATION`）。手工配置的`dll_paths`一直
  靠人工只填Release目录规避，但回退扫描(`ForeverModSubsystem::Initialize()`的默认目录
  分支)会无差别递归整个目录树，必须在这里显式挡掉。
- **路径统一规范化成正斜杠（`ToForwardSlashes`）**——`filesystem::absolute()`/
  `canonical()`在Windows下会把分隔符规范化成反斜杠，一旦原样塞进JSON字符串值（反斜杠后面
  跟的如果不是合法JSON转义字符，比如`\W`/`\P`/`\F`）就是非法JSON，这份自定义`JsonReader`
  遇到一处非法转义会导致整个文件解析失败（不只是这一个字段读不到，其它字段也会被一起
  扔掉）。所有写进`dllPaths`/`layoutPaths`/`resourcePaths`/`storyScriptPaths`的路径在落盘
  前都统一走这个转换。
- **`AddDllPath`探测用的`GetMod<Concept>`符号名来自`loader.h`的
  `GetModConceptDescriptors()`**,不在`config.cpp`里重复维护一份列表,避免两处列表不同步；
  per-mod依赖声明（`kModDependenciesSymbol`，整个DLL一份导出符号，但内容是这个DLL里
  每个mod id各自的依赖列表配对表，不是整个DLL共用一份）在同一次`LoadLibrary`里顺带
  探测，按modId拆开灌进`modDependences`，见`loader.h`。
- **`<concept>_mods`数组既是mod参数来源，也是"谁启用谁没启用"的唯一真相来源**：
  `ReadConfig`扫描所有以`_mods`结尾的顶层key（不硬编码concept名字），每个数组元素是
  `"id"`或`"id 参数..."`（如`"pengzhan --density 1.0"`），按第一个空格切成
  `(id, 参数字符串)`存进`conceptMods[jsonKey]`。`IsModEnabled`/`SetModEnabled`直接读写
  这张表，不维护平行的一份启用状态：`SetModEnabled(true)`把id加进数组（没有参数给空串），
  `SetModEnabled(false)`直接从数组删掉。**不要**改成"按`dll_paths`根目录配一份共用参数"
  的方案（`dll_args`）——这和"每个mod id有自己的参数"的真实需求对不上，已验证过走不通。
- **`RoadnetFactory`/`NameFactory`对应的`"Roadnets"`/`"Names"`两个concept只应该同时
  启用一个**（单选语义，不是Terrain那种多mod叠加）——`SetModEnabled(true)`会把同一
  concept下其它已发现id全部从数组里移除；`NormalizeUniqueConcepts()`在`ReadConfig`读完、
  `WriteConfig`写之前、`GetModEnables()`每次返回数据前都收口一次（只保留数组里排在最前面
  的条目），防止手写config.json时一次性塞了多个条目。
- **`RemoveDllPath`连带清理孤儿启用状态**——一个dll被移除时，它贡献的`(concept, modId)`
  会从对应`<concept>_mods`数组里一并删掉，不留下"启用着一个已经不存在的dll提供的id"。
- **`GetConceptMods`只做字符串切分,不做任何格式校验/解析**——参数部分原样返回,具体格式
  完全由消费方（mod自己注册的creator函数）决定；`Config`/`ModLoader`全程不关心参数内容。
- **`layout_paths`/`resource_paths`用和`dll_paths`同一套
  `recursive_directory_iterator`扫描手法，但不探测/加载任何东西**(`.layout`/`.script`
  是纯文本文件)，扫到就收。`AddResourcePath`在同一次扫描里按后缀分流收集`.script`
  （`resourcePaths`）、`.uplugin`（`pluginPaths`）、`.pak`（`pakPaths`）、`.layout`
  （并入`layoutPaths`，Resource screen要求一个文件夹能同时覆盖`.layout`和`.script`，
  不强制在两处各添加一次）。`GetScriptPath(name)`遍历`resourcePaths`按文件名（不含路径/
  扩展名）反查实际路径，供`JobMod`/`OrganizationMod`/`Story`这类只知道bare文件名、不应该
  替Mod决定"脚本实际存放在哪"的调用方使用，见`Core/society/job.md`"Script配置"一节。
  `HasResourcePaths()`供`ForeverModSubsystem::Initialize`判断要不要回退扫描默认的
  `Resource/Story/`目录。
- **`GetPlugins`/`GetPakFiles`**：UE资产只能通过已注册挂载点访问，独立编辑的UE资产因此
  必须放在一个Plugin里——`resource_paths`配置目录下`Plugins/<ModName>/<ModName>.uplugin`
  （编辑器/PIE挂载用）和`Plugins/<ModName>_Pak/<ModName>.pak`（打包/Shipping用）会被
  `AddResourcePath`的扫描发现；真正的挂载逻辑在UE层`ForeverModSubsystem::Initialize()`，
  见`Source/Forever/Mod/ForeverModSubsystem.md`。
- **`GetMainStoryScriptModName`/`GetMainStoryScriptPath`**：分别读`config.json`的
  `"main_story"`字段、包一层`GetScriptPath("test")`，供`Story::Init()`/
  `AForeverFrameworkActor::ValidateMainStoryDependencies()`等决定主线剧情`Script`挂载
  哪个`ScriptMod`、主线剧情`.script`文件路径在哪。字段缺失时返回空字符串，`Config`自己
  不兜底默认值。
- **`story_scripts`（`AddStoryScript`/`RemoveStoryScript`/`GetStoryScripts`）**：Story
  screen的"添加`.script`文件"操作维护的一份列表，列表里的脚本全部一起加载，不是
  "多选一激活"（和`main_story`单独一个字段、决定主线挂载哪个`ScriptMod`是两件事）。
- **`IsModIdEnabled`/`IsDllActive`/`GetActiveDllPaths`/`GetModDependences`**：给游戏启动
  配置界面的依赖校验用。`IsModIdEnabled(id)`不看concept，只要id在任意一个已启用的
  `(concept, modId)`里出现过就算数（mod id不带concept前缀，不能靠字符串猜它属于哪个
  concept）；`IsDllActive`判断一个DLL导出的id里有没有任意一个被启用（DLL本身只能整体
  加载/不加载，这个函数只管"要不要加载这个DLL"，和下面的依赖声明粒度无关）；
  `GetModDependences(modId)`按modId查`AddDllPath`探测阶段缓存的、来自
  `GetModDependences()`导出符号的依赖声明——粒度是单个mod id，不是DLL：同一个DLL
  注册的多个mod可以各自声明不同的依赖，不需要共用一份。

## 依赖关系

- 依赖:`Dependence/common/json.h`(解析JSON)、`Core/common/loader.h`
  (`GetModConceptDescriptors()`探测符号表、`kModDependenciesSymbol`)、`<windows.h>`
  (`LoadLibraryA`/`GetProcAddress`/`FreeLibrary`,只在`.cpp`里出现,头文件不引入)。
- 被谁依赖:`Source/Forever/Mod/ForeverModSubsystem.cpp`；游戏启动配置界面
  （Story/Mod/Resource三screen）读取`GetModEnables()`/`GetModDependences()`等展示，
  写入`SetModEnabled()`/`AddStoryScript()`等，最终调`WriteConfig()`落盘。

## 待办/后续阶段

- 全局设置字典（`GetGlobalSettings`一类，不同于`global_settings`脚本顶层字段，见
  `Core/player/player.md`"时钟行为"一节）尚未实现。
- 同一个dll在多次`AddDllPath`调用下被扫描到(如`Debug`/`Release`两个目录都存在)时，如果
  其中声明了同一个mod id，`ModLoader::RegisterConcept`会重复调用
  `RegisterMod<Concept>`，后一次覆盖前一次——目前未去重，开发时建议只保留一份构建产物在
  `Forever_Mod/`下。
