# 约定文档

本文档与 `REFACTOR_PLAN.md` 同级,供所有阶段session共享。阶段0起就要遵守,后续阶段照做。

## 1. `.md`配对文档模板

每个逻辑独立的`.h`/`.cpp`(一个类或紧密相关的一组类)配一份同名`.md`,记录设计思路,取代代码里的大段中文块注释。**Dependence/Core/Basic层强制要求**;Forever UE模块层的类也建议配,尤其是逻辑不止"数据搬运"的类。

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
- 明确写"阶段X会来补充XX逻辑",避免误以为是遗漏
```

## 2. 代码风格

依据旧代码库(`E:\Projects\Forever_UE`)实际风格总结,不凭空造新规矩,减少后续迁移时的改写成本。

### Dependence / Core / Basic 层(与UE无关的纯C++)

- 类型名、方法名:PascalCase(如`BuildingMod`、`RegisterBuilding`)
- 成员变量:camelCase(如`maxAcreage`、`wallTexture`)
- 文件名、domain文件夹名:snake_case(如`building_mod.h`、`map/`)
- 只用`std::`容器和纯C++类型,**不出现任何UE头文件/类型**(`FString`/`TArray`/`UObject`等一律禁止),因为Mod侧是不依赖UE运行时的纯Win32 DLL,依赖二进制兼容
- 函数级中文说明移入配对`.md`,代码里不再写块注释;非显而易见的行内WHY注释仍可保留
- 前向引用集中在`Core/common/class.h`(仿照旧工程`E:\Projects\Forever_UE\Source\Core\common\class.h`):按domain分组(`// Common`/`// Map`/`// Populace`/`// Society`/`// Story`/`// Industry`/`// Traffic`/`// Player`)声明所有跨文件用到的`class Foo;`。Core下其余`.h`/`.cpp`不再自己写任何前向引用,一律`#include "class.h"`(项目IncludePath已把`Core/common`加进去,不用带`common/`前缀);新增一个需要前向引用的类型时,直接加进`class.h`对应分组,不要在具体文件里就地声明
- **每个concept的`.h`必须配一份同名`.cpp`,哪怕逻辑全写在`.h`里(纯虚接口+inline构造函数之类)也要建**——`.cpp`内容可以只有一行`#include "同名.h"`,这不是为了塞逻辑进去,而是让每个头文件都有一个专属的编译单元:①验证头文件本身`#include`齐全、不依赖"被别的头文件带出来的东西"才能编译(header self-contained检查);②和`.md`配对文档、vcxproj里`ClCompile`/`ClInclude`成对出现的既有习惯保持一致,不要出现"只有声明、没有编译单元"的文件。新增一个`<Concept>Mod`/`<Concept>Factory`/`<Concept>Basic`时,`.h`/`.cpp`要一起建,`.cpp`记得同步加进对应`.vcxproj`的`ClCompile`和`.vcxproj.filters`的对应Filter
- **文件头`#include`顺序+空行规则**(完整示例见`Core/map/map.h`/`.cpp`和`Basic/map/terrain_basic.*`/`Dependence/map/terrain_mod.h`/`terrain_factory.*`,这几份是手动定稿的标准范例):
  - `.h`:`#pragma once`,空一行,`#include "class.h"`(仅Core层、且这个文件确实用到前向引用类型时才有这一条),空一行,Dependence的`#include`(本文件直接用到的Dependence头,只在文件内部之间不空行),空一行,Core的`#include`(同上,只有Core层文件才可能有这一组),空一行,`<>`系统头(`<string>`等,组内不空行)。以上每一组如果本来就是空的(比如这个文件不需要class.h,或者不直接include任何Dependence文件)直接跳过,不留空行占位。
  - `.cpp`:先`#include "同名.h"`(自己的头),空一行,再按`.h`同一套顺序(class.h/Dependence/Core/系统头)继续写。
  - 所有`#include`写完之后,如果这个文件有`#define`,紧跟一个空行再写`#define`块(块内的多组`#define`之间可以按语义拆成小节、组间各空一行,这是普通代码分段,不是这条规则管的范围)。
  - `#define`写完(或者没有`#define`、`#include`就是最后一步)之后,空两行,再写这个文件真正的第一行内容——`.cpp`如果要写`using namespace std;`,它算在这"两行空行之后"的内容里,不算在`#include`/`#define`区块内;`.h`没有`using namespace`,两行空行之后直接是第一个类型/声明。
  - 反例(已修正,见`Basic/map/terrain_basic.cpp`历史版本):不要把`using namespace std;`夹在`#include`和`#define`中间——`using namespace`只能出现在整个`#include`+`#define`区块**之后**的两个空行下面。

### Forever UE模块层

- 遵循UE惯例:`A`/`U`/`F`/`I`前缀,`UCLASS`/`USTRUCT`/`UPROPERTY`/`UFUNCTION`等
- 成员变量沿用旧代码习惯用camelCase(而非Epic官方PascalCase),保持与将被迁移进来的Base类风格一致
- 函数/方法用PascalCase(UE标准)

## 3. Source模块划分

- `Source/Dependence`、`Source/Core`:两个独立的VS静态库工程(`.vcxproj`),与UE完全无关的纯C++,产出`.lib`到工程根目录`x64/<Config>/`。用独立的`Source/Framework.sln`维护,和UE自动生成的`Forever.sln`完全解耦。
- `Source/Basic`:**动态库工程**(`ConfigurationType=DynamicLibrary`),产出`Basic.dll`到同一个`x64/<Config>/`目录,但**不会**被`Forever.Build.cs`静态链接——它是内置的默认Mod集合,和`Forever_Mod/Test`、`Forever_Mod/Wxdj`地位完全相同,由`Config`/`ModLoader`在运行时扫描`Basic.dll`导出的`GetMod<Concept>`/`RegisterMod<Concept>`/`FinishMod<Concept>`符号加载,和旧工程`E:\Projects\Forever_UE\Source\Basic`的`DynamicLibrary`产出方式一致。因此`Source/Basic`下的代码要遵守和Mod DLL相同的约束(只`#include` Dependence头文件、只链接`Dependence.lib`、不出现任何UE类型),不能反过来依赖`Source/Core`。
- `Source/Forever`:UE Runtime模块,通过`Forever.Build.cs`的`PublicAdditionalLibraries`手动链接`Dependence.lib`/`Core.lib`,不链接也不需要链接`Basic`(它在运行时以dll形式被发现)。
- Mod(独立仓库/目录,以及`Source/Basic`本身)编译成不依赖UE运行时的纯Win32 DLL,源码级include Dependence头文件+链接`Dependence.lib`来继承`BuildingMod`等抽象基类,运行时被`LoadLibrary`/`GetProcAddress`加载。这是Dependence/Core必须保持engine-agnostic、Basic和Mod必须只依赖Dependence的根本原因。

## 4. Player类的资产引用方式

Player相关的C++类(`ForeverCharacter`/`ForeverPlayerController`/`ForeverGameMode`/`ForeverPlayerState`)不建Blueprint外壳,直接是可用类,用`ConstructorHelpers::FObjectFinder`/`FClassFinder`在构造函数里引用默认资产(骨骼网格、动画蓝图、Input Mapping Context/Action等)。这只是设置默认值,不影响后续换资产或走向运行时数据驱动换装——子类或运行时代码随时可以覆盖这些`EditDefaultsOnly`属性。
