# change.h / change.cpp

阶段4新设计的变化定义层，取代阶段4-0原样移植的`Dependence/story/change.h/.cpp`（已删除，旧版本
依赖已废弃的`Condition`）。按用户确认的颗粒度：只有`SetValueChange`真正接入JSON分发+执行逻辑，
其余41种变化类型只搬运字段定义。

## 职责

`Change`本来就是老工程的纯数据类设计——没有`Apply`之类的虚方法，执行逻辑在调用方按
`GetType()`/`dynamic_cast`分派（老工程在`Forever/Base/StoryBase.cpp`，这次在
`Core/story/story.cpp`的`Story::ApplyChange`），这次原样保留这个设计。`Change`基类只有
`GetType()`纯虚 + `condition`（控制条件，`Expression`类型，`GetCondition`/`SetCondition`）。
42个具体子类（含`ForRangeChange`/`PlaceHolderChange`两个"已实现"的结构性节点）字段列表照抄老
工程`change.h`，字段类型统一改成`Expression`（含`SpawnNpcChange::jobs`这种`vector<string>`字段
也统一成`vector<Expression>`），理由和设计原则同`event.md`。

`ForRangeChange::changes`（循环体变化列表）保持老工程的引用语义——`vector<const Change*>`，不
持有所有权，本体挂在某个`Milestone`上（见`dialog.md`/`milestone.md`"指针语义"说明）。

## `SetValueChange`（已实现）

唯一有真实执行逻辑的变化类型：`variable`（纯字符串，不是Expression——这是要被赋值的变量名本身，
不是内容，和`ForRangeChange::var`循环变量名同理）+ `value`（`Expression`，求值后赋给
`context.self`这个`Container`）。执行逻辑在`Story::ApplyChange`里，`dynamic_cast`命中
`SetValueChange`后调用`context.self->SetValue(variable, value.EvaluateValue(context))`。

## JSON分发

`Core/story/script.h`的`Script::BuildChanges`只识别`"set_value"`这一种`type`字符串，其余41种
JSON分发分支还没写，遇到未识别的`type`会`THROW_EXCEPTION`。`Story::ApplyChange`同理，
`dynamic_cast`链只有`SetValueChange`分支，其余分支只打一条"未实现"的`debugf`日志，不崩溃也不
抛异常（变化没生效但游戏能继续跑，容错风格更宽松，因为`ApplyChange`是运行时每次匹配后都会调用
的路径，不像JSON解析那样"剧情作者写错了应该尽早报错"）。

## 依赖关系

- 依赖：`common/utility.h`（`ValueType`）、`common/error.h`、`expression.h`（`Expression`）。
- 被谁依赖：`Core/story/milestone.h`（`Milestone::changes`）、`Core/story/script.h`
  （`Script::BuildChanges`/`MatchEvent`）、`Core/story/story.cpp`（`Story::ApplyChange`）、
  `dialog.h`（`Option::changes`）。

## 待办/后续阶段

- 其余41个变化类型的JSON分发分支、`Story::ApplyChange`的执行分支，都等该类型被点名实现时再补，
  两者需要一起补齐。
