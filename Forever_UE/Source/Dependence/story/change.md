# change.h / change.cpp

阶段4新设计的变化定义层，取代阶段4-0原样移植的`Dependence/story/change.h/.cpp`（已删除，旧版本
依赖已废弃的`Condition`）。按用户确认的颗粒度：只有`SetValueChange`真正接入JSON分发+执行逻辑，
其余41种变化类型只搬运字段定义。

## 职责

`Change`本来就是老工程的纯数据类设计——没有`Apply`之类的虚方法，执行逻辑在调用方按
`GetType()`/`dynamic_cast`分派（老工程在`Forever/Base/StoryBase.cpp`，这次在
`Core/story/story.cpp`的`Story::ApplyChange`），这次原样保留这个设计。`Change`基类只有
`GetType()`纯虚 + `condition`（控制条件，`std::string`类型，`GetCondition`/`SetCondition`）。
42个具体子类（含`ForRangeChange`/`PlaceHolderChange`两个"已实现"的结构性节点）字段列表照抄老
工程`change.h`。

`ForRangeChange::changes`（循环体变化列表）保持老工程的引用语义——`vector<const Change*>`，不
持有所有权，本体挂在某个`Milestone`上（见`dialog.md`/`milestone.md`"指针语义"说明）。

## 字段类型是`std::string`（DSL源码），不是`Expression`——一次真实崩溃换来的教训

**这里曾经把所有字段类型统一成`Expression`（预先`Parse`好、存成AST树），后来改成了
`std::string`（原始DSL源码文本，不预先解析），`std::vector<Expression>`同理改成
`std::vector<std::string>`（唯一一处是`SpawnNpcChange::jobs`）。** 起因是一次真实
PIE崩溃：`Source/Basic/society/job_basic.cpp`的`ShopSalerJob::ExecNode`（编译在
`Basic.dll`）曾经用`Expression::Parse`现场构造`Expression`对象塞进`NPCNavigateChange`，
`Forever.dll`侧后来对着这个字段调`.EvaluateValue(context)`求值——`EvaluateValue`内部对
表达式树节点是虚函数调用，走的是节点对象自己的vtable，vtable是`Basic.dll`在`new`它时
钉死的，所以这个虚调用**实际执行的是`Basic.dll`编译的机器码**，返回值（含堆分配的字符串
缓冲区）也是`Basic.dll`自己的标准CRT `operator new`分配的。但UE给每个UE模块（包括
`UnrealEditor-Forever.dll`）单独重载了全局`operator new`/`delete`，改走UE自己的
`FMemory`分配器；`Basic.dll`是普通Win32 DLL，没有这层重载。于是这个字符串在`Basic.dll`
的CRT堆分配、却在`Forever.dll`侧被`FMemory`释放，堆损坏崩溃——"9点市民上班"这个场景
第一次真正踩上，因为地址字符串终于长到超出了`std::string`的SSO阈值，之前用短字面量
（`"home"`/`"workplace"`）从来没真正触发过堆分配，所以一直没暴露。

**修复原则**：只要`Expression`树被一个模块构造、被另一个模块求值，就有这个风险——不是
`NPCNavigateChange`一个类的问题，是"把预先解析好的`Expression`对象当存储字段"这个设计
本身的通病。改成`std::string`存原始DSL源码文本之后，`Expression::Parse`+
`.EvaluateValue()`/`.EvaluateBool()`这两步统一延后到**真正要用的那一刻**才现场执行，
用`Dependence/story/expression.h`新增的`EvaluateExpression(source, context)`/
`EvaluateExpressionBool(source, context)`两个便捷函数完成——这两步永远发生在同一次
函数调用、同一个模块编译执行的代码里，`Expression`树（含虚函数vtable指向谁）从来不会
跨模块存活，问题从根上消失。**`NPCNavigateChange::name`/`destination`是这次唯一的
例外**——它们本来就是纯粹的已算好的值（不需要`$$`动态求值），从Expression体系里彻底
拿出来，改成`ShopSalerJob::ExecNode`直接传参构造，不再走`Parse`/`Evaluate`这一套。

## `SetValueChange`（已实现）

唯一有真实执行逻辑的变化类型：`variable`（这是要被赋值的变量名本身，不是内容，和
`ForRangeChange::var`循环变量名同理）+ `value`（DSL源码字符串，求值后赋给
`context.self`这个`Container`）。执行逻辑在`Story::ApplyChange`里，`dynamic_cast`命中
`SetValueChange`后调用
`context.self->SetValue(variable, EvaluateExpression(value, context))`。

## JSON分发

`Core/story/script.h`的`Script::BuildChanges`识别`"set_value"`/`"place_holder"`/
`"debug_print"`三种`type`字符串，其余39种JSON分发分支还没写，遇到未识别的`type`会
`THROW_EXCEPTION`。`Story::ApplyChange`同理，`dynamic_cast`链只有`SetValueChange`
分支，其余分支只打一条"未实现"的`debugf`日志，不崩溃也不抛异常（变化没生效但游戏能继续
跑，容错风格更宽松，因为`ApplyChange`是运行时每次匹配后都会调用的路径，不像JSON解析那样
"剧情作者写错了应该尽早报错"）——`ChangeControlChange`/`DebugPrintChange`的执行逻辑都
故意不放进`Story::ApplyChange`（前者没有JSON分发分支，不能直接从`test.script`写出来，
只能靠`ScriptMod::WrapScript`把`PlaceHolderChange`占位符替换成它；后者虽然**有**JSON
分发分支，但打印需要调用`GEngine::AddOnScreenDebugMessage`，是UE调用，`Core`不能依赖
UE），`Story::ApplyChange`对这两者都认不出来，同样落进"未实现"分支打日志；它们真正的
执行逻辑都在Forever层拦截处理，`ChangeControlChange`见下"切换控制"一节，
`DebugPrintChange`见下"调试打印"一节。

## `PlaceHolderChange`（已接入JSON分发，本身不代表任何游戏效果）

`"place_holder"`这个`type`对应的就是`PlaceHolderChange`——一个纯粹的"占位符"结构性节点，
只有一个`label`字段（DSL源码字符串，可能引用变量）。它不是给玩家看的内容，是留给`ScriptMod::
WrapScript`在`actionStack`里按标签查找、原地替换成mod自己持有的真实`Change`/`Dialog`用的
"锚点"（`ScriptMod::FindLabel(label, context)`内部用`EvaluateExpression(placeholder->
GetLabel(), context)`求值后按标签字符串查找，见`Dependence/story/script_mod.md`）。
如果`test.json`里写了一个`PlaceHolderChange`但没有任何`WrapScript`重载去
替换它，它会原样出现在`MatchEvent`的返回值里，落进`Story::ApplyChange`的"未实现"分支——
不会崩溃，但也不会产生预期效果，等于剧情作者忘了配对应的mod逻辑。

## 切换控制（`ChangeControlChange`，阶段4新增，老工程没有对应类型）

把玩家的操控权切换到指定姓名的市民身上——`change_control`只有一个字段`name`
（DSL源码字符串，市民姓名）。这次新增的一个特殊点：**执行逻辑故意不放进`Story::ApplyChange`**
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

## 调试打印（`DebugPrintChange`，这次新增）

一个只带`message`（DSL源码字符串）字段的调试输出变化，形状和已有的`GlobalMessageChange`
完全一样（一个字符串字段+`SetMessage`/`GetMessage`），单独新增一个类型而不是复用
`GlobalMessageChange`，是因为两者语义不同——这个类型只用于开发期调试输出，不是游戏内
广播消息。有JSON分发分支（`{"type":"debug_print","message":"..."}`），`message`常见
写法是引用某个`Container`变量，比如`"$$self.name"`（求值出这个Script所在的Job/
Organization自己的唯一名字，见`Core/society/job.md`"Script配置"一节）。和
`ChangeControlChange`同一个理由，执行逻辑不放进`Story::ApplyChange`（`Core`不能调用
`GEngine`），由Forever层（`ForeverFrameworkActor.cpp`的`populace->Tick`/`society->Tick`
回调，`ForeverStoryFrameworkComponent.cpp`的`BroadcastGameStart`回调）在
`story->ApplyChange`/其它分支之前先`dynamic_cast<const DebugPrintChange*>`拦截，用
`EvaluateExpression(debugPrint->GetMessage(), context)`现场parse+求值后调
`GEngine->AddOnScreenDebugMessage`打印到屏幕左上角。

`Resource/Story/job_shop_saler.script`/`organization_shop.script`各自的`game_start`
milestone用它验证：Job/Organization的Script能不能正确广播`game_start`+`$$self.name`
能不能正确取到`JobMod`/`OrganizationMod`自己唯一的`GetName()`。

## 依赖关系

- 依赖：`common/utility.h`（`ValueType`）、`common/error.h`、`expression.h`
  （`EvaluateExpression`/`EvaluateExpressionBool`——字段本身是`std::string`，只有
  真正求值那一刻才用得到`Expression`/`Parse`，见上"字段类型是`std::string`"一节）。
- 被谁依赖：`Core/story/milestone.h`（`Milestone::changes`）、`Core/story/script.h`
  （`Script::BuildChanges`/`MatchEvent`，识别`"set_value"`/`"place_holder"`/
  `"debug_print"`三种`type`）、`Core/story/story.cpp`（`Story::ApplyChange`）、
  `dialog.h`（`Option::changes`）、`Dependence/story/script_mod.h`（`ScriptMod::
  FindLabel`按`dynamic_cast<const PlaceHolderChange*>`查找）、
  `Forever/Framework/ForeverStoryFrameworkComponent.cpp`/`ForeverFrameworkActor.cpp`
  （`dynamic_cast<const ChangeControlChange*>`/`dynamic_cast<const
  DebugPrintChange*>`拦截执行）、`Forever_Mod/Empty/Cpp/Empty/empty_mods.h`
  （`EmptyScript`长期持有一个`ChangeControlChange`成员）。

## 待办/后续阶段

- 其余39个变化类型的JSON分发分支、`Story::ApplyChange`的执行分支，都等该类型被点名实现时再补，
  两者需要一起补齐（`ChangeControlChange`/`DebugPrintChange`不计入这个待办——它们的执行
  逻辑按设计就不在`Story::ApplyChange`里；`DebugPrintChange`已经有JSON分发分支，
  `ChangeControlChange`不需要）。
