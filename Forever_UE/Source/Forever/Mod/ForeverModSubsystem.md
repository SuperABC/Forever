# ForeverModSubsystem

## 职责
负责mod的发现和mod自带UE内容(Plugin/Pak)的挂载。**不负责mod dll的加载和Factory注册**——那是Core的`Registry`单例的事(每局开始`Registry::RefreshMods()`，见`Source/Core/common/registry.md`)。

- `Initialize()`在GameInstance启动时只做纯发现：读取`Resource/Config/config.json`，`dll_paths`缺失时回退扫描`../Forever_Mod`(`resource_paths`不回退，没配置就是没有)，把结果写进`Config`静态状态供配置界面UI展示，不做任何挂载。`ReadConfig`对缺失/无效文件只留空状态不报错，所以回退按"结果为空"判定；`Config`内部的警告走`std::cerr`，不一定进`Forever.log`，这里另用`UE_LOG`打印诊断。
- `MountModContent()`挂载mod自带的UE内容，由`UForeverConfigBridgeSubsystem::ValidateAndStartGame`校验通过、`OpenLevel`进入正式关卡之前显式调用，读档时`UForeverSaveSubsystem::RequestLoad`应用存档的mod集合后也调一次。可以重复调用(退出游戏回菜单后再开始游戏)：已挂载过的Plugin/Pak按挂载点(`mountedMountPoints`)跳过。

## 关键设计
- **挂载mod自己的UE资产Plugin**(`MountModContent()`)：按`WITH_EDITOR`分两条路径：
  - Editor/PIE：对每个`.uplugin`算出`Content`目录，`FPackageName::RegisterMountPoint(TEXT("/")+pluginName+TEXT("/"), contentDir)`+`IAssetRegistry::ScanPathsSynchronous`，不需要cook/pak，不需要在主`.uproject`的`Plugins`列表里启用。
  - 打包/Shipping：对每个`.pak`用`FCoreDelegates::MountPaksEx`挂载，再`RegisterMountPoint`到固定写死的`"../../../"+pluginName+"/Content/"`(`UnrealPak`打包内部路径约定，mod自己的`pak.bat`生成filelist时必须对得上)，外加`FShaderCodeLibrary::OpenLibrary`。**`RegisterMountPoint`不能靠`MountPaksEx.Execute()`的返回值gate**——pak挂载成功时这个返回值仍可能是`false`，`RegisterMountPoint`本身是幂等的纯注册操作，挂完就应该紧接着做。
  - 为什么必须是Plugin而不是普通文件夹：UE资产只能通过已注册的挂载点访问，原始文件夹不是合法的挂载对象。详见`Source/Core/common/config.md`"GetPlugins/GetPakFiles"一节。
- **`Initialize()`和`MountModContent()`拆成两阶段**：前者只读取/回退扫描，给配置界面(Story/Mod/Resource三screen)立刻有数据可展示；后者才有副作用，推迟到用户在配置界面点"开始游戏"、`ValidateAndStartGame`的依赖校验全部通过之后才执行，校验失败不会触碰任何真实状态。
- **用`UGameInstanceSubsystem`而不是塞进`AForeverPlayerGameMode::BeginPlay`**：参照`UForeverKeyBindingSubsystem`的既定模式，生命周期覆盖菜单关卡→`OpenLevel`切到`World.World`的整个过程，不受关卡切换影响。
- **这里不创建任何Factory、不创建mod实例**：mod能不能注册、参数能不能传到，由ModTest(`Source/Test/Mod`，枚举全部23个concept的mod、逐个创建实例跑`UnitTest`)和CoreTest的mods套件(Core的`Registry`认不认得)覆盖。在UE里重复做一遍只会让每个mod的命名计数器多走一格。

## 依赖关系
- 依赖：`Source/Core/common/config.h`(`Config::ReadConfig`/`GetMods`/`AddDllPath`/`GetPlugins`/`GetPakFiles`等)。
- 被谁依赖：`UForeverConfigBridgeSubsystem::ValidateAndStartGame`(依赖校验通过后调用`MountModContent()`，再`OpenLevel`)、`UForeverSaveSubsystem::RequestLoad`(读档时应用存档的mod集合后调用)。UE会在game instance启动时自动实例化所有`UGameInstanceSubsystem`，`Initialize()`不需要手动触发。

## 待办/后续阶段
- 回菜单后再开始游戏：新增的dll路径会被`Registry::RefreshMods()`补加载，从配置里移除的dll不卸载(保持加载、因不在启用表而被禁用)。

## ReloadConfigFrom(path)

菜单"读取配置"用：`Config::ReadConfig(path)` 后若 dll_paths、resource_paths、story_scripts 全空，视为文件无效(ReadConfig 对打不开/语法错误只留空状态不报错)，恢复读回 config.json 并返回 false；否则套用与 `Initialize()` 相同的缺省目录回退(`ReadConfigWithFallbacks`，只对 dll_paths 回退，resource_paths 没配置就是没有)。只更新内存发现状态，不挂载不注册。
