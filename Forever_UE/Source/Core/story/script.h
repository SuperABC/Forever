#pragma once

#include "class.h"

#include "story/script_mod.h"
#include "story/script_factory.h"
#include "common/utility.h"

#include "story/milestone.h"

#include <string>
#include <unordered_map>


// 剧情脚本：Script->Milestone->Event/Dialog/Change架构的顶层持有者，自己就是一份Container
// 变量池（self.前缀路由的目标）。可以在任意地方单独new出来（不假设自己是全局唯一实例）——
// Story域会持有主线剧情的Script*数组，以后Citizen/Elevator等也会各自持有自己的Script*，
// 详见Script.md"顶层原则"。
class Script : public Container {
public:
	/*
	* 禁止默认构造
	*/
	Script() = delete;

	/*
	* 通过类型从工厂构造
	* @factory: 脚本工厂
	* @id: 脚本Mod的注册id
	*/
	Script(ScriptFactory* factory, const std::string& id);

	/*
	* 析构脚本：走factory->DestroyScript(mod)释放mod实例，遵循跨DLL deleter约定
	*/
	~Script();

	/*
	* 获取类型
	*/
	std::string GetType() const;

	/*
	* 获取名称
	*/
	std::string GetName() const;

	/*
	* 持有的mod实例——以后需要读mod内部数据的调用方直接用，不需要另外拷贝。
	*/
	ScriptMod* GetMod() const;

	/*
	* 获取变量值，返回 {是否存在, 值}（Container接口，self.前缀路由的实现）
	* @name: 变量名称
	*/
	std::pair<bool, ValueType> GetValue(const std::string& name) const override;

	/*
	* 设置变量值（Container接口）
	* @name: 变量名称
	* @value: 变量值
	*/
	void SetValue(const std::string& name, ValueType value) override;

	/*
	* 移除变量
	* @name: 变量名称
	*/
	void RemoveValue(const std::string& name);

	/*
	* 获取任务描述：拼接所有active里程碑的目标/描述文本
	*/
	std::string GetTask() const;

	/*
	* 脚本逻辑重载入口，转调mod->WrapScript，返回mod->actionStack这一层的引用（不能按值
	* 返回，见ScriptMod.h"WrapScript"一节的跨DLL堆说明）。调用方读完之后必须调用一次
	* AutoPop()弹出这一层，MatchEvent内部已经处理好这个配对，不需要外部调用方关心。
	* @event: 触发这批actions的运行时事件
	* @actions: 只读动作列表
	* @context: 变量路由上下文
	* @post: 向Core发起查询的句柄，透传给mod->WrapScript，见ScriptMod.h
	*/
	std::vector<ScriptAction>& WrapScript(const Event* event, const std::vector<ScriptAction>& actions,
		const ScriptContext& context, PostHandle* post);

	/*
	* 转调mod->AutoPop()，弹出WrapScript这次压入的那一层，见ScriptMod.h。
	*/
	void AutoPop();

	/*
	* 读取剧本json文件到缓存
	* @path: 剧本文件路径
	*/
	static void ReadScript(const std::string& path);

	/*
	* 从缓存加载里程碑到当前脚本，重建actives/premise
	* @path: 剧本文件路径
	*/
	void ReadMilestones(const std::string& path);

	/*
	* 匹配事件，返回触发的动作列表（已经过WrapScript改写）
	* @event: 触发事件
	* @context: 变量路由上下文（context.self会被强制设为this，忽略调用方传入的self）
	* @post: 向Core发起查询的句柄，透传给WrapScript
	*/
	std::vector<ScriptAction> MatchEvent(Event* event, ScriptContext context, PostHandle* post);

	/*
	* 停用指定里程碑
	* @name: 里程碑名称
	*/
	void DeactivateMilestone(const std::string& name);

	/*
	* 清空里程碑、活动状态与变量
	*/
	void ClearContext();

private:
	/*
	* 从JSON解析事件列表。当前阶段只识别"game_start"这一种type，其余type字符串抛异常——
	* 其余30种事件类型的JSON分发分支等该类型被点名实现时再补，见Script.md。
	* @root: JSON节点
	*/
	static std::vector<Event*> BuildEvent(const JsonValue& root);

	/*
	* 从JSON解析变化列表。当前阶段只识别"set_value"这一种type，其余type字符串抛异常——
	* 其余41种变化类型的JSON分发分支等该类型被点名实现时再补，见Script.md。
	* @root: JSON节点
	*/
	static std::vector<Change*> BuildChanges(const JsonValue& root);

	/*
	* 从JSON解析对话列表
	* @root: JSON节点
	*/
	static std::vector<Dialog*> BuildDialogs(const JsonValue& root);

	/*
	* 从JSON节点解析表达式（节点本身是一个字符串）
	* @root: JSON节点
	*/
	static Expression BuildExpression(const JsonValue& root);

	/*
	* 从JSON解析后续里程碑名列表
	* @root: JSON节点
	*/
	static std::vector<std::string> BuildSubsequences(const JsonValue& root);

	// 模组对象
	ScriptMod* mod;

	// 工厂
	ScriptFactory* factory;

	// 脚本类型
	std::string type;

	// 脚本名称
	std::string name;

	// 脚本缓存：文件路径 -> 该文件解析出的里程碑表（名称->Milestone*，本体持有者）
	static std::unordered_map<std::string, std::unordered_map<std::string, Milestone*>> caches;

	// 里程碑列表（引用缓存里的Milestone*，不持有所有权）
	std::unordered_map<std::string, MilestoneNode> milestones;

	// 活动里程碑
	std::vector<MilestoneNode*> actives;

	// 变量列表
	std::unordered_map<std::string, ValueType> variables;

};
