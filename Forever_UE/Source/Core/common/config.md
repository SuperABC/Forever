# Config (config.h / config.cpp)

## 职责

读取/维护`Forever_UE/Resource/Config/config.json`的静态类：`dll_paths`/
`resource_paths`两类根目录扫描、每个concept的mod参数与启用状态、主线剧情脚本路径与
Story screen的`.script`列表，并能把当前内存状态写回文件（`WriteConfig`），供游戏启动
配置界面（Story/Mod/Resource三个screen）使用。

## 关键设计

- **用`Dependence/common/json.h`而不是UE自带Json模块解析`config.json`**——UE的
  `FJsonSerializer`是严格JSON,不支持注释;`json.h`是JsonCpp衍生的宽松解析器,支持
  `//`/`/* */`注释,更适合手写维护的配置文件。
- **`dll_paths`/`resource_paths`/`story_scripts`里的相对路径都相对
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
  **内容类型不对也一样**(例如`dll_paths`里混进一个数字，`AsString()`会抛数据错误)：`ReadConfig`在自己里面接住，整份配置清空(`ClearState`)而不是留下读到一半的半份，原因记在`GetLastReadError()`(单行文字，带类别和config路径)并打一条Error日志。调用方本来就按"读完是空的"判定配置无效。`AddResourcePath`扫描目录用带`error_code`的迭代器，无权限的子目录跳过而不是抛`filesystem_error`。
  打不开"（可能路径错了，也可能另一个进程正独占持有，比如正在`WriteConfig`）和"JSON语法
  错误"这两种根因不同的情况——否则排查"为什么回退扫描了默认目录"时无法区分。调用方
  （`ForeverModSubsystem`）据此决定要不要回退扫描默认目录(和`UForeverKeyBindingSubsystem`
  读`KeyBindings.ini`的容错风格一致)。
- **`AddDllPath`先预筛、再加载**(2026-11-25)——递归扫到的每个dll先用`ProbeModDll`(`Dependence/common/mod_concepts.h`)只读文件
  看导出表：没有导出任何`GetMod<Concepts>`的不是mod，直接跳过，**不`LoadLibrary`**。以前是"加载→问→不是就卸载"，
  无关dll的`DllMain`和全局构造在"问"之前就已经跑过了，`dll_paths`配到范围大的目录时有风险。mod自己的dll仍然要加载
  (取id列表必须调它的函数)。
- **`AddDllPath`探测完立即`FreeLibrary`,不长期持有句柄**——探测阶段只需要确认"这是不是
  一个合法mod dll"，真正常驻加载是`ModLoader`的职责；`Config`和`ModLoader`各自独立
  `LoadLibraryA`同一个dll两次在Windows上是安全的(按路径做引用计数)。探测时会真正调用一次
  `GetMod<Concept>`读取返回的`vector<string>*`内容（不只是探测符号是否存在），拿到这个dll
  实际提供哪些`(concept, modId)`，供`GetModEnables()`等按id粒度展示用；本工程统一动态CRT
  (MultiThreadedDLL/MultiThreadedDebugDLL)，调用方只读不释放这份mod内static数据是安全的。
- **`AddDllPath`跳过调试版的dll**——Debug配置用`/MDd`调试CRT编译，
  `std::string`内存布局和Release/Development不兼容，跨DLL读取它返回的`vector<string>`会
  按错误的布局解释内存直接崩溃（`EXCEPTION_ACCESS_VIOLATION`）。判断方法是`ProbeModDll`看导入表里有没有调试版CRT
  (`MSVCP140D.dll`/`VCRUNTIME140D.dll`/`ucrtbased.dll`)，不执行dll。2026-11-25以前靠"路径里有没有`Debug`这一段
  目录名"判断，那只是个约定，Debug版dll放到别的目录名下就挡不住。静态链接调试CRT(`/MTd`)的dll导入表里看不出来，
  本工程的mod统一用动态CRT，不会出现。
- **路径只有一种写法（`NormalizePath`，2026-11-25）**——`AddDllPath`/`RemoveDllPath`/`AddResourcePath`/`RemoveResourcePath`/`AddStoryScript`/`RemoveStoryScript`六个入口一进来就把路径规范化：转成绝对路径、去掉`.`/`..`和多余的分隔符、正斜杠、结尾不带斜杠、盘符大写。
  `dllPaths`/`resourcePaths`等map都拿路径字符串当键，同一个目录写法不同就会被当成两个根
  (config.json里出现过`.../Test/Resource/../../../../../Projects/Forever/Forever_UE/Resource/Layouts`)。
  用的是词法规范化(`lexically_normal`)，**不是`canonical`**：不解析目录连接(junction)。Windows自己解析`..`也是词法的，
  所以规范化前后指的是同一个目录；`canonical`会把经过junction的路径换成真实目标(打包产物里`Resource/`就是junction)。
  已有的config.json不用手改：读进来时就规范化了，下次`WriteConfig`写回去的就是规范写法。大小写只统一盘符，
  路径其余部分大小写不同仍算两个写法。
- **路径统一规范化成正斜杠（`ToForwardSlashes`）**——`filesystem::absolute()`/
  `canonical()`在Windows下会把分隔符规范化成反斜杠，一旦原样塞进JSON字符串值（反斜杠后面
  跟的如果不是合法JSON转义字符，比如`\W`/`\P`/`\F`）就是非法JSON，这份自定义`JsonReader`
  遇到一处非法转义会导致整个文件解析失败（不只是这一个字段读不到，其它字段也会被一起
  扔掉）。所有写进`dllPaths`/`resourcePaths`/`storyScriptPaths`的路径在落盘
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
- **`resource_paths`用和`dll_paths`同一套
  `recursive_directory_iterator`扫描手法，但不探测/加载任何东西**(`.layout`/`.script`
  是纯文本文件)，扫到就收。`AddResourcePath`在同一次扫描里按后缀分流收集`.script`
  （`resourcePaths`）、`.uplugin`（`pluginPaths`）、`.pak`（`pakPaths`）、`.layout`
  （并入内部的`layoutPaths`，没有单独的`layout_paths`配置项——一个文件夹天然同时覆盖
  `.layout`和`.script`）。`GetResourceFilePath(fileName)`按带扩展名的文件名在各根目录下现扫递归查找(不缓存)，
  给`PostImplement`的`"resource path"`查询用，让mod能读`.txt`等非`.script`/`.layout`数据文件。
  `GetScriptPath(name)`遍历`resourcePaths`按文件名（不含路径/
  扩展名）反查实际路径，供`JobMod`/`OrganizationMod`/`Story`这类只知道bare文件名、不应该
  替Mod决定"脚本实际存放在哪"的调用方使用，见`Core/society/job.md`"Script配置"一节。
  `HasResourcePaths()`供`ForeverModSubsystem::Initialize`判断要不要回退扫描默认的
  `Resource/Story/`目录。
- **`GetPlugins`/`GetPakFiles`**：UE资产只能通过已注册挂载点访问，独立编辑的UE资产因此
  必须放在一个Plugin里——`resource_paths`配置目录下`Plugins/<ModName>/<ModName>.uplugin`
  （编辑器/PIE挂载用）和`Plugins/<ModName>_Pak/<ModName>.pak`（打包/Shipping用）会被
  `AddResourcePath`的扫描发现；真正的挂载逻辑在UE层`UForeverModSubsystem::MountModContent()`，
  见`Source/Forever/Mod/ForeverModSubsystem.md`。
- **`GetMainStoryScriptModName`**：读`config.json`的`"main_story"`字段，供`Story::Init()`决定主线剧情`Script`挂载
  哪个`ScriptMod`。字段缺失时返回空字符串，`Config`自己不兜底默认值。主线剧情的`.script`文件见下面的
  `GetMainStoryScriptPaths()`。
- **`story_scripts`（`AddStoryScript`/`RemoveStoryScript`/`GetStoryScripts`）**：Story
  screen的"添加`.script`文件"操作维护的一份列表，列表里的脚本全部一起加载，不是
  "多选一激活"（和`main_story`单独一个字段、决定主线挂载哪个`ScriptMod`是两件事）。
- **`IsModIdEnabled`/`GetModDependences`**：给游戏启动
  配置界面的依赖校验用。`IsModIdEnabled(id)`不看concept，只要id在任意一个已启用的
  `(concept, modId)`里出现过就算数（不看id的格式）；
  `GetModDependences(modId)`按modId查`AddDllPath`探测阶段缓存的、来自
  `GetModDependences()`导出符号的依赖声明——粒度是单个mod id，不是DLL：同一个DLL
  注册的多个mod可以各自声明不同的依赖，不需要共用一份。

- **`GetLayouts()`不去重**：`BuildingLayoutLibrary`按文件basename为key，同名`.layout`后加载的会覆盖先加载的，
  调用方自己保证不重复摆放同名模板。
- **`GetMainStoryScriptPaths()`**：Story screen的`story_scripts`列表非空时就是这份列表（按顺序，各文件的
  milestones/names/`name_reserve`/`global_settings`合并进同一个主线`Script`），列表为空就是没有主线脚本，
  不兜底到任何固定名字的脚本(2026-11-25以前会退回名为`test`的脚本)。
- 单选concept的名单(`Roadnets`/`Names`)只在`config.cpp`的`UniqueConcepts()`里列一次，`NormalizeUniqueConcepts`和`IsUniqueConcept`都用它。
- **`GetUniqueConceptMod(conceptKey)`**(2026-11-25)：单选concept(`Roadnets`/`Names`，`IsUniqueConcept`)当前启用的那个mod id，
  一个都没启用返回空串；配置里列了多个时取第一个并`debugf`警告(界面和`WriteConfig`都会经`NormalizeUniqueConcepts`收口成一个，
  手改`config.json`才会出现)。`Map::InitRoadnet`和`Populace::InitNames`都靠它决定用哪个mod——"单选"这件事只在`Config`里表达，
  两个Factory不再各存一张启用标记表。
- **新发现的mod默认禁用**：`IsModEnabled`对没列出的id（含`jsonKey`整个不存在）返回false，刚`AddDllPath`扫出来的mod
  需要在Mod screen手动勾选一次才生效。
- **`WriteConfig`只在Start Game校验通过那一刻调用一次**，不是每次编辑都自动保存；`<concept>_mods`数组就是
  `conceptMods`原样写回。

## 存档相关接口

- `ConceptKeyToJsonKey` 改成 public：存档按 `"<concept>_mods"` key 记 mod，和 `GetModEnables()` 的 conceptKey 对照要用。
- `GetConceptJsonKeys` / `SetConceptMods` / `SetStoryScripts` / `SetMainStoryScriptModName`：读档时把存档的 mod 集合和
  剧情脚本设成当前状态，**只改内存，不写回 config.json**（校验失败时由 `SaveGame::RestoreConfig` 恢复）。
- `FindMissingScripts` / `FindMissingLayouts`：存档的资源检查，按名字在配置的资源/布局目录里找，返回缺失的名字。调用方（`SaveGame::ValidateArchive`）会先 `RescanFiles`，保证查的是磁盘上的当前内容。
- `RescanFiles`：重新扫描所有已配置的 layout/resource 根目录，刷新缓存的文件列表。根目录添加时只扫一次，之后运行中增删文件不会自动反映，所以 `SaveGame::ValidateArchive`（读档校验）和 `ValidateAndStartGame`（开始新游戏）开头各调一次。
  见 `save.md`。

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

## 单选concept(Roadnets/Names)
必须且只能启用一个：`SetModEnabled`启用其一会取消同concept其它id，对已启用的传`enabled=false`无效(`IsUniqueConcept`)。
`ValidateAndStartGame`开局前校验这两个concept各恰好一个，否则在menu报错拦截。运行时`Map::InitRoadnet`/`Populace::InitNames`
仍保留"没配置就选第一个已注册的"兜底(直接PIE不经过menu时用)。

## 保存时丢弃看不见的mod
加载的config里某些mod id当前dll_paths下没有dll提供，UI里不会显示。`WriteConfig`保存前调`PruneUndiscoveredMods`：把这些id从各`<concept>_mods`里删掉
(`IsModDiscovered`判断)，并且main_story不是"已启用的script mod"时清空。`NormalizeUniqueConcepts`单选收口时优先留第一个已发现的，不会被前面藏起来的陈旧id挤掉。
`SetModEnabled`取消启用的script mod正是main_story时同时清空main_story。

## 种子
- 顶层`"seed"`(数字或字符串)：Core各域(`core.map/populace/society/traffic/weather/story`)随机流的种子来源，每域按`hash(seed, 域名)`派生；不写=随机。
- `<concept>_mods`条目里的`--seed N`：该mod自己的随机流种子，如`"building_residence_high --seed 7"`；不写=随机。实际用到的种子在开局时打到日志(`Random: ...`)，想复现就把它填回去。

## 地图大小
- 顶层`"map_size"`：新开局的地图边长(地图元素，1元素=10m)。写一个数=正方形(`"map_size": 256`)，写`[宽,高]`=长方形；不是正整数或格式不对就告警当没配置。`Config::GetMapSize`只做格式解析，"必须是2的整数次幂"由`AForeverFrameworkActor`校验(不满足告警并用默认1024)。读档时地图大小来自存档，不受它影响。`WriteConfig`会原样写回。

## 参数字符串读写（GetModArgs / SetModArgs）
`GetModArgs/SetModArgs(conceptKey, modId[, args])`读写某个mod的整串参数（如`--path=x --skip`）。已启用的mod读写`conceptMods`；**未启用的读写内存暂存表`stashedModArgs`**（key=jsonKey+'
'+id）：`SetModEnabled(false)`和单选concept被挤掉时把该条目的参数暂存，`SetModEnabled(true)`时带回，所以"禁用再启用"不丢参数。暂存表只在内存，`WriteConfig`不写它，`ReadConfig`开头清空。给Menu的Mod参数编辑用。
