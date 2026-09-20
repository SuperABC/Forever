# event.h / event.cpp

阶段4新设计的事件定义层，取代阶段4-0原样移植的`Dependence/story/event.h/.cpp`（已删除，旧版本
依赖已废弃的`Condition`/`$$()`间接寻址语法）。这次按用户确认的颗粒度："完整字段级迁移，但只有
`GameStartEvent`真正接入JSON分发+匹配逻辑，其余30种事件类型只搬运字段定义，不接入JSON分发"。

## 职责

- `Event`基类：`GetType()`纯虚（每个子类的静态类型标识，snake_case字符串，如`"game_start"`）；
  `Match(Event* e, const ScriptContext& context)`这次改成**有默认实现的虚函数**（默认只比较
  `GetType()`是否相等），不再是纯虚——老工程31个子类都要各自override字段级匹配逻辑，这次除了
  `GameStartEvent`（本来语义就是纯类型匹配，直接用默认实现，不需要override）之外，其余30个子类
  暂时都没有字段级匹配逻辑，继承默认实现即可，减少了这次的机械工作量。等某个具体事件类型的字段
  匹配逻辑被点名实现时，再针对该类型override`Match`。
- `Event::GetLocalValue(const std::string& name) const`：新增虚方法，`local.`前缀变量的求值
  入口（见`expression.md`"ScriptContext"一节）。基类默认返回`{false, {}}`（查不到任何字段），
  具体子类实现真逻辑时按需override，把自己的字段（如`GlobalMessageEvent::message`）暴露出来。
  这是通用化"local.你自己判断怎么实现"这条开放问题的落地方式——比老工程`Story::CreateLocal`
  （每次匹配前手动new一个临时Script、把事件字段一个个显式塞进变量表）更轻量，不需要额外分配。
- 31个具体子类：字段列表照抄老工程`event.h`，字段类型统一从`std::string`/`int`/`bool`改成
  `std::string`（DSL源码文本，不预先解析）——呼应"理论上json里所有值都是表达式"的顶层原则
  （见`expression.md`），即使是像`PuzzleResultEvent::result`这种老工程是`int`的字段，这次也
  统一存成DSL源码字符串，取值时调用方自己用`EvaluateExpression`/`EvaluateExpressionBool`
  现场`Parse`+求值、按需`ToInt`/`ToBool`/`ToString`转换（**不要**预先`Parse`好存成`Expression`
  对象再存成员——`Expression`树若被一个模块构造、另一个模块求值会导致跨模块堆损坏崩溃，完整
  原因见`change.md`"字段类型是`std::string`"一节）。
- `OptionDialogEvent`不需要"按序号构造"这一种——老工程用参数类型重载区分`(int id, string
  option)`（按序号）和`(string name, string option)`（按名称）两种构造方式，这次确认按序号
  这种用法不需要，直接砍掉`id`字段，只保留`(std::string name, std::string option)`一种构造。

## JSON分发

`Core/story/script.h`的`Script::BuildEvent`只识别`"game_start"`这一种`type`字符串，其余29种
（`OptionDialogEvent`等）JSON分发分支还没写，遇到未识别的`type`会`THROW_EXCEPTION`（和老工程
"未识别类型抛异常"的容错风格一致，不是静默跳过）。等某个事件类型被点名实现时，在`BuildEvent`里
补一个`else if (type == "...")`分支，从JSON节点提取字段，直接调`.AsString()`拿到字符串，
构造对应的`Event`子类（`Script::BuildExpression`这层一行的包装函数已删除，见`script.md`）。

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
