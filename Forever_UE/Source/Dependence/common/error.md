# error.h / error.cpp

## 职责

项目统一的异常体系：三个异常类、抛异常的`THROW_EXCEPTION`宏、记录"出错时正在做什么"的`ErrorScope`、把一次异常整理成
文字的`ErrorReport`/`DescribeCurrentException`，以及两个接异常的入口`GuardCall`/`GuardData`。
使用规则的总览在`CONVENTIONS.md`"异常"一节，UE那一层的接法在`Source/Forever/Framework/CoreGuard.md`。

## 关键设计

### 三类异常：按"接住之后该怎么办"分

| 类 | `ErrorKind` | 什么时候抛 | 接住之后 |
|---|---|---|---|
| `DataException` | `Data` | 外部数据写错了：config、layout、脚本、存档、路网文件、没启用的mod id、解析不了的时间 | 这一次加载/这一条脚本指令失败，把文件和位置报给写数据的人 |
| `ModException` | `Mod` | mod交给Core的东西自相矛盾：朝向越界、引用了不存在的layout模板、取名重试不出不重复的名字 | 同上，并指名是哪个mod |
| `InternalException` | `Internal` | 程序自己的不变量被破坏（Core/Dependence的bug）：内部下标越界、清单对不上 | 这一局结束，世界不再继续用 |

- 以前是10个按"错误长什么样"分的类（空指针、越界、参数错……），其中`RuntimeException`一个类占了一半，没有任何地方按类型做
  不同处理。现在的三类对应三种不同的处理方式，接的地方只看`GetKind()`。
- **不是`ExceptionBase`的异常**（`std::exception`、`throw 42`这类）：如果抛出时正在某个mod里（见下面`ErrorScope`），算
  `Mod`；否则算`Internal`。`std::bad_alloc`恒为`Internal`。
- 抛的时候不确定归哪类：能通过改数据文件修好的是`Data`；要改mod的C++才能修好的是`Mod`；要改Core/Dependence才能修好的是
  `Internal`。
- `THROW_EXCEPTION(类, 消息)`自动带上`__FILE__`/`__LINE__`/`__FUNCTION__`。

### `ErrorScope`：出错时"当时正在做什么"

- 在进入一段有名字的工作时放一个：`ErrorScope scope(kErrorScopeFile, path);`。正常返回时它什么都不做；**有异常穿过它时**
  （析构时`std::uncaught_exceptions()`比构造时多），它把自己记进当前线程的"展开轨迹"。接住异常的地方用
  `DescribeCurrentException()`把轨迹一并取走。
- 好处是不用在每个调用点写try/catch，也能知道异常来自哪个mod、哪个文件、哪个阶段；而且对任何类型的异常都有效，包括mod抛的
  `std::runtime_error`。
- 标签有四个常量：`kErrorScopeMod`（detail=mod id）、`kErrorScopeFile`（文件路径）、`kErrorScopeStep`（阶段/步骤名）、
  `kErrorScopeScript`（里程碑名、change类型）。
- **scope要放在接异常的地方里面**：异常得穿过它才会被记下来。写成`GuardCall([&] { ErrorScope s(...); 干活(); }, report)`；
  把scope放在`GuardCall`外面的话，异常在到它之前就被接住了，报告里没有这一层。
- **只存指针不拷贝**：`detail`必须活得比scope久。传临时`std::string`的重载是`= delete`的，写错了编译不过。
- **调mod用`MOD_CALL(mod, mod->Layout(...))`**：等价于在这一次调用外面套一个mod scope，返回值原样传出来。Core里调mod虚函数
  的地方都是这么写的。工厂里调mod登记的函数指针（创建实例、`Assign`这类静态函数）用`MOD_ID_CALL(id, ...)`。
- **过期的轨迹**：异常被一个不取报告的`catch`接住时，轨迹会留在线程里。下一个`ErrorScope`在正常路径上构造时发现轨迹非空，
  就当作过期的清掉。所以**接住异常后要先调`DescribeCurrentException()`，再做别的**。确定不需要报告时调`ClearErrorTrace()`。
- **轨迹是每个模块各自一份的线程局部变量**（Dependence是静态库，链接进谁就是谁的）。`ErrorScope`和
  `DescribeCurrentException`要在同一个模块里用才对得上：宿主（Core/UE/CoreTest）里放、宿主里取。mod自己不用放。

### `ErrorReport` / `DescribeCurrentException`

- `DescribeCurrentException()`只能在`catch`块里调。它把当前异常原样再抛一次，按类型分别取信息，返回一个全是本模块自己分配的
  字符串的`ErrorReport`：`kind`、`message`（去掉末尾换行）、`location`（"文件:行 in 函数"，只有`ExceptionBase`有）、
  `context`（从最里层到最外层）、`mod`（最里层那个mod的id）。
- `ToLine()`单行，给界面和存档读取的错误文本；`ToText()`多行，给日志。

### 两个接异常的入口

- `GuardCall(body, report)`：任何异常都接住。用在**会话边界**——接住之后这段代码动过的东西都不能再用。CoreTest、存档各段的
  读取、`Config::ReadConfig`用它；UE里用`CoreGuard`（在它外面多一层"这一局结束"的处理）。
- `GuardData(body, report)`：只接`Data`，别的原样往外抛。用在**一个可以整体跳过的操作**外面：一条脚本指令
  （`AForeverFrameworkActor::ApplyChange`）、一个里程碑的触发条件（`Script::MatchEvent`）、一段对话、一次`start_puzzle`。
  接住后打一条警告、跳过这一个，后面的照常进行。

### 跨DLL

异常对象可能是另一个模块抛的（mod dll，或者Basic.dll里它自己那份Dependence代码）。所有工程都用`/MD`，共用同一个C++运行时，
所以异常本身能正常穿过dll边界；但宿主和mod的内存分配器不是同一个（`CONVENTIONS.md`第4节），下面几条必须守住：

- **一律`catch (const X&)`，不按值接。** 按值接会用抛出方的拷贝构造函数分配字符串、再由接住方的析构函数释放。
- **不保存异常对象**（不用`std::exception_ptr`存起来以后再处理）。它的析构函数和类型信息都在抛出它的dll里，dll卸载后就失效了。
  在接住的地方立刻用`DescribeCurrentException()`转成`ErrorReport`。
- **`ExceptionBase`不要再加"按值返回`std::string`"的虚函数。** 以前的`GetDetailedInfo()`就是这样：宿主调它，字符串在抛出方
  的dll里分配、在宿主里释放。现在只有`what()`和`GetKind()`两个虚函数，都不返回需要对方释放的东西；其余取值函数返回
  `const std::string&`，宿主只读。

### 不要用异常做流程控制

"试着解析一下，不行就算了"不要写`try { stoi(...) } catch (...) {}`：用`TryParseInt`/`TryParseDouble`（`utility.h`，和
`stoi`/`stod`语义完全一样，只是不抛）、`Time::TryParse`。全项目不允许空的`catch {}`：接住了至少打一条警告。唯一的例外是`ErrorScope`的析构函数：异常展开途中记不下轨迹(内存不够)时不能再抛，只能算了。

## 依赖关系

- 依赖：只有标准库。
- 被谁依赖：Dependence/Core里所有抛异常的地方；Core里调mod的地方（`MOD_CALL`）；各`*_factory.cpp`（`MOD_ID_CALL`）；
  `Source/Forever/Framework/CoreGuard.h`；`Source/Test/Core/suite_fault.cpp`逐条验证上面的规则。

## 待办/后续阶段

- 无。
