# ForeverModSubsystem

## 职责
负责mod的发现、Plugin/Pak资产挂载、16个concept的`<Concept>Factory`注册与校验。`Initialize()`在GameInstance启动时只做纯发现(读取`Resource/Config/config.json`,`dll_paths`/`resource_paths`缺失时分别回退扫描默认目录(resource回退为`Resource/Story`+`Resource/Layouts`)),把结果写进`Config`静态状态供配置界面UI展示,不做任何Plugin/Pak挂载或Factory注册。真正的挂载+注册+校验在`EnsureModsRegistered()`,由`UForeverConfigBridgeSubsystem::ValidateAndStartGame`校验通过、`OpenLevel`进入正式关卡之前显式调用,`bModsRegistered`门闩保证每个GameInstance生命周期只真正跑一次。

## 关键设计
- **挂载mod自己的UE资产Plugin**(`EnsureModsRegistered()`开头):按`WITH_EDITOR`分两条路径:
  - Editor/PIE:对每个`.uplugin`算出`Content`目录,`FPackageName::RegisterMountPoint(TEXT("/")+pluginName+TEXT("/"), contentDir)`+`IAssetRegistry::ScanPathsSynchronous`,不需要cook/pak,不需要在主`.uproject`的`Plugins`列表里启用。
  - 打包/Shipping:对每个`.pak`用`FCoreDelegates::MountPaksEx`挂载,再`RegisterMountPoint`到固定写死的`"../../../"+pluginName+"/Content/"`(`UnrealPak`打包内部路径约定,mod自己的`pak.bat`生成filelist时必须对得上),外加`FShaderCodeLibrary::OpenLibrary`。**`RegisterMountPoint`不能靠`MountPaksEx.Execute()`的返回值gate**——pak挂载成功时这个返回值仍可能是`false`,`RegisterMountPoint`本身是幂等的纯注册操作,挂完就应该紧接着做。
  - 为什么必须是Plugin而不是普通文件夹:UE资产只能通过已注册的挂载点访问,原始文件夹不是合法的挂载对象。详见`Source/Core/common/config.md`"GetPlugins/GetPakFiles"一节。
- **`Initialize()`和`EnsureModsRegistered()`拆成两阶段**:前者只读取/回退扫描,给配置界面(Story/Mod/Resource三screen)立刻有数据可展示;后者才是真正有副作用的挂载/注册,推迟到用户在配置界面点"开始游戏"、`UForeverConfigBridgeSubsystem::ValidateAndStartGame`的依赖校验全部通过之后才执行,校验失败不会触碰任何真实状态。
- **用`UGameInstanceSubsystem`而不是塞进`AForeverPlayerGameMode::BeginPlay`**:参照`UForeverKeyBindingSubsystem`的既定模式,生命周期覆盖菜单关卡→`OpenLevel`切到`World.World`的整个过程,不受关卡切换影响。
- **`EnsureModsRegistered()`不持有任何成员对象**:`ModLoader`和16个`<Concept>Factory`全部是函数内的局部变量,注册+校验完就在同一次调用里`modLoader.UnloadAll()`卸载所有dll句柄。真正持久的Factory归属属于对应的Core领域系统类(如Terrain已经转移给`Source/Core/map/map.h`的`Map`类持有,不在这16个之内)。
- **`ValidateFactory`是模板辅助函数,用lambda传入create/destroy**:16个`<Concept>Factory`的`Create<Concept>`/`Destroy<Concept>`方法名各不相同,没有共同基类可以多态调用,用lambda参数化"怎么创建/怎么销毁"避免写16份几乎相同的校验循环。
- **每个concept块在`RegisterConcept`之前先调用`factory.SetModArgs(...)`**:参数来自`Config::GetConceptMods("<concept>_mods")`,经`ToArgsMap`转成`id->参数`表后传给Factory;mod调用`RegisterX(id, ...)`时Factory按id查表、把参数存进注册项,`CreateX(id)`创建实例时把参数字符串传给注册的creator函数。详见`Source/Dependence/README.md`和`Source/Core/common/config.md`。

## 依赖关系
- 依赖:`Source/Core/common/config.h`(`Config::ReadConfig`/`GetMods`/`AddDllPath`/`GetConceptMods`等)、`Source/Core/common/loader.h`(`ModLoader::RegisterConcept`/`UnloadAll`)、16个`<domain>/<concept>_factory.h`(Component/Room/Asset/App/Puzzle/Name/Scheduler/Job/Organization/Script/Product/Storage/Manufacture/Route/Station/Vehicle)。
- 被谁依赖:`UForeverConfigBridgeSubsystem::ValidateAndStartGame`(依赖校验通过后调用`EnsureModsRegistered()`,再`OpenLevel`)。UE会在game instance启动时自动实例化所有`UGameInstanceSubsystem`,`Initialize()`不需要手动触发。

## 待办/后续阶段
- 随着更多领域系统在Core里落地,仍留在本类里长期验证的Factory实例可以继续转移给对应领域系统类持有(参照Terrain已转移给`Map`的先例)。
- 不支持中途回菜单重新注册mod(`bModsRegistered`门闩只允许每个GameInstance生命周期注册一次)。
- 目前"参数"只是Core透传的原始字符串,mod自己决定怎么解析;如果要支持结构化的命令行式参数(如`--density 1.0 --max 1000`),可以在Dependence层加一个共享的小型参数解析工具函数,避免每个mod自己重复写tokenize逻辑。

## ReloadConfigFrom(path)

菜单"读取配置"用：`Config::ReadConfig(path)` 后若 dll_paths、resource_paths、story_scripts 全空，视为文件无效(ReadConfig 对打不开/语法错误只留空状态不报错)，恢复读回 config.json 并返回 false；否则套用与 `Initialize()` 相同的缺省目录回退(`ReadConfigWithFallbacks`)。只更新内存发现状态，不挂载不注册。
