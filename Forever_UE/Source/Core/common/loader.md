# ModLoader (loader.h / loader.cpp)

## 职责

给定一批已经被`Config::AddDllPath`确认过"实现了某个Mod接口"的dll绝对路径,针对某个具体
concept,真正`LoadLibraryA`并常驻句柄、解析`RegisterMod<Concept>`/`FinishMod<Concept>`符号
并调用,把dll里的mod注册进调用方传入的具体`<Concept>Factory*`实例。

## 关键设计

- **和`Config`拆成两个类,而不是合并**——`Config`只回答"config.json配置的目录下,有哪些
  dll是合法mod"(探测完即释放句柄,详见`config.md`);`ModLoader`回答"把这些合法dll接入
  某个具体系统的Factory"(常驻句柄,按需调用注册/收尾)。这是两件不同频率的事:探测在
  `ReadConfig`时做一次,注册要按每个系统(Building/Script/……)各自的Factory分别做——阶段4
  加入新系统时只需要多调一次`RegisterConcept`,不用碰`Config`。
- **`GetModConceptDescriptors()`是21个concept的DLL导出符号名唯一权威列表**——`conceptKey`
  只是内部使用的可读标签(`"Buildings"`/`"Scripts"`等),真正决定探测/注册行为的是
  `getModSymbol`/`registerModSymbol`/`finishModSymbol`三个字符串,和旧工程
  `Config::AddDllPath`里探测的21个固定`GetMod<Concept>`符号名一一对应。`Config::AddDllPath`
  复用这份列表的`getModSymbol`做探测,避免两处维护同一份符号表。
- **头文件不`#include <windows.h>`**——`HMODULE`/`FARPROC`只在`loader.cpp`里出现,句柄
  对外一律是`void*`。这是因为`loader.h`会被`Source/Forever/Mod/ForeverModSubsystem.cpp`
  间接引用,而UE的`CoreMinimal.h`已经拉了一套宏安全的Windows类型子集,再让这个头文件自己
  `#include <windows.h>`有和UE宏冲突的风险,隔离在.cpp里彻底避免这类问题。
- **`RegisterConcept`是模板方法**——直接用`FactoryT*`承接调用方传入的具体Factory类型
  (`BuildingFactory`/`ScriptFactory`等),`reinterpret_cast`成`void(*)(FactoryT*)`函数指针
  调用,不需要为每个concept写一份几乎相同的非模板重载。
- **参数不经过`ModLoader`**——早期版本让`RegisterMod<Concept>`导出函数多带一个`args`参数、
  按dll根目录配一份共用参数,后来发现和用户要的"`config.json`里`<concept>_mods`数组每项
  `"id 参数..."`(和旧工程`"test ---name value"`一样的命令行式写法)"这种**按mod id**配置
  参数的格式对不上——`ModLoader`按dll路径工作,根本不知道一次`RegisterMod<Concept>`调用会
  注册哪些id,没法把"哪个id对应哪份参数"这件事做对。现在改成:参数完全在`Config`
  (`GetConceptMods`解析`<concept>_mods`)和`<Concept>Factory`
  (`SetModArgs`/`ApplyArgs`,见`Source/Dependence/README.md`)之间流转,`RegisterMod<Concept>`
  导出函数签名维持`(factory)`单参数不变,`ModLoader`对参数机制完全无感知。
- **跨DLL new/delete安全体现在这里**:`RegisterModBuildings`等导出函数把
  `[]() { return new PengzhanBuilding(); }`和`[](BuildingMod* b) { delete b; }`成对注册进
  Factory(见`Forever_Mod/README.md`),两者都在mod自己的DLL里,`Factory::Destroy<Concept>`
  调用的正是这对函数指针里的deleter,而不是Factory自己`delete`——保证分配和释放发生在同一
  模块。`ModLoader`本身不直接涉及对象生命周期,只负责把这对函数指针"运进"Factory。
- **已知限制:同一个dll在`Debug`/`Release`两个目录都存在时会被扫描到两次**——如果
  `Forever_Mod/`下同时有Debug和Release构建产物,`Config::GetMods()`按绝对路径去重后仍会是
  两条不同路径(一个在`x64/Debug/`,一个在`x64/Release/`),`RegisterConcept`会对同一个mod id
  重复调用`RegisterMod<Concept>`,后一次覆盖前一次——阶段3不处理这个情况,建议开发时只保留
  一份构建产物在`Forever_Mod/`下。

## 依赖关系

- 依赖:无引擎依赖,纯C++(`<windows.h>`只在`.cpp`里)。
- 被谁依赖:`Source/Core/common/config.cpp`(用`GetModConceptDescriptors()`做探测)、
  `Source/Forever/Mod/ForeverModSubsystem.cpp`(构造`ModLoader`实例并调用
  `RegisterConcept<BuildingFactory>`/`RegisterConcept<ScriptFactory>`)。

## 待办/后续阶段

- 阶段4:按系统迁移进度,逐个把`GetModConceptDescriptors()`里其余19个concept接入对应系统
  的`RegisterConcept`调用(目前只有`ForeverModSubsystem`验证了Buildings/Scripts两项)。
- 阶段4:如果同一个dll需要支持"重新扫描/热重载"场景,需要在`UnloadAll`之外补充"卸载单个
  dll"的能力,目前只支持一次性加载、退出时整体卸载。
