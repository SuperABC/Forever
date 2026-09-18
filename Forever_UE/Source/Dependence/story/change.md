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

`Core/story/script.h`的`Script::BuildChanges`识别`"set_value"`/`"place_holder"`两种`type`
字符串，其余40种JSON分发分支还没写，遇到未识别的`type`会`THROW_EXCEPTION`。`Story::
ApplyChange`同理，`dynamic_cast`链只有`SetValueChange`分支，其余分支只打一条"未实现"的
`debugf`日志，不崩溃也不抛异常（变化没生效但游戏能继续跑，容错风格更宽松，因为`ApplyChange`
是运行时每次匹配后都会调用的路径，不像JSON解析那样"剧情作者写错了应该尽早报错"）——
`ChangeControlChange`没有JSON分发分支（不能直接从`test.json`写出来，只能靠`ScriptMod::
WrapScript`把`PlaceHolderChange`占位符替换成它，见下"切换控制"一节），`Story::ApplyChange`
也认不出它，同样落进"未实现"分支打日志；它真正的执行逻辑在Forever层拦截处理，见下。

## `PlaceHolderChange`（已接入JSON分发，本身不代表任何游戏效果）

`"place_holder"`这个`type`对应的就是`PlaceHolderChange`——一个纯粹的"占位符"结构性节点，
只有一个`label`字段（`Expression`，可能引用变量）。它不是给玩家看的内容，是留给`ScriptMod::
WrapScript`在`actionStack`里按标签查找、原地替换成mod自己持有的真实`Change`/`Dialog`用的
"锚点"（`ScriptMod::FindLabel(label, context)`按标签字符串查找，见`Dependence/story/
script_mod.md`）。如果`test.json`里写了一个`PlaceHolderChange`但没有任何`WrapScript`重载去
替换它，它会原样出现在`MatchEvent`的返回值里，落进`Story::ApplyChange`的"未实现"分支——
不会崩溃，但也不会产生预期效果，等于剧情作者忘了配对应的mod逻辑。

## 切换控制（`ChangeControlChange`，阶段4新增，老工程没有对应类型）

把玩家的操控权切换到指定姓名的市民身上——`change_control`只有一个字段`name`
（`Expression`，市民姓名）。这次新增的一个特殊点：**执行逻辑故意不放进`Story::ApplyChange`**
——`Core/story`是纯C++层，不知道`AActor`/`APlayerController`的存在，没法执行"把操控权切给
某个Actor"这件事。真正的执行由Forever层的`UForeverStoryFrameworkComponent::
ApplyControlChange`拦截处理：先`dynamic_cast<const ChangeControlChange*>`判断出这个类型，
命中就跳过`story->ApplyChange`、直接调用自己的`ApplyControlChange`（求值出姓名→
`UForeverPopulaceFrameworkComponent::FindOrSpawnCitizenByName`按姓名找到/强制生成对应的
`ACitizenElement`→`APlayerController::Possess`），详见`Forever/Framework/
ForeverStoryFrameworkComponent.md`"ApplyControlChange"一节。

也没有对应的JSON分发分支（不能直接在`test.json`里写`{"type":"change_control",...}`）——
这次的用法是`test.json`先放一个`{"type":"place_holder","label":"control"}`占位，运行时由
`EmptyScript::WrapScript`查询Core要一个随机citizen姓名，构造出`ChangeControlChange`后替换掉
这个占位符，见`Dependence/story/script_mod.md`"典型用法"一节、
`Forever_Mod/Empty/Cpp/Empty/empty_mods.h`。

## 依赖关系

- 依赖：`common/utility.h`（`ValueType`）、`common/error.h`、`expression.h`（`Expression`）。
- 被谁依赖：`Core/story/milestone.h`（`Milestone::changes`）、`Core/story/script.h`
  （`Script::BuildChanges`/`MatchEvent`，识别`"set_value"`/`"place_holder"`两种`type`）、
  `Core/story/story.cpp`（`Story::ApplyChange`）、`dialog.h`（`Option::changes`）、
  `Dependence/story/script_mod.h`（`ScriptMod::FindLabel`按`dynamic_cast<const
  PlaceHolderChange*>`查找）、`Forever/Framework/ForeverStoryFrameworkComponent.cpp`
  （`dynamic_cast<const ChangeControlChange*>`拦截执行）、
  `Forever_Mod/Empty/Cpp/Empty/empty_mods.h`（`EmptyScript`长期持有一个
  `ChangeControlChange`成员）。

## 待办/后续阶段

- 其余40个变化类型的JSON分发分支、`Story::ApplyChange`的执行分支，都等该类型被点名实现时再补，
  两者需要一起补齐（`ChangeControlChange`不计入这个待办——它的执行逻辑按设计就不在
  `Story::ApplyChange`里，也不需要JSON分发分支）。
