# event.h / event.cpp

## 职责

- `Event`基类：`GetType()`纯虚（每个子类的静态类型标识，snake_case字符串，如`"game_start"`）；
  `Match(Event* e, const ScriptContext& context)`是**有默认实现的虚函数**（默认只比较
  `GetType()`是否相等）。`GameStartEvent`本来语义就是纯类型匹配，直接用默认实现，不需要
  override；其余30个子类暂时都没有字段级匹配逻辑，继承默认实现即可。等某个具体事件类型的
  字段匹配逻辑被点名实现时，再针对该类型override`Match`。
- `Event::GetLocalValue(const std::string& name) const`：`local.`前缀变量的求值入口（见
  `expression.md`"ScriptContext"一节）。基类默认返回`{false, {}}`（查不到任何字段），具体
  子类实现真逻辑时按需override，把自己的字段（如`GlobalMessageEvent::message`）暴露出来。
- 31个具体子类：字段统一是`std::string`（DSL源码文本，不预先解析）——呼应"理论上json里所有
  值都是表达式"的顶层原则（见`expression.md`），即使是像`PuzzleResultEvent::result`这种概念
  上是`int`的字段，也统一存成DSL源码字符串，取值时调用方自己用`EvaluateExpression`/
  `EvaluateExpressionBool`现场`Parse`+求值、按需`ToInt`/`ToBool`/`ToString`转换（**不要**
  预先`Parse`好存成`Expression`对象再存成员——`Expression`树若被一个模块构造、另一个模块
  求值会导致跨模块堆损坏崩溃，完整原因见`change.md`"字段类型是`std::string`"一节）。
- `OptionDialogEvent`只保留`(std::string name, std::string option)`一种构造方式（按名称
  引用对话选项），不提供按序号构造的重载。

## JSON分发

`Core/story/script.h`的`Script::BuildEvent`只识别`"game_start"`这一种`type`字符串，其余29种
（`OptionDialogEvent`等）JSON分发分支还没写，遇到未识别的`type`会`THROW_EXCEPTION`（未识别
类型直接抛异常，不是静默跳过）。等某个事件类型被点名实现时，在`BuildEvent`里补一个
`else if (type == "...")`分支，从JSON节点提取字段，直接调`.AsString()`拿到字符串，构造对应
的`Event`子类。

## 依赖关系

- 依赖：`common/utility.h`（`ValueType`）、`common/error.h`、`expression.h`
  （`ScriptContext`、`EvaluateExpression`/`EvaluateExpressionBool`——字段本身是
  `std::string`，只有真正求值那一刻才用得到`Expression`/`Parse`）。
- 被谁依赖：`Core/story/milestone.h`（`Milestone::triggers`）、`Core/story/script.h`
  （`Script::BuildEvent`/`MatchEvent`）、`Core/story/story.cpp`（`Story::BroadcastGameStart`
  构造`GameStartEvent`）、`Dependence/story/expression.cpp`（`VariableExpression::Evaluate`
  调`Event::GetLocalValue`）。

## 待办/后续阶段

- 其余30个事件类型的`Match`字段级逻辑、`GetLocalValue`字段暴露、`BuildEvent`的JSON分发分支，
  都等该类型被点名实现时再补，三者通常需要一起补齐。

## 已经能在.script里写的触发条件

- `game_start`
- `option_dialog`(name/option)
- `citizen_decease`(name/reason，留空=不限)：`CitizenDeceaseEvent::Match`按name/reason过滤(都是表达式)，和`OptionDialogEvent::Match`同一个写法。市民死亡时由`UForeverStoryFrameworkComponent::BroadcastCitizenDecease`广播给主线脚本和死者自己的Scheduler脚本(按name找，和`option_dialog`同一个模式)。
- 事件字段：`option_dialog`在脚本里可以用`local.name`/`local.option`读到被点的对象名和选项文字，`citizen_decease`可以用`local.name`/`local.reason`读到死者姓名和死因(`GetLocalValue`)。
