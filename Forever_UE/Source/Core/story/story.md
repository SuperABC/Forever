# story.h / story.cpp

阶段4新落地（新工程首次迁移这个文件）。参考老工程`Core/story/story.h/.cpp`的`Story`聚合类，
但按用户这次的范围砍掉了大量当前用不到的功能（计时器、对话历史、全局消息队列等），只保留
"完整跑通Script->Milestone->Event/Dialog/Change架构到GameStart广播+SetValue执行"这一步必需的
最小集合。

## 持有的内容

- `ScriptFactory scriptFactory` + `ModLoader modLoader`：和`Map::InitTerrains()`同款的mod注册
  手法——`modLoader.RegisterConcept<ScriptFactory>(Config::GetMods(), "RegisterModScripts",
  "FinishModScripts", &scriptFactory)`，`config.json`里`"script_mods"`已经配好
  `["wxdj", "empty --test true"]`（`Source/Core/common/loader.cpp`的`kModConceptDescriptors`
  早就有`Scripts`这一项，`Forever_Mod/Empty`提供id为`"empty"`的`EmptyScript`），不需要新增
  任何mod发现/注册的基础设施。
- `std::vector<Script*> mainScripts`：主线剧情Script数组。用户点2字面写的是"数组"，这次按数组
  实现（为以后多个主线剧情脚本预留），但当前阶段`Init()`里只塞1个（从`test.json`读取）。
- `Script* systemScript`：`system.`前缀路由的目标（见`Dependence/story/expression.md`
  "ScriptContext"一节）——一个不加载任何milestone、纯粹当`Container`变量池用的`Script`，同样
  走`"empty"`id创建（`test.json`的内容和具体挂载哪个`ScriptMod`无关，只是需要"随便一个能创建
  出来的`ScriptMod`"）。

## `BroadcastGameStart`用回调而不是直接返回`vector<ScriptAction>`

```cpp
void BroadcastGameStart(const std::function<void(const std::vector<ScriptAction>&,
    const ScriptContext&)>& onActions, PostHandle* post);
```

`post`是新增的第二个参数：`BroadcastGameStart`自己不解读它，只是原样转发给每个
`script->MatchEvent(&event, context, post)`（进而透传到`mod->WrapScript`）——mod侧要用
`PostHandle`向Core发起查询（这次唯一的例子是"随机挑一个citizen"，见`Dependence/story/
script_mod.md`），必须先能拿到这个句柄，`Story`这一层只是编排调用的中间人，不需要关心
`post`具体指向哪个`PostHandle`实现（这次由调用方`UForeverStoryFrameworkComponent::
BroadcastGameStart`现场构造一个`PostImplement`传进来，见`Core/common/implement.md`）。

`ScriptContext.local`指向这个函数栈上的局部`GameStartEvent`——如果直接把`vector<ScriptAction>`
和`ScriptContext`返回给调用方、让调用方以后再处理（比如攒起来下一帧再显示），`local`就会变成
悬垂指针（`GameStartEvent`早就随函数返回被析构了），一旦剧情内容用到`local.xxx`就是真实的
use-after-free。改成回调、在`GameStartEvent`还活着的这个函数调用帧内**同步**处理完，从根源上
避免这个问题。这不是"防御性”多此一举——`GameStartEvent`当前没有字段，这次不写`local.`也不会
出问题，但以后随便一个真的有字段的事件（比如`GlobalMessageEvent`）一旦被用同样的模式广播，不用
回调就会踩到这个坑，所以从`GameStartEvent`这个最简单的例子开始就用对的模式。

多个`mainScripts`场景下，`onActions`按脚本各调用一次（`context.self`绑定成对应脚本），不是
先把所有脚本的actions拍平再统一调一次——这样`Story::ApplyChange`按`context.self`执行
`SetValueChange`时，作用的`Container`天然就是产生这批actions的那个`Script`，不需要额外的
"哪个action属于哪个脚本"的归属信息。

## `ApplyChange`

`dynamic_cast`分派，目前只有`SetValueChange`分支真正执行：
`context.self->SetValue(setValue->GetVariable(), setValue->GetValue().EvaluateValue(context))`。
其余41种变化类型只打一条`debugf`"未实现"日志，不崩溃也不抛异常（这条路径每次匹配后都会被调用，
容错风格比JSON解析阶段更宽松，见`Dependence/story/change.md`"JSON分发"一节）。这个分派函数是
`Story`的方法而不是`Change`类自己的虚方法，延续老工程"`Change`是纯数据类"的设计。

## test.json路径

`Resource/Story/test.json`，相对`Config::GetConfigDir()`（`Resource/Config/`）的固定相对路径，
`Story::Init()`直接用`std::filesystem::path`拼出来读取。这次没有复活老工程
`Config::GetStories()`/`AddScript`/`RemoveScript`那一整套多剧情路径管理——`config.md`里明确
写着这块"仍未迁移"，用户这次的12条需求也没有提到要恢复它，先用一个硬编码路径把主线剧情跑通。

## 依赖关系

- 依赖：`script.h`、`script_factory.h`、`common/loader.h`（`ModLoader`）、`common/config.h`
  （`Config::GetMods`/`GetConceptMods`/`GetConfigDir`）、`event.h`（`GameStartEvent`）、
  `change.h`（`SetValueChange`）、`Dependence/common/handle.h`（`PostHandle`，
  `BroadcastGameStart`透传给`Script::MatchEvent`）。
- 被谁依赖：`Forever/Framework/ForeverFrameworkActor.cpp`（持有`Story*`，生命周期管理方式和
  `Map*`/`Populace*`完全一致）、`Forever/Framework/ForeverStoryFrameworkComponent.cpp`
  （展示`BroadcastGameStart`的结果，现场构造`PostImplement`作为`post`参数传入）。

## 待办/后续阶段

- 计时器（`CreateTimer`/`PopExpiredTimers`）、对话历史（`AddTalk`/`GetHistory`）、全局消息队列
  （`AddMessage`/`PopMessages`）——老工程`Story`有这些，这次全部没有迁，等对应功能被点名时再加。
- 老工程`Config::GetStories`/`AddScript`/`RemoveScript`多剧情路径管理。
- `mainScripts`目前只有1个，多个主线剧情脚本之间如何分工（比如按章节切换）还没有设计。
