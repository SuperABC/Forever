#pragma once

#include "class.h"

#include "story/script_mod.h"
#include "story/script_factory.h"
#include "common/utility.h"

#include "story/milestone.h"

#include <string>
#include <vector>
#include <unordered_map>

// 剧情脚本：Script->Milestone->Event/Dialog/Change架构的顶层持有者，自己就是一份Container
// 变量池（self.前缀路由的目标）。不假设全局唯一，设计见script.md。
class Script : public Container {
	// 存档：ScriptSaveCodec直接读写私有字段(script_save.md)。
	friend class ScriptSaveCodec;
public:
	Script() = delete;

	Script(ScriptFactory* factory, const std::string& id);

	~Script(); // 走factory->DestroyScript(mod)释放mod实例

	std::string GetType() const;

	std::string GetName() const;

	ScriptMod* GetMod() const;

	std::pair<bool, ValueType> GetValue(const std::string& name) const override;

	void SetValue(const std::string& name, ValueType value) override;

	void RemoveValue(const std::string& name);

	std::string GetTask() const;

	// 转调mod->WrapScript，返回mod->actionStack这一层的引用(不能按值返回，跨DLL堆)；
	// 读完必须AutoPop()，MatchEvent内部已配对处理。
	std::vector<ScriptAction>& WrapScript(const Event* event, const std::vector<ScriptAction>& actions,
		const ScriptContext& context, PostHandle* post);

	void AutoPop();

	// 读取并缓存一份.script文件(已缓存则不重复读盘)。
	static void ReadScript(const std::string& path);

	void ReadMilestones(const std::string& path);

	// 以下三个静态查询(内部先ReadScript)不需要任何Script实例，字段缺失/文件不存在返回空，见script.md。
	static const std::vector<std::string>& GetNameReserve(const std::string& path);

	static const std::unordered_map<std::string, ValueType>& GetGlobalSettings(const std::string& path);

	static const std::vector<std::string>& GetModDependences(const std::string& path);

	// 匹配事件，返回触发的动作列表(已经过WrapScript改写)；context.self会被强制设为this。
	std::vector<ScriptAction> MatchEvent(Event* event, ScriptContext context, PostHandle* post);

	void DeactivateMilestone(const std::string& name);

	void ClearContext();

private:
	// JSON分发：未实现的type字符串抛异常，见script.md。
	static std::vector<Event*> BuildEvent(const JsonValue& root);

	static std::vector<Change*> BuildChanges(const JsonValue& root);

	static std::vector<Dialog*> BuildDialogs(const JsonValue& root);

	static std::vector<std::string> BuildSubsequences(const JsonValue& root);

	ScriptMod* mod;
	ScriptFactory* factory;
	std::string type;
	std::string name;

	// 一份.script文件解析出的全部内容(milestones+三个顶层字段)，都是纯JSON数据，不依赖Script实例。
	struct FileCache {
		std::vector<std::string> nameReserve;
		std::unordered_map<std::string, ValueType> globalSettings;
		std::vector<std::string> modDependences; // mod id，不带concept前缀
		std::unordered_map<std::string, Milestone*> milestones;
	};

	// 脚本缓存：文件路径 -> 该文件解析出的全部内容（本体持有者）
	static std::unordered_map<std::string, FileCache> caches;

	std::unordered_map<std::string, MilestoneNode> milestones; // Milestone*引用缓存，不持有所有权
	std::vector<MilestoneNode*> actives; // 活动里程碑
	std::unordered_map<std::string, ValueType> variables;

};

// 按mod的scriptModName/milestoneNames创建Script，规则见script.md"MakeScript"。
Script* MakeScript(ScriptFactory* factory, const std::string& scriptModName, const std::vector<std::string>& milestoneNames);
