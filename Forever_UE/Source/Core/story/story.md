# story.h / story.cpp

## 职责

`Story`是Script->Milestone->Event/Dialog/Change架构的聚合入口，负责GameStart广播+
`SetValueChange`执行这条链路。不包含计时器、对话历史、全局消息队列等功能（见"待办"）。

## 持有的内容

- `ScriptFactory& scriptFactory`：引用成员，构造函数初始化列表里绑定
  `Registry::Get().GetScriptFactory()`——mod dll的发现/注册只在整个UE进程生命周期里跑一次，
  统一挂在`Registry`（见`Source/Core/common/registry.md`），不是每个`Story`实例各自持有一份。
  `config.json`里`"script_mods"`已经配好`["wxdj", "empty --test true"]`
  （`Source/Core/common/loader.cpp`的`kModConceptDescriptors`有`Scripts`这一项，
  `Forever_Mod/Empty`提供id为`"empty"`的`EmptyScript`）。
- `Script* mainScript`：主线剧情Script——一份剧本只能由一个Script表示，`Init()`从
  `Resource/Story/test.script`读取。
- `Script* systemScript`：`system.`前缀路由的目标（见`Dependence/story/expression.md`
- `Script* globalScript`：全局变量池，没有前缀的变量(`$$x`、`set_value`的`variable`)路由到这里，脚本可读可写；`systemScript`是系统变量，脚本只读、只由C++写。
  "ScriptContext"一节）——一个不加载任何milestone、纯粹当`Container`变量池用的`Script`。

## `ScriptModName`从`config.json`的`"main_story"`字段读

`Story::Init()`从`Config::GetMainStoryScriptModName()`（读`config.json`的`"main_story"`
字段）拿到真正的`scriptModName`，字段缺失或对应ScriptMod没有注册时跳过初始化：

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

`post`自己不解读，只是原样转发给每个`script->MatchEvent(&event, context, post)`（进而透传到
`mod->WrapScript`）——mod侧要用`PostHandle`向Core发起查询（唯一的例子是"随机挑一个citizen"，
见`Dependence/story/script_mod.md`），必须先能拿到这个句柄，`Story`这一层只是编排调用的中间人，
不需要关心`post`具体指向哪个`PostHandle`实现（调用方`UForeverStoryFrameworkComponent::
BroadcastGameStart`现场构造一个`PostImplement`传进来，见`Core/common/implement.md`）。

用回调而不是返回值，是因为`ScriptContext.local`指向函数栈上的局部`GameStartEvent`——如果把
`vector<ScriptAction>`和`ScriptContext`直接返回给调用方、让调用方以后再处理，`local`就会变成
悬垂指针（`GameStartEvent`随函数返回被析构），一旦剧情内容用到`local.xxx`就是use-after-free。
改成回调、在`GameStartEvent`还活着的这个函数调用帧内**同步**处理完，从根源上避免这个问题；
`GameStartEvent`当前没有字段，这次用不用回调都不出问题，但任何有字段的事件（比如
`GlobalMessageEvent`）一旦被用同样的模式广播就会踩到这个坑，所以统一按这个模式写。

`mainScript`为空（`Init()`没有成功创建，比如`main_story`没配对）时`BroadcastGameStart`
直接返回，不调用`onActions`。

## `ApplyChange`

`dynamic_cast`分派，目前只有`SetValueChange`分支真正执行：变量名按`ResolveVariableName`
(和读`$$x`同一条规则)决定写到哪——`self.`前缀写`context.self`(当前脚本自己的变量)，没有前缀写
`context.global`(全局变量)；`system.`(系统变量，只由C++写)和`local.`(事件字段)脚本不能写，忽略并打警告；值是`EvaluateExpression(setValue->GetValue(),
context)`——`GetValue()`返回的是`std::string`（DSL源码文本，不是预先解析好的`Expression`
对象），`EvaluateExpression`现场`Parse`+求值一步到位，原因见`Dependence/story/change.md`
"字段类型是`std::string`"一节。
其余类型返回`false`(不认识)。这个分派函数是`Story`的方法而不是`Change`类自己的虚方法——`Change`是
纯数据类。`AForeverFrameworkActor::ApplyChange`是统一的Change消费入口，转发给`map`/`populace`/`society`/
`industry`/`traffic`/`story`/`player`七个域各自的`ApplyChange`，每个域返回自己认不认识这个类型，七个域都不
认识时由FrameworkActor打一条"未实现"警告(`UE_LOG`)，详见
`Source/Forever/Framework/ForeverFrameworkActor.md`"统一的Change消费入口：`ApplyChange`"一节。

## `Tick`：占位，保持"每个Core域都有Tick"的形状一致

空实现——`AForeverFrameworkActor::Tick`每帧会挨个调用`map`/`populace`/`society`/`industry`/
`traffic`/`story`六个域各自的`Tick`，`Story`域目前没有需要每帧处理的逻辑（GameStart广播走
`BroadcastGameStart`，只在开局调用一次，不是每帧），等Story域真的有每帧要处理的东西（比如
milestone超时之类）时再补内容。

## test.script路径

`Resource/Story/test.script`，实际存放位置由`config.json`的`"resource_paths"`数组
（`["../Story"]`）决定，`Story::Init()`用`Config::GetScriptPath("test")`按bare文件名反查
实际路径，不自己拼`filesystem::path`。`Config::AddResourcePath`只按`.script`扩展名扫描
（见`config.md`"resource_paths"一节）。目前没有多剧情路径管理机制，固定用"test"这一个bare
名字跑主线剧情（见"待办"）。

## 依赖关系

- 依赖：`script.h`、`script_factory.h`、`common/registry.h`（`Story`构造函数绑定
  `scriptFactory`，见`registry.md`）、`common/config.h`（`Config::
  GetMainStoryScriptModName()`/`GetScriptPath()`）、`event.h`（`GameStartEvent`）、
  `change.h`（`SetValueChange`）、`Dependence/common/handle.h`（`PostHandle`，
  `BroadcastGameStart`透传给`Script::MatchEvent`）。
- 被谁依赖：`Forever/Framework/ForeverFrameworkActor.cpp`（持有`Story*`，生命周期管理方式和
  `Map*`/`Populace*`完全一致）、`Forever/Framework/ForeverStoryFrameworkComponent.cpp`
  （展示`BroadcastGameStart`的结果，现场构造`PostImplement`作为`post`参数传入；这个函数
  还会遍历`Society`下所有`Organization`/`Job`各自的`Script`广播一次game_start，见
  `ForeverStoryFrameworkComponent.md`）。

## 待办/后续阶段

- 计时器（`CreateTimer`/`PopExpiredTimers`）、对话历史（`AddTalk`/`GetHistory`）、全局消息队列
  （`AddMessage`/`PopMessages`），等对应功能被点名时再加。
- 多剧情路径管理（类似`AddScript`/`RemoveScript`），等需要同时维护多份剧情时再加。
