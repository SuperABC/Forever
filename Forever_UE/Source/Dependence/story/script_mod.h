#pragma once

#include "common/mod_state.h"
#include "expression.h"
#include "change.h"
#include "../common/handle.h"

#include <deque>
#include <string>
#include <variant>
#include <vector>


class Event;
class Dialog;

// 一次Script::MatchEvent匹配命中后要执行的一个动作；指针是引用，本体挂在Milestone上，绝不delete
using ScriptAction = std::variant<const Dialog*, const Change*>;

// ScriptMod基类，说明见script_mod.md；concept/domain对照表见Source/Dependence/README.md
class ScriptMod {
public:
	ScriptMod() = default;
	virtual ~ScriptMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 脚本逻辑重载入口，Script::MatchEvent匹配出一批actions后调用；重载时先AutoCopy(actions)再改写actionStack.back()。
	// 默认实现原样透传。必须返回void而不是按值返回vector（跨DLL堆损坏），见script_mod.md
	virtual void WrapScript(const Event* event,
		const std::vector<ScriptAction>& actions, const ScriptContext& context, PostHandle* post) {
		AutoCopy(actions);
	}

	// 弹出WrapScript压入的那一层；虚函数，保证vector的释放发生在构造该mod实例的模块
	virtual void AutoPop() {
		if (!actionStack.empty()) {
			actionStack.pop_back();
		}
	}

	// 在actionStack里新增一层；非虚，只会被已虚分派到mod侧的WrapScript重载调用
	void AutoCopy(const std::vector<ScriptAction>& actions) {
		actionStack.push_back(actions);
	}

	// 在actionStack最上层找label匹配的PlaceHolderChange，返回下标，找不到返回-1；非虚，理由同AutoCopy
	int FindLabel(const std::string& label, const ScriptContext& context) const {
		if (actionStack.empty()) return -1;
		const std::vector<ScriptAction>& top = actionStack.back();
		for (size_t i = 0; i < top.size(); i++) {
			if (auto changePtr = std::get_if<const Change*>(&top[i])) {
				if (auto placeholder = dynamic_cast<const PlaceHolderChange*>(*changePtr)) {
					if (ToString(EvaluateExpression(placeholder->GetLabel(), context)) == label) {
						return static_cast<int>(i);
					}
				}
			}
		}
		return -1;
	}

	// 脚本动作栈：每次WrapScript对应一层，Core只读actionStack.back()的引用，不持有/析构
	std::deque<std::vector<ScriptAction>> actionStack;

	// ---- 存档(见Source/Dependence/common/mod_state.md) ----
	// mod私有数据的存档/读档钩子，默认不存任何东西
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	// SaveState写出数据的格式版本，默认0
	virtual int GetStateVersion() const { return 0; }
	// 唯一实例名用的静态计数器，存档时保存、读档后恢复；没有计数器的类保持默认(返回-1、Set不做事)
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
