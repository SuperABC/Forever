# utility.h / utility.cpp

## 职责

提供跨全部domain复用的原语——`ValueType`（脚本引擎的值类型，
`std::variant<int, double, bool, std::string>`）及其与字符串的相互转换（`ToInt`/`ToDouble`/
`ToBool`/`ToString`/`FromString`）、`Container`键值查询接口、`Time`日期时间类、`Counter`倒计时
计数器、`debugf`调试输出、`GetRandom`/`GetRandomNormal`随机数。`story/condition.h`（脚本
表达式引擎）、`map/general_geometry.h`（几何原语）都直接依赖这里的`ValueType`/`OBJECT_HOLDER`宏。

## 关键设计

- **头文件不`#include <windows.h>`/`<strsafe.h>`**——`debugf`签名是
  `debugf(const char* format, ...)`，`<windows.h>`（`OutputDebugStringA`实现用）只在
  `utility.cpp`里出现。这个头文件会被`condition.h`/`general_geometry.h`间接include，进而被
  `story_mod.h`等Dependence层Mod接口headers传递依赖，最终被`Forever_Mod`下的Mod DLL和
  UE侧`Forever`模块都间接include——和`Core/common/loader.md`"头文件不
  `#include <windows.h>`，句柄只在.cpp里出现"的约定一致，避免和UE的`CoreMinimal.h`宏
  冲突。
- **`ValueType`是脚本引擎的核心值类型**——`story/condition.h`的`Expression::Evaluate`、
  `Container::GetValue`/`SetValue`都以它为载体，`FromString`会自动推断字面量类型（布尔/
  整数/浮点/字符串），`ToString`则会把浮点数的多余尾零裁掉，方便脚本里做字符串比较/拼接。
- **`OBJECT_HOLDER`/`COSTOM_INIT`/`COSTOM_RUNTIME`是纯标记宏**（定义为空），用于在声明处
  标注"这个指针成员持有对象所有权"之类的语义，不影响编译，纯粹给读代码的人看。
- **`GetRandomNormal(mean, stddev)`**：正态分布随机浮点数，和`GetRandom(int)`同样的
  风格——每次调用现场构造`mt19937`+`normal_distribution`，不做静态/线程局部引擎优化。给
  `Populace`生成市民`Personality`（-1到1正态分布）+四类人际关系的`Relation`数值用，见
  `Source/Core/populace/populace.md`"四类人际关系生成"一节。
- **`Time`不依赖`<chrono>`做内部存储**，全部用年/月/日/时/分/秒/毫秒整数字段+手写的进位/借位
  归一化逻辑（`NormalizeTime`），构造函数支持解析ISO 8601、中文（`YYYY年MM月DD日`）、美式
  （`MM/DD/YYYY`）及纯时间等多种字符串格式，供story脚本里写时间字面量时随意选一种熟悉的格式。
- **`debugf`用`OutputDebugStringA`实现，只有附加调试器时才能看到，不会写进
  `Saved/Logs/Forever.log`**——Dependence/Core/Basic层的`debugf`输出对着PIE日志文件读是
  看不到的，只有Forever UE模块层的`UE_LOG`才会写进日志文件。诊断需要玩家可见、或者免调
  试器确认的信息时要在Forever层用`UE_LOG`，不能指望`debugf`。
- **`Time::DifferenceInSeconds`的符号方向**：`this`比`other`早时必须返回正数——唯一
  调用方`Traffic::Tick`里`if (delta > 0.0) elapsedSeconds += delta;`依赖这个方向，符号
  反了会导致`elapsedSeconds`永远停在初始值不动，表现为"公交车/火车/飞机生成之后位置再也
  不变"，排查记录见`Source/Core/traffic/route.md`"`Time::DifferenceInSeconds`"一节。

## 依赖关系

- 依赖：`error.h`（`THROW_EXCEPTION`，`Time`字段校验失败时抛`OutOfRangeException`/
  `InvalidArgumentException`）。`.cpp`额外依赖`<windows.h>`（`OutputDebugStringA`）。
- 被谁依赖：`story/condition.h`（`ValueType`）、`map/general_geometry.h`（`OBJECT_HOLDER`宏、
  `GetRandom`）、Core层各domain代码（`Time`常被城市模拟里的日程/日历系统使用）。

## 待办/后续阶段

- `Counter`目前仍然没有被任何domain实际使用——`Time`已经被`Player`(游戏时钟)、
  `Traffic`(`Route::Update`的时刻表)等系统广泛使用，`ValueType`被`story/condition.h`
  的脚本表达式引擎使用，只有`Counter`还停留在"可编译但没有调用方"的状态。

## Time解析扩展
中文格式年份允许1~4位，时间部分既可以是`HH:MM:SS`也可以是`H时M分S秒`(如`0年1月1日0时0分4秒`)。
