# dialog.h / dialog.cpp

## 职责

三个类`Option`/`Section`/`Dialog`，`Condition`字段统一是`std::string`（DSL源码文本，
`EvaluateExpressionBool`现场求值）。

## 指针语义（延迟求值 + 引用约定）

和`Change`/`Event`一样，**Dialog本体只在`Milestone`里持有一份**（`Milestone::dialogs`拥有
所有权，析构时delete），别处（`Option::dialogs`、`Script::MatchEvent`返回的`ScriptAction`）
一律用裸指针引用，绝不delete。分支选项里嵌套的`dialogs`/`changes`本体（`Script::BuildDialogs`
解析时`new`出）不在`Milestone`的顶层列表里，由包含该分支段的`Dialog`在析构时递归`delete`——`Option`是被拷贝的值类型，
不能由它释放。

## 延迟求值

`Section`的四个台词字段（`speaker`/`content`/`label`/`voice`）存成未解析的DSL源码字符串
（`speakerExpr`等，`std::string`类型——命名带`Expr`后缀不代表类型是`Expression`，见
`change.md`"字段类型是`std::string`"一节），只有调用`EvaluateText(context)`时才用
`EvaluateExpression`现场`Parse`+求值、写入内部缓存`speaking`（`GetSpeaking()`读这个缓存）。
关键是`Dialog::GetDialogs() const`**每次返回的是`Section`的拷贝**（不是引用）——调用方在
这份拷贝上调`EvaluateText`，不会污染`Dialog`本体持有的未求值版本。这样同一个`Dialog`被反复
触发/播放时（比如可重复触发的milestone，或Option里的`dialogs`被多次引用），每次都会用当时
最新的变量值重新求值，而不是复用第一次求值的结果——这是"延迟求值"的核心：不是"晚一点求值"，
而是"每次用的时候才求值，且用当次最新状态"。

## 后台对话

普通台词`Section`有一个`background`(秒，默认0)：大于0时是后台对话，由`Script::BuildDialogs`从JSON的`"background"`读入，展示规则见`Source/Forever/Framework/ForeverStoryFrameworkComponent.md`"后台对话"。分支选项段恒为0。

## 分支选项

`Section::IsBranch()`区分普通台词段/分支选项段。选中逻辑不在`Dialog`/`Section`/`Option`身上（保持职责单一），由调用方
`UForeverStoryFrameworkComponent::EnqueueDialog`处理：对每个`Option`先按`condition`过滤（求值为假的选项不出现），文本用
`EvaluateExpression`求值；全部选项被过滤掉则整个分支跳过。`Dialog`自己的`condition`为假时整段不展示。

## 依赖关系

- 依赖：`common/utility.h`、`common/error.h`、`expression.h`
  （`EvaluateExpression`/`EvaluateExpressionBool`）、`change.h`（`Option::changes`）。
- 被谁依赖：`Core/story/milestone.h`（`Milestone::dialogs`）、`Core/story/script.h`
  （`Script::BuildDialogs`/`MatchEvent`）、`Dependence/story/script_mod.h`（`ScriptAction`
  variant的`const Dialog*`分支）、`Forever/Framework/ForeverStoryFrameworkComponent.cpp`
  （展示对话）。

## 待办/后续阶段

- 无。
