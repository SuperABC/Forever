# story.h / story.cpp

阶段4新落地（新工程首次迁移这个文件）。参考老工程`Core/story/story.h/.cpp`的`Story`聚合类，
但按用户这次的范围砍掉了大量当前用不到的功能（计时器、对话历史、全局消息队列等），只保留
"完整跑通Script->Milestone->Event/Dialog/Change架构到GameStart广播+SetValue执行"这一步必需的
最小集合。

## 持有的内容

- `ScriptFactory& scriptFactory`：引用成员，构造函数初始化列表里绑定
  `Registry::Get().GetScriptFactory()`——最初这里和`Map::InitTerrains()`一样自己持有一份
  `ScriptFactory scriptFactory;`+`ModLoader modLoader;`，每次`new Story()`（每次开局）都会
  重新扫描/注册一遍script mod dll，被要求和`Map`/`Populace`一起统一挪进`Registry`
  （`Source/Core/common/registry.md`），mod dll的发现/注册现在只在整个UE进程生命周期里跑
  一次。`config.json`里`"script_mods"`已经配好`["wxdj", "empty --test true"]`
  （`Source/Core/common/loader.cpp`的`kModConceptDescriptors`早就有`Scripts`这一项，
  `Forever_Mod/Empty`提供id为`"empty"`的`EmptyScript`）。
- `Script* mainScript`：主线剧情Script——**这次从`std::vector<Script*> mainScripts`改成
  单个`Script*`**（用户明确指出"一份剧本只能由一个Script表示"，数组没有意义），
  `Init()`从`Resource/Story/test.script`读取。
- `Script* systemScript`：`system.`前缀路由的目标（见`Dependence/story/expression.md`
  "ScriptContext"一节）——一个不加载任何milestone、纯粹当`Container`变量池用的`Script`。

## ScriptModName不再硬编码，从`config.json`的`"main_story"`字段读

**这里曾经硬编码过`constexpr const char* kEmptyScriptId = "empty";`**，`systemScript`/
`mainScript`（当时还是`mainScripts`数组）都固定用这个id创建——这替Mod/配置做了"用哪个
ScriptMod"的决策，且和`Job::Job()`当时的同款硬编码是同一类问题（见`job.md`"Script配置"
一节）。修复后，`Story::Init()`改成从`Config::GetMainStoryScriptModName()`（读
`config.json`的`"main_story"`字段）拿到真正的`scriptModName`，字段缺失或对应
ScriptMod没有注册时跳过初始化：

```cpp
void Story::Init() {
	string scriptModName = Config::GetMainStoryScriptModName();
	if (scriptModName.empty() || !scriptFactory.CheckRegistered(scriptModName)) {
		debugf("Warning: main_story script mod not configured or not registered, Story::Init skipped.\n");
		return;
	}
	systemScript = new Script(&scriptFactory, scriptModName);
	mainScript = new Script(&scriptFactory, scriptModName);
	mainScript->ReadMilestones(Config::GetScriptPath("test"));
}
```

`config.json`当前配的是`"main_story": "empty"`。

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

`mainScript`为空（`Init()`没有成功创建，比如`main_story`没配对）时`BroadcastGameStart`
直接返回，不调用`onActions`。

## `ApplyChange`

`dynamic_cast`分派，目前只有`SetValueChange`分支真正执行：
`context.self->SetValue(setValue->GetVariable(), EvaluateExpression(setValue->GetValue(),
context))`——`GetValue()`返回的是`std::string`（DSL源码文本，不是预先解析好的`Expression`
对象），`EvaluateExpression`现场`Parse`+求值一步到位，原因见`Dependence/story/change.md`
"字段类型是`std::string`"一节。
其余41种变化类型只打一条`debugf`"未实现"日志，不崩溃也不抛异常（这条路径每次匹配后都会被调用，
容错风格比JSON解析阶段更宽松，见`Dependence/story/change.md`"JSON分发"一节）。这个分派函数是
`Story`的方法而不是`Change`类自己的虚方法，延续老工程"`Change`是纯数据类"的设计。**这次重构
`AForeverFrameworkActor::Tick`后，`Story::ApplyChange`不再被直接调用**——统一由
`AForeverFrameworkActor::ApplyChange`（新增的Change消费入口）转发给`map`/`populace`/
`society`/`industry`/`traffic`/`story`六个域各自的`ApplyChange`，`Story`是其中唯一
保留"未实现"警告的一个（其余五个域都是空占位，故意不打警告，避免同一个Change被六个域
各打一遍重复日志），详见`Source/Forever/Framework/ForeverFrameworkActor.md`"统一的Change
消费入口：`ApplyChange`"一节。

## `Tick`：占位，保持"每个Core域都有Tick"的形状一致

这次重构新增的空实现方法——`AForeverFrameworkActor::Tick`每帧会挨个调用`map`/`populace`/
`society`/`industry`/`traffic`/`story`六个域各自的`Tick`，`Story`域目前没有需要每帧处理
的逻辑（GameStart广播走的是`BroadcastGameStart`，只在开局调用一次，不是每帧），空实现，
等Story域真的有每帧要处理的东西（比如milestone超时之类）时再补内容。

## test.script路径

`Resource/Story/test.script`（原名`test.json`，这次连同"Script配置"一起改名——`Config::
AddResourcePath`只按`.script`扩展名扫描，见`config.md`"resource_path"一节），实际存放
位置由`config.json`的`"resource_path"`数组（`["../Story"]`）决定，`Story::Init()`用
`Config::GetScriptPath("test")`按bare文件名反查实际路径，不再自己拼`filesystem::path`。
这次没有复活老工程`Config::GetStories()`/`AddScript`/`RemoveScript`那一整套多剧情路径
管理——`config.md`里明确写着这块"仍未迁移"，先用固定的"test"这一个bare名字把主线剧情
跑通。

## 依赖关系

- 依赖：`script.h`、`script_factory.h`、`common/registry.h`（`Story`构造函数绑定
  `scriptFactory`，见`registry.md`）、`common/config.h`（`Config::
  GetMainStoryScriptModName()`/`GetScriptPath()`）、`event.h`（`GameStartEvent`）、
  `change.h`（`SetValueChange`）、`Dependence/common/handle.h`（`PostHandle`，
  `BroadcastGameStart`透传给`Script::MatchEvent`）。
- 被谁依赖：`Forever/Framework/ForeverFrameworkActor.cpp`（持有`Story*`，生命周期管理方式和
  `Map*`/`Populace*`完全一致）、`Forever/Framework/ForeverStoryFrameworkComponent.cpp`
  （展示`BroadcastGameStart`的结果，现场构造`PostImplement`作为`post`参数传入；这个函数
  这次还顺带遍历`Society`下所有`Organization`/`Job`各自的`Script`广播一次game_start，
  见`ForeverStoryFrameworkComponent.md`）。

## 待办/后续阶段

- 计时器（`CreateTimer`/`PopExpiredTimers`）、对话历史（`AddTalk`/`GetHistory`）、全局消息队列
  （`AddMessage`/`PopMessages`）——老工程`Story`有这些，这次全部没有迁，等对应功能被点名时再加。
- 老工程`Config::GetStories`/`AddScript`/`RemoveScript`多剧情路径管理。
