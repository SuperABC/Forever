# ForeverModSubsystem

## 职责

阶段3的UE侧启动触发点:读取`Forever_UE/Resource/Config/config.json`拿到mod dll扫描目录
和每个concept的`"<concept>_mods"`参数列表,对全部21个concept分别构造一个临时Factory、
设置参数表、发现/加载/注册该concept下的所有mod,并把每个已注册id创建出来的临时实例的
`GetType()`/`GetName()`打进Output Log,验证"config.json→Config解析(dll+参数)→
Factory.SetModArgs→ModLoader加载并注册进Factory→创建实例(参数字符串直接传给creator)→
读身份信息"
整条链路可用。**这是阶段3的验证手段,不是最终产品UX**——真正的mod启用/禁用UI、按系统接入
等能力留到阶段4及以后。

**阶段4-1起,Terrain这一个concept已经不在这份代码里了**——它的`TerrainFactory`现在由
`Source/Core/map/map.h`的`Map`类长期持有(`Map::InitTerrains()`自己调用
`ModLoader::RegisterConcept<TerrainFactory>`),不再需要`ForeverModSubsystem`临时代管+
验证，Calendar这个concept被整体移除之后又少了一个。目前还剩19个concept的验证块。

## 关键设计

- **用`UGameInstanceSubsystem`而不是塞进`AForeverGameMode::BeginPlay`**——参照
  `UForeverKeyBindingSubsystem`(`Source/Forever/Input/ForeverKeyBindingSubsystem.h`)已经
  立下的模式:`Initialize()`在关卡加载前、每个game instance生命周期内只跑一次,不受
  PIE/关卡切换影响,和`ForeverGameMode.h`里阶段0/阶段2那些"临时兜底,后续要删"的逻辑完全
  解耦,不需要改`ForeverGameMode.h/.cpp`。
- **不再持有任何成员对象**——`ModLoader`和21个`<Concept>Factory`全部是`Initialize()`内的
  局部变量,注册+校验完就在同一次调用里`modLoader.UnloadAll()`卸载所有dll句柄,不需要覆写
  `Deinitialize()`。这是本次加全21个concept时顺带做的简化:阶段3只是一次性验证,不需要让
  这些Factory实例活过`Initialize()`;阶段4随Map/Story等系统落地后,真正持久的Factory归属
  会转移给对应的Core领域系统类,到时候这里的验证代码可以整体删掉。
- **`ValidateFactory`是个模板辅助函数,用lambda传入create/destroy**——21个`<Concept>Factory`
  的`Create<Concept>`/`Destroy<Concept>`方法名各不相同,没有共同基类可以多态调用,所以用
  lambda把"怎么创建/怎么销毁"这两个动作参数化,避免写21份几乎相同的校验循环。
- **`config.json`缺失或`dll_paths`为空时,回退扫描默认的`Forever_Mod/`目录**——和
  `UForeverKeyBindingSubsystem`"缺配置就用硬编码默认值,不报错"的容错风格一致,详见
  `Source/Core/common/config.md`。
- **每个concept块在`RegisterConcept`之前先调用`factory.SetModArgs(...)`**——参数来自
  `Config::GetConceptMods("<concept_lower>_mods")`(解析`config.json`同名数组,每项是
  `"id"`或`"id 参数..."`,和旧工程`"test ---name value"`写法一致),经本文件里的
  `ToArgsMap`辅助函数转成`id->参数`表后传给Factory。之后mod调用`RegisterX(id, ...)`时
  Factory会按id查这张表、把参数存进注册项,`CreateX(id)`创建实例时直接把参数字符串传给
  注册的creator函数——`ForeverModSubsystem`自己不直接接触参数字符串,只负责把配置读出来、
  按concept分发给对应Factory。详见`Source/Dependence/README.md`和
  `Source/Core/common/config.md`。示例见`Forever_Mod/Empty/`(`Empty<Concept>`的构造函数
  把收到的参数字符串直接拼进`GetName()`,方便在这里的日志里确认参数到达且按id区分正确)。

## 依赖关系

- 依赖:`Source/Core/common/config.h`(`Config::ReadConfig`/`GetMods`/`AddDllPath`)、
  `Source/Core/common/loader.h`(`ModLoader::RegisterConcept`/`UnloadAll`)、
  `Source/Dependence`下全部20个`<domain>/<concept>_factory.h`。
- 被谁依赖:无(UE会在game instance启动时自动实例化所有`UGameInstanceSubsystem`,不需要
  手动获取)。

## 待办/后续阶段

- 阶段4:随着Zone/Story等系统在Core里落地,每个concept真正需要长期存在的Factory实例应该
  转移给对应的领域系统类持有(Terrain/`Map`已经是第一个例子,见`Source/Core/map/
  terrain_factory.md`),`ForeverModSubsystem`这份一次性验证代码到时候可以删掉或大幅精简。
- 阶段4/参数化机制的后续扩展:目前"参数"只是Core透传的原始字符串,mod自己决定怎么解析;
  如果要支持`REFACTOR_PLAN.md`原文提到的"命令行式参数"(`--density 1.0 --max 1000`)这种
  结构化格式,可以在Dependence层加一个共享的小型参数解析工具函数供各mod调用,避免每个mod
  自己重复写tokenize逻辑——这次没有加,因为阶段3只要求"参数能传到",没有要求"传什么格式"。
