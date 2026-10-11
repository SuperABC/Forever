# 约定文档

本文档记录整个项目(内核迁移 + 新功能开发)过程中确立的约定,新开的session都要遵守。

## 1. `.md`配对文档模板

每个逻辑独立的`.h`/`.cpp`(一个类或紧密相关的一组类)配一份同名`.md`,记录设计思路,取代代码里的大段中文块注释。**Dependence/Core/Basic层强制要求**;Forever UE模块层的类也建议配,尤其是逻辑不止"数据搬运"的类。一批没有独特逻辑的空实现(如早期的`<Concept>Mod`/`<Concept>Basic`占位骨架)可以共用一份文档,避免大量重复样板文字;某个成员从占位升级为真正实现后,要从共用文档"毕业",改配自己的独立`.md`。

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
- 成员变量:camelCase(如`maxAcreage`、`wallTexture`)
- 文件名、domain文件夹名:snake_case(如`building_mod.h`、`map/`)
- 只用`std::`容器和纯C++类型,**不出现任何UE头文件/类型**(`FString`/`TArray`/`UObject`等一律禁止),因为Mod侧是不依赖UE运行时的纯Win32 DLL,依赖二进制兼容
- 函数级中文说明移入配对`.md`,代码里不再写块注释;非显而易见的行内WHY注释仍可保留
- 前向引用集中在`Core/common/class.h`:按domain分组(`// Common`/`// Map`/`// Populace`/`// Society`/`// Story`/`// Industry`/`// Traffic`/`// Player`)声明所有跨文件用到的`class Foo;`。Core下其余`.h`/`.cpp`不再自己写任何前向引用,一律`#include "class.h"`(项目IncludePath已把`Core/common`加进去,不用带`common/`前缀);新增一个需要前向引用的类型时,直接加进`class.h`对应分组,不要在具体文件里就地声明
- **每个concept的`.h`必须配一份同名`.cpp`,哪怕逻辑全写在`.h`里(纯虚接口+inline构造函数之类)也要建**——让每个头文件都有一个专属的编译单元:①验证头文件本身`#include`齐全、不依赖"被别的头文件带出来的东西"才能编译(header self-contained检查);②和`.md`配对文档、vcxproj里`ClCompile`/`ClInclude`成对出现的既有习惯保持一致。新增一个`<Concept>Mod`/`<Concept>Factory`/`<Concept>Basic`时,`.h`/`.cpp`要一起建,`.cpp`记得同步加进对应`.vcxproj`的`ClCompile`和`.vcxproj.filters`的对应Filter——这一项反复出现过筛选器/编译列表漏加的问题,新建文件后务必两处一起检查。
- **文件头`#include`顺序+空行规则**(完整示例见`Core/map/map.h`/`.cpp`和`Basic/map/terrain_basic.*`/`Dependence/map/terrain_mod.h`/`terrain_factory.*`):
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
- Mod(`Forever_Mod/`下各独立目录,以及`Source/Basic`本身)编译成不依赖UE运行时的纯Win32 DLL,源码级include Dependence头文件+链接`Dependence.lib`来继承`BuildingMod`等抽象基类,运行时被`LoadLibrary`/`GetProcAddress`加载。这是Dependence/Core必须保持engine-agnostic、Basic和Mod必须只依赖Dependence的根本原因。
- 非UE资源(`layouts`/`story`脚本等)放在`Forever_UE/Resource`下,与`Source`、`Content`平级,不挂在`Source`目录里;任何读取这类资源路径的代码都要用这个位置。

## 4. 跨模块(跨DLL) `new`/`delete` 安全

整个项目靠加载大量DLL(主程序 + Dependence/Core/Basic三层 + 各Mod)实现实际逻辑,跨DLL边界的内存分配/释放处理不当会导致堆错乱、崩溃。这类问题的根本原因都是同一个:**C++没有跨DLL统一的全局`operator new`/`delete`,不同DLL各自静态链接一份CRT,对同一块内存,分配和释放如果发生在不同DLL编译的代码里,两边用的分配器状态不一致,直接堆损坏**。具体落地手法:

- **跨DLL暴露的类,所有会被另一侧DLL调用的公开方法(含析构函数)必须是`virtual`,即使当前没有任何派生类覅要重写**。原因:非虚方法在调用方(如Mod DLL)里会被**静态绑定**到调用方自己链接的那份实现代码,即使`this`指针指向的对象是宿主(UE进程)`new`出来的——这份被静态绑定的实现如果在内部做了任何涉及内存分配/释放的操作(往`unordered_map`插入节点、`std::string`扩容等),实际使用的是调用方自己的CRT分配器,但对象本身的生命周期(构造/析构)可能发生在宿主那一侧,两者对不上。标成`virtual`后,调用会通过对象自带的vtable指针跳转,而vtable是宿主构造对象时装配的,永远指向宿主自己编译的那份实现——分配和释放因此总落在同一侧。这个坑在`<Concept>Factory`上实测复现过:`RegisterX`等方法是非虚函数时,`ScriptFactory`析构时崩溃在`std::list::~list()`(`unordered_map`内部节点链表的析构),标`virtual`后问题消失。
- **跨DLL传递的对象,销毁必须用分配它的那个模块提供的销毁函数(deleter),不能由另一侧直接`delete`**。`<Concept>Factory::Destroy<Concept>`的实现方式是示范:用`liveInstances`(实例指针→注册id的映射)反查该实例是哪个mod注册的,再调用该mod注册时一并提供的`deleter`函数指针释放,Factory自己永远不对一个由Mod DLL `new`出来的对象调`delete`。
- **Mod DLL不能对Core拥有的对象的STL容器调非虚的mutator方法**(如直接`push_back`/`insert`一个Core端已经持有的`vector`/`map`)——同样是"调用方用自己的分配器操作另一侧分配的容器内部结构"的变体,哪怕看起来只是"调用一个容器方法"而不是直接`new`/`delete`。跨DLL交互要通过宿主导出的、方法体编译在宿主自己那一侧的接口(如虚函数、或者裸函数指针回调,回调函数体本身编译在调用方那一侧,见下一条)。
- **裸函数指针跨DLL传递是安全的**,只要函数体本身是不捕获任何跨DLL状态的static/自由函数:调用方调用这个指针触发的任何容器操作,实际执行的都是**定义这个函数的那一侧**自己的分配器,不会出现"一侧分配、另一侧释放"的问题。`<Concept>Factory::RegisterBuilding`等除了`creator`/`deleter`两个函数指针,还可以按需注册更多这样的查询用静态函数指针(如`RandomAcreageFunc`/`AssignFunc`),这类纯数据输入输出、不涉及容器所有权转移的查询,比起"分配一个实例、虚方法调用"更轻量。
- **每次调整模块边界或新增跨DLL传递对象的地方,都要重新检查该处的分配/释放是否仍然安全**;发现新的跨模块new/delete风险点时要主动修复,不能绕过或留到以后处理。涉及跨模块生命周期管理的`.md`文档要专门记录清楚对应的安全手法。

## 5. `<Concept>Mod`/`<Concept>Factory`可扩展点的既定设计

- `<Concept>Factory`和对应的`<Concept>Mod`**同放在`Source/Dependence`,不放在`Source/Core`**——Mod DLL只链接`Dependence.lib`,如果`Factory`的方法实现编译进`Core.lib`,Mod就无法调用`factory->RegisterXxx(...)`这个方法。
- **`GetId()`不放进`<Concept>Mod`基类**,只是具体mod子类自己的静态方法约定(如`PengzhanBuilding::GetId()`),不通过基类指针调用,也不是DLL导出符号本身。
- **给mod传参数的唯一路径是`Factory::SetModArgs`+`Create<Concept>(id)`创建实例那一刻把参数字符串直接传给注册的creator函数**,不经过DLL导出函数的参数,也不经过创建后再调用的钩子方法。`config.json`里每个concept一个`"<concept>_mods"`数组,元素是`"id"`或`"id 参数..."`(命令行式写法);`CreateFunc`的函数指针类型是`<Concept>Mod*(*)(const std::string&)`,`RegisterX`只登记creator/deleter,不复制参数;mod自己的creator决定怎么用参数(传给具体子类构造函数最常见)。**两个被否掉的方案不要重新尝试**:按`dll_paths`根目录配一份共用参数经导出函数传给mod(和"每个mod id有自己的参数"的真实需求对不上);创建实例**之后**再调用`instance->ApplyArgs(args)`钩子(此时构造函数已经跑完,`const`成员没法再赋值,参数到手已经太晚)。
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

## 6. Player类的资产引用方式

Player相关的C++类(`ForeverCharacter`/`ForeverPlayerController`/`ForeverGameMode`/`ForeverPlayerState`)不建Blueprint外壳,直接是可用类,用`ConstructorHelpers::FObjectFinder`/`FClassFinder`在构造函数里引用默认资产(骨骼网格、动画蓝图、Input Mapping Context/Action等)。这只是设置默认值,不影响后续换资产或走向运行时数据驱动换装——子类或运行时代码随时可以覆盖这些`EditDefaultsOnly`属性。

## 7. Enhanced Input映射上下文挂在被占有的Pawn上,不挂在Controller上

需要支持"同一个Controller先后占有不同类型的Pawn(步行角色/载具等),每种Pawn要用不同的输入映射"这种场景时,Enhanced Input的`UInputMappingContext`添加/移除要放在Pawn自己的`PossessedBy`/`UnPossessed`里,不要放在Controller的`BeginPlay`/`OnPossess`里——Controller在整个游玩过程中只有一个实例,把映射上下文跟着Controller走,换Pawn时无法方便地切换成这个新Pawn专属的映射集合。

## 8. 蓝图逻辑获取方式:只凭dump内容写C++

迁移任何具体蓝图(`Blueprint/Framework/*`、`Blueprint/Element/*`、`Blueprint/UI/*`等)的图表逻辑时,必须先由用户dump该蓝图的实际内容(图表、变量、事件绑定),再据此写C++——不在没有dump内容的情况下凭空还原/猜测蓝图逻辑。dump文件本身不要删除,后续如果发现当时分析有遗漏,仍需要回去打开dump文件逐条核对。

## 9. 外部Mod插件(`Forever_Mod/*`)不在`Framework.sln`里,改公共头文件要记得单独重新编译

`Forever_Mod/Empty`、`Forever_Mod/Test`、`Forever_Mod/Wxdj`各自是独立的`.sln`工程,产出自己的DLL,运行时由`Config`/`ModLoader`按`config.json`的`dll_paths`扫描加载——它们`#include`和`Source/Basic`相同的`Dependence`头文件,但**不会**随`Framework.sln`的编译自动重新生成。修改任何`Source/Dependence`下的头文件(尤其是`<Concept>Mod`基类新增字段)后,除了重新编译`Framework.sln`,**必须额外手动编译这几个外部Mod工程**,否则它们的DLL仍然按旧的(更短的)结构体布局分配对象,Core之后按新布局读取这些对象会读到越界内存,容易表现成"看似随机但其实是读到了旧对象末尾之外的内存"的诡异崩溃或读数(实测复现过一次:一个被追加了新字段的`Mod`基类,某个外部Mod插件没跟着重新编译,导致该插件创建的实例在读取新字段时读出一个看起来合理却完全是垂悬数据的数字)。

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" "<repo>\Forever_Mod\Empty\Cpp\Empty.sln" /p:Configuration=Release /p:Platform=x64 /m
& "...\MSBuild.exe" "<repo>\Forever_Mod\Test\Cpp\Test.sln" /p:Configuration=Release /p:Platform=x64 /m
& "...\MSBuild.exe" "<repo>\Forever_Mod\Wxdj\Cpp\Wxdj.sln" /p:Configuration=Release /p:Platform=x64 /m
```

## 10. C++改动必须跑一次真实编译才能算完成

任何Dependence/Core/Basic/Forever层的C++改动,汇报"完成"之前必须真实跑一次对应的编译(`Framework.sln`用MSBuild,`Forever`模块用UBT的`Build.bat`),不能只凭代码审查/人工检查判断"应该没问题"——过去出现过纯靠审查漏掉真实编译错误的情况。涉及`Source/Dependence`公共头文件的改动,按上一条同时重新编译受影响的外部Mod插件。

## 11. UE工程特定的几个硬性约束(不是通用知识,容易被忽略)

- 这个项目的`DefaultEngine.ini`里`Pawn`碰撞预设**忽略`ECC_Visibility`**——任何意图打到角色身上的射线检测(武器开火、瞄准等)必须用`ECC_Pawn`通道,不能想当然用`ECC_Visibility`。
- `ACitizenElement`默认关闭Actor Tick(性能优化),玩家附身(`PossessedBy`)一个市民之后如果该市民的原生`Tick()`逻辑里还存在"无待处理路径点就自动再关一次Tick"这类逻辑,要连同`PossessedBy`本身一起检查,否则被操控角色身上依赖`Tick()`的功能会静默失效。
