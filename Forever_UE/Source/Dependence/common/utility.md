# utility.h / utility.cpp

## 职责

阶段4-0共享基础设施之一：提供跨全部domain复用的原语——`ValueType`（脚本引擎的值类型，
`std::variant<int, double, bool, std::string>`）及其与字符串的相互转换（`ToInt`/`ToDouble`/
`ToBool`/`ToString`/`FromString`）、`Container`键值查询接口、`Time`日期时间类、`Counter`倒计时
计数器、`debugf`调试输出、`GetRandom`/`GetRandomNormal`随机数。`story/condition.h`（脚本
表达式引擎）、`map/geometry.h`（几何原语）都直接依赖这里的`ValueType`/`OBJECT_HOLDER`宏。

## 关键设计

- **移植自旧工程，但去掉了头文件对`<windows.h>`/`<strsafe.h>`的直接依赖**——原文件
  `debugf(LPCSTR format, ...)`用了Windows类型`LPCSTR`，且`.h`直接`#include <windows.h>`。
  这个头文件会被`condition.h`/`geometry.h`间接include，进而被`story_mod.h`等Dependence层
  Mod接口headers传递依赖，最终被`Forever_Mod`下的Mod DLL和UE侧`Forever`模块都间接
  include——参照`Core/common/loader.md`已经定下的先例（"头文件不`#include <windows.h>`，
  句柄只在.cpp里出现，避免和UE的`CoreMinimal.h`宏冲突"），把`debugf`的签名改成
  `debugf(const char* format, ...)`（`LPCSTR`在ANSI字符集下本来就是`const char*`的别名，是
  纯签名调整，不改变行为），`<windows.h>`/`<strsafe.h>`移到`utility.cpp`里（`.cpp`里只用到
  `OutputDebugStringA`，`strsafe.h`原文件其实没有用到任何函数，属于旧代码遗留的多余
  include，一并去掉）。除此之外`Time`/`Counter`/`ValueType`转换函数等业务逻辑原样移植，未做
  修改。
- **`ValueType`是脚本引擎的核心值类型**——`story/condition.h`的`Expression::Evaluate`、
  `Container::GetValue`/`SetValue`都以它为载体，`FromString`会自动推断字面量类型（布尔/整数/
  浮点/字符串），`ToString`则会把浮点数的多余尾零裁掉，方便脚本里做字符串比较/拼接。
- **`OBJECT_HOLDER`/`COSTOM_INIT`/`COSTOM_RUNTIME`是纯标记宏**（定义为空），旧工程用它们在
  声明处标注"这个指针成员持有对象所有权"之类的语义，不影响编译，纯粹给读代码的人看，新工程
  保留这个约定，阶段4读到用了这些宏的旧代码时不要误以为要额外处理。
- **`GetRandomNormal(mean, stddev)`（新增，四类人际关系生成用）**：正态分布随机浮点数，
  和`GetRandom(int)`同样的风格——每次调用现场构造`mt19937`+`normal_distribution`，不做
  静态/线程局部引擎优化。给`Populace`生成市民`Personality`（-1到1正态分布）+四类人际关系
  的`Relation`数值用，见`Source/Core/populace/populace.md`"四类人际关系生成"一节。
- **`Time`不依赖`<chrono>`做内部存储**，全部用年/月/日/时/分/秒/毫秒整数字段+手写的进位/借位
  归一化逻辑（`NormalizeTime`），构造函数支持解析ISO 8601、中文（`YYYY年MM月DD日`）、美式
  （`MM/DD/YYYY`）及纯时间等多种字符串格式，供story脚本里写时间字面量时随意选一种熟悉的格式。
- **`debugf`用`OutputDebugStringA`实现，只有附加调试器时才能看到，不会写进
  `Saved/Logs/Forever.log`**——这个事实在排查公共交通车辆相关bug时反复踩到：Dependence/
  Core/Basic层的`debugf`输出对着PIE日志文件读是看不到的，只有Forever UE模块层的`UE_LOG`
  才会写进日志文件。诊断需要玩家可见、或者免调试器确认的信息时要在Forever层用`UE_LOG`，
  不能指望`debugf`，见`Source/Core/traffic/route.md`"`Time::DifferenceInSeconds`"一节的
  排查过程。
- **`Time::DifferenceInSeconds`2026-10-04修过一次符号反了的bug**——`this`比`other`早时
  应该返回正数，原来的实现返回的是负数，导致唯一调用方`Traffic::Tick`里的
  `if (delta > 0.0) elapsedSeconds += delta;`每一帧都被跳过，`elapsedSeconds`永远停在
  初始值不动，表现为"公交车/火车/飞机生成之后位置再也不变"。完整排查记录见
  `utility.cpp`里这个函数声明处的注释。

## 依赖关系

- 依赖：`error.h`（`THROW_EXCEPTION`，`Time`字段校验失败时抛`OutOfRangeException`/
  `InvalidArgumentException`）。`.cpp`额外依赖`<windows.h>`（`OutputDebugStringA`）。
- 被谁依赖：`story/condition.h`（`ValueType`）、`map/geometry.h`（`OBJECT_HOLDER`宏、
  `GetRandom`）、后续阶段4迁移的Core层各domain代码（`Time`常被城市模拟里的日程/日历系统
  使用）。

## 待办/后续阶段

- `Counter`目前仍然没有被任何domain实际使用——`Time`已经被`Player`(游戏时钟)、
  `Traffic`(`Route::Update`的时刻表)等系统广泛使用，`ValueType`被`story/condition.h`
  的脚本表达式引擎使用，只有`Counter`还停留在"可编译但没有调用方"的状态。
