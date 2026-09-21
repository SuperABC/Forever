#pragma once

#include "story/script_factory.h"

#include "story/script.h"

#include <vector>
#include <functional>


// 剧情聚合：持有主线剧情Script*数组（当前阶段只有1个，为以后多个主线剧情脚本预留数组容量）
// + 一份全局变量池systemScript（Story.md"变量系统"一节的system.前缀路由目标，纯Container用途，
// 不加载任何milestone）。当前阶段运行时只有这一份Story（AForeverFrameworkActor持有），但
// Script类本身不假设自己是全局唯一实例，见Script.md。
class Story {
public:
	/*
	* 构造剧情聚合：绑定scriptFactory引用成员(Registry::Get().GetScriptFactory())——mod
	* 注册不在这里做，属于Registry全局一次性注册的范围，见Source/Core/common/registry.md
	*/
	Story();

	/*
	* 析构剧情聚合：delete mainScripts里每个Script*+systemScript
	*/
	~Story();

	/*
	* 初始化：创建systemScript，从config.json的"main_story"字段读取ScriptModName，
	* 用Config::GetScriptPath("test")解析实际路径读取主线剧情Script
	*/
	void Init();

	/*
	* 构造一个GameStartEvent，广播给mainScript，同步回调onActions交给调用方
	* （UForeverStoryFrameworkComponent）展示——用回调而不是直接返回vector<ScriptAction>，
	* 是因为ScriptContext.local指向这个函数栈上的GameStartEvent，只在回调同步执行期间有效，
	* 调用方不能把actions/context留到回调返回之后再用（local.前缀求值会变成悬垂指针）。
	* mainScript为空(Init()没有成功创建)时直接返回，不调用onActions。
	* @onActions: 收到匹配出的动作列表和对应变量路由上下文
	* @post: 向Core发起查询的句柄，透传给Script::MatchEvent/WrapScript
	*/
	void BroadcastGameStart(const std::function<void(const std::vector<ScriptAction>&, const ScriptContext&)>& onActions,
		PostHandle* post);

	/*
	* 执行一个变化：dynamic_cast分派，目前只有SetValueChange分支真正执行，其余分支
	* 只打一条"未实现"日志。阶段4占位，等对应变化类型被点名实现时再补分支，见Story.md。
	* @change: 待执行的变化
	* @context: 变量路由上下文（context.self决定SetValueChange作用在哪个Script的变量池上）
	*/
	void ApplyChange(const Change* change, const ScriptContext& context);

	/*
	* 阶段占位：Story域这次还没有需要每帧处理的逻辑（GameStart广播走的是BroadcastGameStart，
	* 只在开局调用一次，不是每帧），空实现——和Map/Populace/Society/Industry/Traffic一起被
	* AForeverFrameworkActor::Tick统一调用一遍，保持"每个域都有Tick"这个形状一致，等Story域
	* 真的有每帧要处理的东西（比如milestone超时之类）时再补内容。
	*/
	void Tick(const Time& currentTime, bool crossedDay, PostHandle* post);

	/*
	* 获取全局变量池（system.前缀路由目标）
	*/
	Script* GetSystemScript() const;

	/*
	* 获取主线剧情Script——用户明确指出一份剧本只能由一个Script表示，这次从
	* vector<Script*>改成单个Script*，不再是数组
	*/
	Script* GetMainScript() const;

private:
	// 引用`Registry::Get().GetScriptFactory()`，不再自己持有ModLoader/ScriptFactory——mod
	// dll的发现/注册只在整个UE进程生命周期里跑一次，见Source/Core/common/registry.md。
	// 构造函数初始化列表里绑定。
	ScriptFactory& scriptFactory;

	// 主线剧情Script（持有所有权）——一份剧本只能由一个Script表示，不是数组
	Script* mainScript = nullptr;

	// 全局变量池（持有所有权）
	Script* systemScript = nullptr;

};
