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
