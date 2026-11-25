# mod_concepts.h / mod_concepts.cpp

## 职责

宿主和mod dll之间的**导出符号约定**：23个concept各自的三个导出函数名(`GetMod<Concepts>`/`RegisterMod<Concepts>`/
`FinishMod<Concepts>`)、每个dll一份的依赖声明符号`GetModDependencies`，以及concept名到`config.json`里key的换算
(`ModConceptJsonKey`：`"Terrains"`->`"terrain_mods"`)。另外提供`ProbeModDll(path)`：不加载、不执行，只读文件判断
一个dll是不是mod dll、是不是调试版；以及宿主把自己的东西交给mod dll的**宿主绑定**(`ModHostBinding`、`BindModHost`、
入口宏`FOREVER_MOD_BIND_HOST`)。

## 关键设计

- **放在Dependence而不是Core**：这张表是"mod dll必须导出什么"的约定本身，所有加载mod的宿主都要用——游戏里是Core的
  `ModLoader`/`Config`，mod作者手里是只链接`Dependence.lib`的`ModTest`(`Source/Test/Mod`)。放在Core的话，没有Core的
  ModTest就得自己再抄一份。
- **`GetModConceptDescriptors()`是这份约定唯一的权威列表**，顺序即配置界面Mod screen按钮的显示顺序。新增concept时
  在这里加一行，再改`Core/common/registry.h`的`ForEachConcept`(Core里唯一的一份concept清单，见`registry.md`)和
  `Test/Mod/mod_checks.cpp`。两边启动时都会核对自己的清单和这张表对不对得上。`FindModConceptDescriptor(conceptKey)`按名字查一行。
- **探测(`ProbeModDll`，2026-11-25)**：扫描目录找mod时，"是不是mod"这个问题不需要执行dll就能回答。它用`ifstream`读文件、
  手工解析PE：导出表里有任何一个`GetMod<Concepts>`就是mod(`isMod`)；导入表里有调试版CRT就是调试版(`isDebug`)。
  全程只是读字节，无关dll的`DllMain`/全局构造一行都不会跑。调用方(`Config::AddDllPath`、ModTest)只对`isMod && !isDebug`
  的dll才`LoadLibrary`。
  - 只认64位(x64的PE32+)，其它一律`readable=false`。
  - 导出名字表是按名字升序排的(链接器保证)，我们的符号都以`GetMod`开头，所以二分定位再看相邻的几个，
    导出几万个符号的大dll也只读十几次文件。
  - 不用`<windows.h>`(Dependence不引入平台头)，字段偏移直接按PE格式写。
  - 看不出来的：静态链接调试CRT(`/MTd`)的dll、延迟加载(delay-load)的导入。
- **宿主绑定(2026-11-25)**：每个mod dll静态链接各自一份Dependence，随机流的host(`RandomHost`)和`debugf`的输出函数在dll里
  都是它自己的那份。宿主加载dll后调它导出的`BindModHost(const ModHostBinding*)`，把`{随机host, 输出函数}`交过去——
  `GetRandom`才受种子控制，`debugf`才进宿主的日志。
  - mod这边：入口cpp写一行`FOREVER_MOD_BIND_HOST`。原来的宏叫`FOREVER_MOD_BIND_RANDOM`、导出`BindRandomHost`，只绑随机，
    已经删掉。
  - 宿主这边：`CurrentModHostBinding()`取调用方模块当前的host和输出函数，`GetProcAddress(kBindModHostSymbol)`后传进去
    (`Core/common/loader.cpp`、ModTest)。没导出的dll退回不可控随机+写标准错误，不崩；ModTest和CoreTest的random套件会查。
  - 以后宿主还有别的东西要交给mod，往`ModHostBinding`末尾加字段，不用再加导出函数。
  - 绑定只在加载时做一次：宿主之后再换输出函数，已经加载的dll拿到的还是旧的。
- 表里只有符号名字符串，不涉及任何`<Concept>Factory`类型——调用方自己把`GetProcAddress`拿到的指针转成
  `void(*)(<Concept>Factory*)`。

## 依赖关系

- 依赖：`common/random.h`(`RandomHost`)、`common/utility.h`(`DebugSink`)，其余只用标准库。
- 被谁依赖：`Core/common/loader.h`(转include，`ModLoader`和`Config`沿用原来的名字)、`Core/common/config.cpp`
  (`ConceptKeyToJsonKey`转调`ModConceptJsonKey`，`AddDllPath`用`ProbeModDll`预筛)、`Test/Mod`(ModTest自己的dll加载，同样先预筛)。

## 待办/后续阶段

- 无。
