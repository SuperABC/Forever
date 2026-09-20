# dialog.h / dialog.cpp

阶段4新设计，取代阶段4-0未落地的老工程`dialog.h`（新工程首次迁移这个文件）。三个类
（`Option`/`Section`/`Dialog`）的结构照抄老工程，`Condition`→`std::string`（DSL源码文本，
`EvaluateExpressionBool`现场求值），`vector<function<...>> getValues`→`const ScriptContext&`。

## 指针语义（延迟求值 + 引用约定）

和`Change`/`Event`一样，**Dialog本体只在`Milestone`里持有一份**（`Milestone::dialogs`拥有所有权，
析构时delete），别处（`Option::dialogs`、`Script::MatchEvent`返回的`ScriptAction`）一律用裸指针
引用，绝不delete。`Option::changes`同理引用`Change*`本体（也挂在`Milestone`上）。

## 延迟求值

`Section`的四个台词字段（`speaker`/`content`/`label`/`voice`）存成未解析的DSL源码字符串
（`speakerExpr`等，`std::string`类型——命名带`Expr`后缀是历史遗留，不代表类型是`Expression`，
见`change.md`"字段类型是`std::string`"一节的崩溃教训），只有调用`EvaluateText(context)`时才
用`EvaluateExpression`现场`Parse`+求值、写入内部缓存
`speaking`（`GetSpeaking()`读这个缓存）。关键是`Dialog::GetDialogs() const`**每次返回的是
`Section`的拷贝**（不是引用）——调用方在这份拷贝上调`EvaluateText`，不会污染`Dialog`本体持有的
未求值版本。这样同一个`Dialog`被反复触发/播放时（比如可重复触发的milestone，或Option里的
`dialogs`被多次引用），每次都会用**当时最新的变量值**重新求值，而不是复用第一次求值的结果——
这是"延迟求值"这个要求的核心：不是"晚一点求值"，而是"每次用的时候才求值，且用当次最新状态"。

## 分支选项

`Section::IsBranch()`区分普通台词段/分支选项段。当前阶段没有玩家交互，`Option`的选中逻辑不在
`Dialog`/`Section`/`Option`任何一个类自己身上（保持职责单一），由调用方
（`Forever/Framework/ForeverStoryFrameworkComponent`）决定："默认选中`GetOptions()[0]`"，见
`ForeverStoryFrameworkComponent.md`。

## 嵌套Option的dialogs/changes归属（已知的未完成点）

`Core/story/script.cpp`的`Script::BuildDialogs`解析分支选项时，`Option`引用的嵌套`dialogs`/
`changes`是递归调用`BuildDialogs`/`BuildChanges`当场`new`出来的——这些嵌套本体**不在**外层
`Milestone`的顶层`dialogs`/`changes`列表里，所以也不会被`Milestone`析构时`delete`（和老工程
`script.cpp`的`BuildDialogs`实现完全一样，包括这个未回收的问题）。当前阶段`test.json`不含分支
选项，这条路径没有被实际执行到；等分支选项被真正点名实现时，需要重新设计"嵌套本体到底归谁持有"
（可能是给`Milestone`新增一个专门的owned池来承接所有嵌套层级的Dialog/Change本体），不要在没有
想清楚之前简单认为现在的写法是最终方案。

## 依赖关系

- 依赖：`common/utility.h`、`common/error.h`、`expression.h`
  （`EvaluateExpression`/`EvaluateExpressionBool`）、`change.h`（`Option::changes`）。
- 被谁依赖：`Core/story/milestone.h`（`Milestone::dialogs`）、`Core/story/script.h`
  （`Script::BuildDialogs`/`MatchEvent`）、`Dependence/story/script_mod.h`（`ScriptAction`
  variant的`const Dialog*`分支）、`Forever/Framework/ForeverStoryFrameworkComponent.cpp`
  （展示对话）。
