# milestone.h / milestone.cpp

阶段4新落地（新工程首次迁移这个文件），结构照抄老工程`Core/story/milestone.h/.cpp`，
`Condition`→`std::string`（DSL源码文本，`EvaluateExpressionBool`现场求值——`drop`字段最初
写成`Expression`，后来因为跨模块崩溃改回`std::string`，完整原因见`Dependence/story/
change.md`"字段类型是`std::string`"一节），`vector<function<...>> getValues`→
`const ScriptContext&`。

## 职责

`Milestone`是`Script`的最小调度单元：一组触发事件（`triggers`）+ 一次性失效条件（`drop`）+
对话/变化列表（`dialogs`/`changes`）+ 后续里程碑名称列表（`subsequences`，用于顺序解锁）。
`triggers`/`dialogs`/`changes`三个列表都是`Milestone`自己持有本体（`OBJECT_HOLDER`语义，
析构时`delete`），这是`Dialog`/`Change`"本体只在Milestone里持有一份"这条约定的落地点，见
`Dependence/story/dialog.md`/`change.md`。

## `MatchTrigger`：条件门 + 类型/字段匹配二合一

```cpp
for (auto trigger : triggers) {
    if (!EvaluateExpressionBool(trigger->GetCondition(), context)) continue; // 触发事件自己的控制条件
    if (trigger->GetType() != e->GetType()) continue;
    if (trigger->Match(e, context)) return true;
}
```

老工程`Script::MatchEvent`里有一个单独的外层循环先对每个trigger做"控制条件"检查
（`trigger->GetCondition().EvaluateBool(getValues)`），但检查完之后调的是
`Milestone::MatchTrigger(event, getValues)`——这个函数内部又会重新遍历全部triggers做类型+
`Match`检查，等于同一批triggers被遍历了两次，且外层"控制条件通过的trigger"和内层"实际参与
`Match`的trigger"没有绑定关系（看起来像是老代码的遗留写法）。这次把"控制条件门"直接**折进**
`MatchTrigger`自己的循环里（每个trigger先检查自己的条件，再检查类型/Match），逻辑更清晰，
`Core/story/script.cpp`的`Script::MatchEvent`不需要再单独维护一层触发条件检查。

## changes在dialogs之前

`Milestone`的构造函数参数、字段声明顺序、`GetChanges`/`GetDialogs`两个getter的声明顺序，这次
统一把`changes`排在`dialogs`前面（json里`test.json`的字段顺序也一样）——`Change`是立即执行的
（`SetValueChange`直接改`Container`的变量值），`Dialog`是延迟求值的（`Dialog::GetDialogs()`
每次返回未求值的`Section`拷贝，真正求值的时机在展示层，见`Dependence/story/dialog.md`）。这个
顺序不是随便定的：`Script::MatchEvent`（`Core/story/script.cpp`）往`actions`里塞的时候，同样是
先塞`GetChanges()`再塞`GetDialogs()`，这样如果一段对话文本里引用了同一个milestone里某个change
刚设置的变量（如`$$self.test`），显示层按顺序处理`actions`时，执行到这条change、变量已经被更新，
后面再对dialog求值就能读到新值——如果顺序反过来，dialog会用到change生效之前的旧值。

## 顺序解锁：`MilestoneNode`

`premise`（前置未满足数量）+ `subsequents`（后置节点列表）实现里程碑的顺序解锁——只有
`premise`降为0的节点才会被放进`Script::actives`参与匹配。这部分在`Core/story/script.h`的
`ReadMilestones`里按`GetSubsequences()`统一建图，`MilestoneNode`本身只是纯数据结构。

## 依赖关系

- 依赖：`Dependence/story/expression.h`（`ScriptContext`、`EvaluateExpressionBool`）、
  `event.h`/`dialog.h`/`change.h`（仅milestone.cpp需要完整定义，milestone.h只前置声明）。
- 被谁依赖：`Core/story/script.h`（`Script::milestones`/`actives`）。
