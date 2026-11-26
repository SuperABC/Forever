# 约定文档

本文档记录整个项目(内核迁移 + 新功能开发)过程中确立的约定,新开的session都要遵守。

## 1. `.md`配对文档模板

每个逻辑独立的`.h`/`.cpp`(一个类或紧密相关的一组类)配一份同名`.md`,记录设计思路,取代代码里的大段中文块注释。**Dependence/Core/Basic层强制要求**;Forever UE模块层的类也建议配,尤其是逻辑不止"数据搬运"的类。一批没有独特逻辑的同类实现(如`Forever_Mod/Empty`里各concept的`Empty<Concept>`占位、Basic里同一批小游戏)可以共用一份文档,避免大量重复样板文字;某个成员从占位升级为真正实现后,要从共用文档"毕业",改配自己的独立`.md`。

```
# <ClassOrFileName>

## 职责
一两句话说明这个文件/类是做什么的。

## 关键设计
- 非显而易见的设计取舍、为什么这样做而不是别的方式
- 隐藏的约束/不变量

## 依赖关系
- 依赖:...
- 被谁依赖/谁会扩展它(尤其Mod可扩展点要写清楚):...

## 待办/后续阶段
- 明确写清楚还缺什么,避免误以为是遗漏;已经做完的事项不要留在这里
```

`.md`和代码注释只记录**当前状态**和**非显而易见的WHY**——不要保留"最初怎么写、后来改成什么样、改了几版"这类过程叙事;某个设计被推翻/废弃,直接说清楚现在的做法即可,除非废弃方案本身是容易被重新踩中的坑(如下面"Mod参数传递"一节),才值得提一句"不要再这样做"。

## 2. 代码风格

### Dependence / Core / Basic 层(与UE无关的纯C++)

- 类型名、方法名:PascalCase(如`BuildingMod`、`RegisterBuilding`)
- 成员变量:camelCase(如`batchSize`、`floorHeights`)
- 文件名、domain文件夹名:snake_case(如`building_mod.h`、`map/`)
- 只用`std::`容器和纯C++类型,**不出现任何UE头文件/类型**(`FString`/`TArray`/`UObject`等一律禁止),因为Mod侧是不依赖UE运行时的纯Win32 DLL,依赖二进制兼容
- 函数级中文说明移入配对`.md`,代码里不再写块注释;非显而易见的行内WHY注释仍可保留
- 前向引用集中在`Core/common/class.h`:按domain分组(`// Common`/`// Map`/`// Populace`/`// Society`/`// Story`/`// Industry`/`// Traffic`/`// Player`/`// Polit`)声明所有跨文件用到的`class Foo;`。Core下其余`.h`/`.cpp`不再自己写任何前向引用,一律`#include "class.h"`(项目IncludePath已把`Core/common`加进去,不用带`common/`前缀);新增一个需要前向引用的类型时,直接加进`class.h`对应分组,不要在具体文件里就地声明
- **每个concept的`.h`必须配一份同名`.cpp`,哪怕逻辑全写在`.h`里(纯虚接口+inline构造函数之类)也要建**——让每个头文件都有一个专属的编译单元:①验证头文件本身`#include`齐全、不依赖"被别的头文件带出来的东西"才能编译(header self-contained检查);②和`.md`配对文档、vcxproj里`ClCompile`/`ClInclude`成对出现的既有习惯保持一致。新增一个`<Concept>Mod`/`<Concept>Factory`或Basic里的一个mod时,`.h`/`.cpp`要一起建,`.cpp`记得同步加进对应`.vcxproj`的`ClCompile`和`.vcxproj.filters`的对应Filter——这一项反复出现过筛选器/编译列表漏加的问题,新建文件后务必两处一起检查。
- **文件头`#include`顺序+空行规则**(完整示例见`Core/map/map.h`/`.cpp`和`Basic/map/terrain_ocean.*`/`Dependence/map/terrain_mod.h`/`terrain_factory.*`):
  - `.h`:`#pragma once`,空一行,`#include "class.h"`(仅Core层、且这个文件确实用到前向引用类型时才有这一条),空一行,Dependence的`#include`(本文件直接用到的Dependence头,只在文件内部之间不空行),空一行,Core的`#include`(同上,只有Core层文件才可能有这一组),空一行,`<>`系统头(`<string>`等,组内不空行)。以上每一组如果本来就是空的直接跳过,不留空行占位。
  - `.cpp`:先`#include "同名.h"`(自己的头),空一行,再按`.h`同一套顺序(class.h/Dependence/Core/系统头)继续写。
  - 所有`#include`写完之后,如果这个文件有`#define`,紧跟一个空行再写`#define`块(块内的多组`#define`之间可以按语义拆成小节、组间各空一行,这是普通代码分段,不是这条规则管的范围)。
  - `#define`写完(或者没有`#define`、`#include`就是最后一步)之后,空两行,再写这个文件真正的第一行内容——`.cpp`如果要写`using namespace std;`,它算在这"两行空行之后"的内容里,不算在`#include`/`#define`区块内;`.h`没有`using namespace`,两行空行之后直接是第一个类型/声明。
  - 不要把`using namespace std;`夹在`#include`和`#define`中间——`using namespace`只能出现在整个`#include`+`#define`区块**之后**的两个空行下面。

### Forever UE模块层

- 遵循UE惯例:`A`/`U`/`F`/`I`前缀,`UCLASS`/`USTRUCT`/`UPROPERTY`/`UFUNCTION`等
- 成员变量用camelCase(而非Epic官方PascalCase),和Dependence/Core/Basic层保持一致风格
- 函数/方法用PascalCase(UE标准)

## 3. Source模块划分

- `Source/Dependence`、`Source/Core`:两个独立的VS静态库工程(`.vcxproj`),与UE完全无关的纯C++,产出`.lib`到工程根目录`x64/<Config>/`。用独立的`Source/Framework.sln`维护,和UE自动生成的`Forever.sln`完全解耦。
- `Source/Basic`:**动态库工程**(`ConfigurationType=DynamicLibrary`),产出`Basic.dll`到同一个`x64/<Config>/`目录,但**不会**被`Forever.Build.cs`静态链接——它是内置的默认Mod集合,和`Forever_Mod/`下的第三方Mod地位完全相同,由`Config`/`ModLoader`在运行时扫描`Basic.dll`导出的`GetMod<Concept>`/`RegisterMod<Concept>`/`FinishMod<Concept>`符号加载。因此`Source/Basic`下的代码要遵守和Mod DLL相同的约束(只`#include` Dependence头文件、只链接`Dependence.lib`、不出现任何UE类型),不能反过来依赖`Source/Core`。
- `Source/Forever`:UE Runtime模块,通过`Forever.Build.cs`的`PublicAdditionalLibraries`手动链接`Dependence.lib`/`Core.lib`,不链接也不需要链接`Basic`(它在运行时以dll形式被发现)。
- `Source/Test`:测试。`Test/Mod`是`ModTest`(给mod作者用的mod自测程序,只依赖Dependence,不需要Core和UE);`Test/Core`是`CoreTest`(不带UE直接跑Core+真实mod dll,项目自己用);`Test/UE`是带画面的UE测试模块`ForeverTest`。前两个都在`Framework.sln`里,用法见`Source/Test/README.md`。
- **Core和Forever(UE)层不感知任何具体mod**:只处理所有mod共性的内容——通过`<Concept>Mod`基类的字段/虚方法、Factory注册的静态函数指针、脚本词汇表(Change/Event)和mod打交道,不出现具体mod的id、类型名或只对某个mod成立的分支;某个mod需要的新能力要抽象成所有mod都能用的共性接口再加进Dependence。
- Mod(`Forever_Mod/`下各独立目录,以及`Source/Basic`本身)编译成不依赖UE运行时的纯Win32 DLL,源码级include Dependence头文件+链接`Dependence.lib`来继承`BuildingMod`等抽象基类,运行时被`LoadLibrary`/`GetProcAddress`加载。这是Dependence/Core必须保持engine-agnostic、Basic和Mod必须只依赖Dependence的根本原因。
- 非UE资源(`layouts`/`story`脚本等)放在`Forever_UE/Resource`下,与`Source`、`Content`平级,不挂在`Source`目录里;任何读取这类资源路径的代码都要用这个位置。

## 4. 跨模块(跨DLL) `new`/`delete` 安全

整个项目靠加载大量DLL(主程序 + Dependence/Core/Basic三层 + 各Mod)实现实际逻辑,跨DLL边界的内存分配/释放处理不当会导致堆错乱、崩溃。这类问题的根本原因都是同一个:**宿主和mod用的不是同一个内存分配器,而同一份源码在两边各有一份编译产物**。

具体是这三件事叠在一起:

- **UE模块的`new`/`delete`不走C运行时。** `UnrealEditor-Forever.dll`(打包版是游戏exe)是UE模块,UE给每个模块都换掉了全局`operator new`/`delete`,走引擎自己的`FMemory`(宏`REPLACEMENT_OPERATOR_NEW_AND_DELETE`,定义在引擎的`Modules/Boilerplate/ModuleBoilerplate.h`;每个模块经`HAL/PerModuleInline.inl`各带上一份,崩溃调用栈里看到的就是这个文件)。`Core.lib`和`Dependence.lib`是静态链接进这个模块的,所以Core的代码在游戏里分配内存也走`FMemory`。
- **mod dll的`new`/`delete`走C运行时的堆。** `Basic.dll`和`Forever_Mod/*`的dll不是UE模块,没有被换掉。
- **`Dependence.lib`是静态库,链接进谁就在谁那里多一份编译产物。** 同一个函数(比如`Lot`的某个非虚方法、`JsonValue::AsString`)在宿主里有一份,在每个mod dll里又各有一份;谁调用就执行谁那一份,用的也是谁的分配器。

所以对同一块内存,分配和释放如果发生在两侧各自编译的代码里,就是`FMemory`去释放C运行时分配的内存(或者反过来),直接堆损坏。

各工程的运行库设置都是`/MD`(动态链接同一个C运行时)。**这不解决问题**:共用运行时只是让mod和mod之间、mod和普通exe之间用上了同一个堆,UE那一侧仍然是`FMemory`。由此有几点要记住:

- **只有在UE里才会崩**(编辑器和打包版都会)。CoreTest、ModTest、`Tools`下的程序是普通exe,和mod用的是同一个C运行时的堆,这类错误在它们里面不会暴露。**测试通过不能说明跨DLL的分配是安全的**,只能靠下面的规则在写代码时守住。
- **短字符串不暴露问题。** `std::string`不超过15个字符时存在对象内部、不分配,超过了才上堆。"一直好好的,改了个名字就崩"是这类问题的典型表现(`"mountain"`改成`"terrain_mountain"`那次就是)。
- **规则按"任何两个模块的分配器都可能不同"来守**,不要依赖"mod之间其实是同一个堆":调试版(`/MDd`)和发布版就是两个堆(`Config::AddDllPath`因此跳过调试版dll),mod作者也可能用别的工具链编译。
- **怎么认出来。** 两种表现:①退出或关卡销毁时崩,调用栈是某个Core层的析构函数 → `std::vector<T>::_Tidy()` / `basic_string::_Tidy_deallocate` → `operator delete` → `HAL/PerModuleInline.inl`;②一碰到就崩且`Saved/Crashes`里没有崩溃报告(`STATUS_HEAP_CORRUPTION` `0xc0000374`绕过了UE的异常处理),要到Windows的应用程序事件日志里看。

具体落地手法:

- **跨DLL暴露的类,所有会被另一侧DLL调用的公开方法(含析构函数)必须是`virtual`,即使当前没有任何派生类覅要重写**。原因:非虚方法在调用方(如Mod DLL)里会被**静态绑定**到调用方自己链接的那份实现代码,即使`this`指针指向的对象是宿主(UE进程)`new`出来的——这份被静态绑定的实现如果在内部做了任何涉及内存分配/释放的操作(往`unordered_map`插入节点、`std::string`扩容等),实际使用的是调用方自己的CRT分配器,但对象本身的生命周期(构造/析构)可能发生在宿主那一侧,两者对不上。标成`virtual`后,调用会通过对象自带的vtable指针跳转,而vtable是宿主构造对象时装配的,永远指向宿主自己编译的那份实现——分配和释放因此总落在同一侧。这个坑在`<Concept>Factory`上实测复现过:`RegisterX`等方法是非虚函数时,`ScriptFactory`析构时崩溃在`std::list::~list()`(`unordered_map`内部节点链表的析构),标`virtual`后问题消失。
- **跨DLL传递的对象,销毁必须用分配它的那个模块提供的销毁函数(deleter),不能由另一侧直接`delete`**。`<Concept>Factory::Destroy<Concept>`的实现方式是示范:用`liveInstances`(实例指针→注册id的映射)反查该实例是哪个mod注册的,再调用该mod注册时一并提供的`deleter`函数指针释放,Factory自己永远不对一个由Mod DLL `new`出来的对象调`delete`。
- **Mod DLL不能对Core拥有的对象的STL容器调非虚的mutator方法**(如直接`push_back`/`insert`一个Core端已经持有的`vector`/`map`)——同样是"调用方用自己的分配器操作另一侧分配的容器内部结构"的变体,哪怕看起来只是"调用一个容器方法"而不是直接`new`/`delete`。跨DLL交互要通过宿主导出的、方法体编译在宿主自己那一侧的接口(如虚函数、或者裸函数指针回调,回调函数体本身编译在调用方那一侧,见下一条)。
- **裸函数指针跨DLL传递是安全的**,只要函数体本身是不捕获任何跨DLL状态的static/自由函数:调用方调用这个指针触发的任何容器操作,实际执行的都是**定义这个函数的那一侧**自己的分配器,不会出现"一侧分配、另一侧释放"的问题。`<Concept>Factory::RegisterBuilding`等除了`creator`/`deleter`两个函数指针,还可以按需注册更多这样的查询用静态函数指针(如`RandomAcreageFunc`/`AssignFunc`),这类纯数据输入输出、不涉及容器所有权转移的查询,比起"分配一个实例、虚方法调用"更轻量。
- **每次调整模块边界或新增跨DLL传递对象的地方,都要重新检查该处的分配/释放是否仍然安全**;发现新的跨模块new/delete风险点时要主动修复,不能绕过或留到以后处理。涉及跨模块生命周期管理的`.md`文档要专门记录清楚对应的安全手法。

## 5. `<Concept>Mod`/`<Concept>Factory`可扩展点的既定设计

- `<Concept>Factory`和对应的`<Concept>Mod`**同放在`Source/Dependence`,不放在`Source/Core`**——Mod DLL只链接`Dependence.lib`,如果`Factory`的方法实现编译进`Core.lib`,Mod就无法调用`factory->RegisterXxx(...)`这个方法。
- **`GetId()`不放进`<Concept>Mod`基类**,只是具体mod子类自己的静态方法约定(如`ResidenceHighBuilding::GetId()`),不通过基类指针调用,也不是DLL导出符号本身。**mod id可以是任意字符串**:`<concept>_<name>`(如`building_residence_high`、占位mod`<concept>_empty`)只是一个弱约定,**任何地方的实现都不能假定mod id的格式**——不按前缀/后缀判断它属于哪个concept或是不是占位,要判断就去问对应的Factory(`CheckRegistered`)或看提供它的dll。**所有mod的id必须全局唯一**——不管属于哪个concept、由哪个dll提供,两个mod不能用同一个id(随机流、依赖表、`Registry::CheckModRegistered`、脚本的`mod_dependences`都只按id找mod);不支持不同concept共用一个id。由测试保证:`ModTest`对它加载的全部dll查重名(mod作者手里没有别人的mod,所以**项目这边收集到新mod后必须把全部mod dll一起跑一遍ModTest**才能查全),CoreTest的mods套件对`config.json`的`dll_paths`再查一次。`Source/Basic`里每个mod一组以自己id命名的文件(`<id>.h/.cpp/.md`)。
- **给mod传参数的唯一路径是`Factory::SetModArgs`+`Create<Concept>(id)`创建实例那一刻把参数字符串直接传给注册的creator函数**,不经过DLL导出函数的参数,也不经过创建后再调用的钩子方法。`config.json`里每个concept一个`"<concept>_mods"`数组,元素是`"id"`或`"id 参数..."`(命令行式写法);`CreateFunc`的函数指针类型是`<Concept>Mod*(*)(const std::string&)`,`RegisterX`只登记creator/deleter,不复制参数;mod自己的creator决定怎么用参数(传给具体子类构造函数最常见)。**参数串统一用`Dependence/common/mod_args.h`的`ModArgs`解析**(`--key value`/`--key=value`/`--flag`，取值带默认值和范围夹取，见`mod_args.md`)，mod里不要再自己`istringstream`/`find("--xxx")`手写解析。**两个被否掉的方案不要重新尝试**:按`dll_paths`根目录配一份共用参数经导出函数传给mod(和"每个mod id有自己的参数"的真实需求对不上);创建实例**之后**再调用`instance->ApplyArgs(args)`钩子(此时构造函数已经跑完,`const`成员没法再赋值,参数到手已经太晚)。
- **所有`XxxMod`具体子类的`GetName()`返回值必须在整个进程生命周期内全局唯一**——不能是固定的类型名字符串,同一类型创建多个实例时每个实例的名字都要不一样。统一用`static int count`+构造函数`id(count++)`+`GetName()`里现拼`name = <前缀> + std::to_string(id)`返回`name.data()`这个固定模式实现:

```cpp
// .h
private:
	static int count;
	int id;
	std::string name;
// .cpp
int OceanTerrain::count = 0;
OceanTerrain::OceanTerrain() : id(count++) { }
const char* OceanTerrain::GetName() {
	name = "海洋地形" + to_string(id);
	return name.data();
}
```

不要用"构造函数里一次性拼好存进一个成员、`GetName()`只返回这个成员"的变体——两种写法效果上都能保证唯一性,但风格必须统一,全项目只用上面这一种。

### 哪些对象可以没有mod

一个concept的mod就是这个东西的定义，包装类拿不到mod就不能存在：建筑/房间/地形这类缺了世界就生成不出来的抛`DataException`；产品/仓库/工坊/武器/载具/物品这类缺了只是少一样东西的，创建函数返回空并打一条警告(同一个mod只报一次)，不许悄悄跳过。

**脚本是例外：`ScriptMod`可以没有。** 脚本的内容来自`.script`文件，`ScriptMod`只是"匹配出一批动作之后改写它们"的C++钩子。`scriptModName`留空=这个脚本没有钩子，`Script`照常创建、动作原样执行(`Core/story/script.md`)。所以Core不兜底到任何具体的脚本mod，各concept的`scriptModName`默认值都是空串。别的concept不要照这个做——它们的mod不是可选的附加逻辑。

## 6. Player类的资产引用方式

Player相关的C++类(`ForeverCharacter`/`ForeverPlayerController`/`ForeverPlayerGameMode`/`ForeverPlayerState`)不建Blueprint外壳,直接是可用类,用`ConstructorHelpers::FObjectFinder`/`FClassFinder`在构造函数里引用默认资产(骨骼网格、动画蓝图、Input Mapping Context/Action等)。这只是设置默认值,不影响后续换资产或走向运行时数据驱动换装——子类或运行时代码随时可以覆盖这些`EditDefaultsOnly`属性。

## 7. Enhanced Input映射上下文挂在被占有的Pawn上,不挂在Controller上

需要支持"同一个Controller先后占有不同类型的Pawn(步行角色/载具等),每种Pawn要用不同的输入映射"这种场景时,Enhanced Input的`UInputMappingContext`添加/移除要放在Pawn自己的`PossessedBy`/`UnPossessed`里,不要放在Controller的`BeginPlay`/`OnPossess`里——Controller在整个游玩过程中只有一个实例,把映射上下文跟着Controller走,换Pawn时无法方便地切换成这个新Pawn专属的映射集合。

## 8. 蓝图逻辑获取方式:只凭dump内容写C++

迁移任何具体蓝图(`Blueprint/Framework/*`、`Blueprint/Element/*`、`Blueprint/UI/*`等)的图表逻辑时,必须先由用户dump该蓝图的实际内容(图表、变量、事件绑定),再据此写C++——不在没有dump内容的情况下凭空还原/猜测蓝图逻辑。dump文件本身不要删除,后续如果发现当时分析有遗漏,仍需要回去打开dump文件逐条核对。

## 9. 外部Mod插件(`Forever_Mod/*`)不在`Framework.sln`里,改公共头文件要记得单独重新编译

`Forever_Mod/Empty`、`Forever_Mod/Test`、`Forever_Mod/Yuanshen`、`Forever_Mod/Wxdj`各自是独立的`.sln`工程,产出自己的DLL,运行时由`Config`/`ModLoader`按`config.json`的`dll_paths`扫描加载——它们`#include`和`Source/Basic`相同的`Dependence`头文件,但**不会**随`Framework.sln`的编译自动重新生成。修改任何`Source/Dependence`下的头文件(尤其是`<Concept>Mod`基类新增字段)后,除了重新编译`Framework.sln`,**必须额外手动编译这几个外部Mod工程**,否则它们的DLL仍然按旧的(更短的)结构体布局分配对象,Core之后按新布局读取这些对象会读到越界内存,容易表现成"看似随机但其实是读到了旧对象末尾之外的内存"的诡异崩溃或读数(实测复现过一次:一个被追加了新字段的`Mod`基类,某个外部Mod插件没跟着重新编译,导致该插件创建的实例在读取新字段时读出一个看起来合理却完全是垂悬数据的数字)。

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" "<repo>\Forever_Mod\Empty\Cpp\Empty.sln" /p:Configuration=Release /p:Platform=x64 /m
& "...\MSBuild.exe" "<repo>\Forever_Mod\Test\Cpp\Test.sln" /p:Configuration=Release /p:Platform=x64 /m
& "...\MSBuild.exe" "<repo>\Forever_Mod\Yuanshen\Cpp\Yuanshen.sln" /p:Configuration=Release /p:Platform=x64 /m
& "...\MSBuild.exe" "<repo>\Forever_Mod\Wxdj\Cpp\Wxdj.sln" /p:Configuration=Release /p:Platform=x64 /m
```

## 10. C++改动必须跑一次真实编译才能算完成

任何Dependence/Core/Basic/Forever层的C++改动,汇报"完成"之前必须真实跑一次对应的编译(`Framework.sln`用MSBuild,`Forever`模块用UBT的`Build.bat`),不能只凭代码审查/人工检查判断"应该没问题"——过去出现过纯靠审查漏掉真实编译错误的情况。涉及`Source/Dependence`公共头文件的改动,按上一条同时重新编译受影响的外部Mod插件。

## 11. UE工程特定的几个硬性约束(不是通用知识,容易被忽略)

- 这个项目的`DefaultEngine.ini`里`Pawn`碰撞预设**忽略`ECC_Visibility`**——任何意图打到角色身上的射线检测(武器开火、瞄准等)必须用`ECC_Pawn`通道,不能想当然用`ECC_Visibility`。
- `ACitizenElement`默认关闭Actor Tick(性能优化),玩家附身(`PossessedBy`)一个市民之后如果该市民的原生`Tick()`逻辑里还存在"无待处理路径点就自动再关一次Tick"这类逻辑,要连同`PossessedBy`本身一起检查,否则被操控角色身上依赖`Tick()`的功能会静默失效。

## 12. 存档格式随便改,不考虑兼容

游戏还没发布,也没有需要保留的旧存档(每次测试都重新存)。改存档数据布局(`.fsav`的段、文件外壳、mod状态)时:直接按新格式改读写,**不用**考虑旧存档能不能读,**不升**`SAVE_FILE_VERSION`/各段版本号/mod的`stateVersion`,**不写**旧版本迁移代码,也不要因为"不想动存档格式"而把本该存的状态省掉。只有用户明确要求升版本时才升。详见`Source/Core/common/save.md`的"版本号"。

## 13. mod dll的生命周期:只在一局游戏期间留在进程里

**规则**:mod dll在开局时加载(`AForeverFrameworkActor::BeginPlay`的`Registry::RefreshMods`),一局结束、七个域都销毁之后注销并卸载(`EndPlay`里的`Registry::UnloadMods`)。菜单界面上——不管是刚打开程序,还是打完一局退回来——进程里**没有任何mod dll**,所以菜单里可以随意添加、去掉、替换dll。编辑器和打包版是同一套逻辑。设计见`Forever_UE/Source/Core/common/registry.md`"卸载"。

**dll一卸载,它的代码、虚函数表、静态数据就都不在内存里了。**这时还指进去的任何指针,下次用到就崩,而且崩的位置离原因很远。会指进mod dll的东西:

- mod实例(`XxxMod*`):虚函数表在dll里。Core里包着它的对象(`Building`/`Citizen`/`Vehicle`/`Script`/`Asset`/`Puzzle`/`Weapon`…)同理。
- mod自己`new`出来交给宿主的对象(比如mod造的`Change`):`Change`这类的代码在静态库里,每个dll各有一份,mod造的对象用的是mod那份虚函数表。
- mod返回的`const char*`、`const std::string&`,以及任何指向mod内部数据的指针/引用。
- 从dll里取出来的函数指针(Factory里的注册项就是)。

### 写UE代码时要守的

- **活得比游戏关卡长的对象不能持有上面任何一种东西。**这类对象包括:`UGameInstanceSubsystem`/`UEngineSubsystem`、`static`/全局变量、菜单关卡的Widget、跨关卡保留的`UObject`。它们只能存拷贝出来的值(`FString`、数字、自己`new`的数据),不能存Core对象的指针。
- **mod返回的字符串当场拷贝**(`UTF8_TO_TCHAR(...)`构造`FString`,或拷进自己的`std::string`),不要把`const char*`存起来以后再用。
- **凡是经Factory创建、由UE对象自己持有的东西,必须在`AForeverFrameworkActor::EndPlay`卸载之前销毁。**关卡里的Actor/Widget的销毁可能晚于`EndPlay`,不能指望析构顺序——在`EndPlay`里显式收掉(例子:`UPuzzleWidget`持有`Puzzle`,`EndPlay`里调`AbortPuzzle()`,Widget的`NativeDestruct`里再兜底一次)。新加这类对象时照这个模式做。
- **菜单里的代码不要碰`Registry`和各Factory。**菜单只用`Config`(它只在添加路径时临时打开dll读一下id,马上关掉)。确实需要Factory的(读档前核对mod状态版本),用之前调`Registry::RefreshMods()`,用完如果不进游戏就调`Registry::UnloadMods()`——参照`UForeverSaveSubsystem::RequestLoad`。**游戏进行中绝不能调`UnloadMods`**。
- **会碰Core的异步工作要在`EndPlay`卸载之前跑完**(后台线程、`Async`、定时器、延迟回调)。现有的异步寻路在`EndPlay`开头`WaitForPendingPathfinding()`。新加的异步任务如果读Core对象或调mod,要有同样的等待;纯UE数据(比如截图写盘)的不受影响。
- **`EndPlay`之后别再用Core指针。**关卡里的Actor/组件手里的`Citizen*`/`Vehicle*`这类裸指针在`EndPlay`之后全部是悬空的,它们的`EndPlay`/`BeginDestroy`/`Tick`里不能解引用。

### 兜底和怎么发现问题

- `UnloadMods`卸载前会数23个Factory里还活着的mod实例。还有活实例的dll**不卸载**(连同它的注册项一起留下),其余照常卸载,并在`Forever.log`里打一条`LogForeverCore: Warning: Registry::UnloadMods: ... still alive after the game ended (leak)`,写明是哪个mod id、几个实例、留下了哪个dll。**看到这条就是有东西没跟着一局结束销毁,要去修,不是正常现象。**
- 这个兜底只看得见mod实例。别处存下的函数指针、字符串指针它发现不了——所以上面几条规则要靠写代码时自己守。
- Core这一侧由CoreTest的`unload`套件查:销毁世界后没有活着的`Script`和mod实例、dll确实离开进程、内存不逐轮上涨、卸载重载后同种子的世界和第一次一致。UE这一侧没有自动化测试,靠"进游戏→退回菜单→再进游戏"多试几轮,并留意上面那条警告。

### 做不到的

mod的UE内容(打包版的`.pak`、编辑器里挂载的Content目录)挂载之后不卸载,到进程退出为止。去掉一个带内容的mod,dll会走,pak文件仍被占用;不退出程序替换同一个mod的pak不会生效。

### 对mod作者的要求

mod不能留下会在卸载后被调用的东西:不开线程、不向系统或别的库注册回调。mod里的`static`状态每局都会重置(dll是重新加载的),不要指望它跨局保留。这两条也写在`Forever_Mod/README.md`里。

## 14. 异常:抛哪一类、在哪接、接住之后怎么办

细节见`Source/Dependence/common/error.md`(异常体系本身)和`Source/Forever/Framework/CoreGuard.md`(UE那一层)。这里只列规则。

### 抛

- 只有三类,按"接住之后该怎么办"分:`DataException`(外部数据写错了:config、layout、脚本、存档、没启用的mod id)、`ModException`(mod交给Core的东西自相矛盾)、`InternalException`(程序自己的不变量被破坏)。拿不准时:改数据文件能修好的是Data,要改mod的C++的是Mod,要改Core/Dependence的是Internal。
- 用`THROW_EXCEPTION(类, 消息)`抛,不要直接`throw`。
- 析构函数里不抛;排序比较器这类被标准库反复调用的回调里不抛。
- 不用异常做流程控制。"试着解析一下"用`TryParseInt`/`TryParseDouble`/`Time::TryParse`,不要写`try { stoi(...) } catch (...) {}`。

### 登记"当时正在做什么"

- 宿主(Core/UE)调mod的代码一律写成`MOD_CALL(mod, mod->Xxx(...))`;工厂里调mod登记的函数指针写成`MOD_ID_CALL(id, ...)`。出错时报告里就有mod的id,不用在调用点写try/catch。
- 读一个数据文件、做一个有名字的阶段时放一个`ErrorScope`(`kErrorScopeFile`/`kErrorScopeStep`/`kErrorScopeScript`)。它只存指针:传进去的字符串要活得比它久。

### 接

分三层,从里到外:

1. **一个可以整体跳过的操作**(一条脚本指令、一个里程碑的触发条件、一段对话):用`GuardData`。只接数据错误,打一条警告,跳过这一个;mod错误和内部错误原样往外抛。
2. **一次有明确失败返回值的加载**(存档各段、`Config::ReadConfig`、菜单里的存档/读档/开始游戏):什么异常都接,变成错误文字返回给调用方。前提是这次加载动过的东西在失败时整份丢掉,不留半份。
3. **会话边界**(UE层):引擎直接调进来、里面会调Core的函数,函数体用`CORE_GUARD_BEGIN`/`CORE_GUARD_END`(或`CoreGuard::Run`)包住。接住之后这一局结束、回菜单并显示原因——世界已经是半更新的,不能接着Tick。**新写这样的函数时要自己加上**,否则异常会穿进UE引擎的栈帧(引擎编译时没开异常),编辑器里是整个编辑器崩。

不允许空的`catch {}`:接住了至少打一条警告,说明丢掉了什么。(唯一的例外在`ErrorScope`自己的析构函数里:异常展开途中记不下轨迹时不能再抛。)

### 接的写法

- 一律`catch (const X&)`或`catch (...)`,不按值接。
- 接住后第一件事是`DescribeCurrentException()`,拿到一份全是自己这一侧分配的`ErrorReport`;之后只用这份报告,不再碰异常对象,更不把异常对象存起来以后处理。原因:异常可能是mod dll抛的,它的析构函数和类型信息都在那个dll里,而mod dll一局结束就卸载(第13节),内存分配器也和宿主不是同一个(第4节)。
- `ExceptionBase`不要加"按值返回`std::string`"的虚函数,理由同上。

### 接不住的

空指针解引用、除零、栈溢出不是C++异常,这套东西接不住,也不去接。

### 怎么验证

CoreTest的`fault`套件逐条验证上面的规则:三类异常的归类、`ErrorScope`的顺序和过期清理、`GuardData`只接数据错误、坏config/脚本/layout、mod dll里抛出的三种异常(`Forever_Mod/Test`的`puzzle_fault`)、出错之后世界仍能销毁且mod dll仍能卸干净。它验证不了两件事:UE层`CoreGuard`的回菜单流程(只能在编辑器里看);以及宿主和mod分配器不同带来的问题——CoreTest是普通exe,和mod用的是同一个堆,这类问题只在UE里才会出现。
