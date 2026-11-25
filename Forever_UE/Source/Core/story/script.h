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
// 脚本的所有者种类：脚本由哪类对象创建并持有。供"对自己操作"的Change(add_option/remove_option/assign_task/open_shop/
// pay_wage)直接拿到所有者，不用按名字/脚本指针去全图查找，见script.md"所有者"一节。
enum class ScriptOwnerKind { None, Door, Cabin, Citizen, Vehicle, Asset, Organization, Job, Weapon };

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

	// 所有者：创建这份脚本的对象(Door*/Cabin*/Citizen*/Vehicle*/Asset*/Organization*/Job*，void*由kind决定类型)。
	// parent只有Job用：它所属的Organization*。创建脚本的地方在构造时设置，读档重建同样走这些构造函数。
	void SetOwner(ScriptOwnerKind kind, void* owner, void* parent = nullptr);
	void SetOwnerParent(void* parent);
	ScriptOwnerKind GetOwnerKind() const;
	void* GetOwner() const;
	void* GetOwnerParent() const;

	// 稳定键(ScriptSaveCodec::CollectScripts的键，计时器/小游戏归属用)的缓存，第一次算出后记在这里，
	// 之后"对自己建计时器"不用任何查找。空=还没算。创建后键不会变(组织/岗位/市民只增不删)。
	const std::string& GetStableKey() const;
	void SetStableKey(std::string key) const;

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

	// 清空.script文件缓存(释放解析出的Milestone)，下次ReadScript重新读盘——每局开始调一次，改过的脚本才会生效。
	// 还有Script实例活着时(它们引用着缓存里的Milestone)什么都不做并返回false。
	static bool ClearCache();

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
		std::vector<std::string> modDependences; // 这份脚本依赖的mod id(完整id)
		std::unordered_map<std::string, Milestone*> milestones;
	};

	// 脚本缓存：文件路径 -> 该文件解析出的全部内容（本体持有者）
	static std::unordered_map<std::string, FileCache> caches;
	// 当前存活的Script实例数，ClearCache据此判断能不能安全释放Milestone。
	static int liveCount;

	std::unordered_map<std::string, MilestoneNode> milestones; // Milestone*引用缓存，不持有所有权
	std::vector<MilestoneNode*> actives; // 活动里程碑
	std::unordered_map<std::string, ValueType> variables;

	ScriptOwnerKind ownerKind = ScriptOwnerKind::None;
	void* owner = nullptr;
	void* ownerParent = nullptr;
	mutable std::string stableKey;

};

// 按mod的scriptModName/milestoneNames创建Script，规则见script.md"MakeScript"。
Script* MakeScript(ScriptFactory* factory, const std::string& scriptModName, const std::vector<std::string>& milestoneNames);
