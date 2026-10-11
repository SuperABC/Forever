# milestone.h / milestone.cpp

## 职责

`Milestone`是`Script`的最小调度单元：一组触发事件（`triggers`）+ 一次性失效条件（`drop`，
`std::string`类型的DSL源码文本，`EvaluateExpressionBool`现场求值）+ 对话/变化列表
（`dialogs`/`changes`）+ 后续里程碑名称列表（`subsequences`，用于顺序解锁）。
`triggers`/`dialogs`/`changes`三个列表都是`Milestone`自己持有本体（`OBJECT_HOLDER`语义，
析构时`delete`），这是`Dialog`/`Change`"本体只在Milestone里持有一份"这条约定的落地点，见
`Dependence/story/dialog.md`/`change.md`。

## 关键设计

- **`MatchTrigger`：条件门 + 类型/字段匹配一次遍历内完成**：

  ```cpp
  for (auto trigger : triggers) {
      if (!EvaluateExpressionBool(trigger->GetCondition(), context)) continue; // 触发事件自己的控制条件
      if (trigger->GetType() != e->GetType()) continue;
      if (trigger->Match(e, context)) return true;
  }
  ```

  每个trigger在同一次循环里先检查自己的控制条件，再检查类型/`Match`，`Core/story/script.cpp`
  的`Script::MatchEvent`不需要再单独维护一层触发条件检查。

- **`changes`排在`dialogs`之前**（构造函数参数、字段声明顺序、`GetChanges`/`GetDialogs`两个
  getter的声明顺序、json字段顺序都一致）：`Change`是立即执行的（`SetValueChange`直接改
  `Container`的变量值），`Dialog`是延迟求值的（`Dialog::GetDialogs()`每次返回未求值的
  `Section`拷贝，真正求值的时机在展示层，见`Dependence/story/dialog.md`）。`Script::MatchEvent`
  （`Core/story/script.cpp`）往`actions`里塞的时候也是先塞`GetChanges()`再塞`GetDialogs()`：
  如果一段对话文本引用了同一个milestone里某个change刚设置的变量（如`$$self.test`），显示层按
  顺序处理`actions`时，执行到这条change、变量已经被更新，后面再对dialog求值就能读到新值——如果
  顺序反过来，dialog会用到change生效之前的旧值。

- **顺序解锁：`MilestoneNode`**：`premise`（前置未满足数量）+ `subsequents`（后置节点列表）
  实现里程碑的顺序解锁——只有`premise`降为0的节点才会被放进`Script::actives`参与匹配。这部分
  在`Core/story/script.h`的`ReadMilestones`里按`GetSubsequences()`统一建图，`MilestoneNode`
  本身只是纯数据结构。

## 依赖关系

- 依赖：`Dependence/story/expression.h`（`ScriptContext`、`EvaluateExpressionBool`）、
  `event.h`/`dialog.h`/`change.h`（仅milestone.cpp需要完整定义，milestone.h只前置声明）。
- 被谁依赖：`Core/story/script.h`（`Script::milestones`/`actives`）。
