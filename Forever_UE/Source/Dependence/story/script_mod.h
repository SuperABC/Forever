#pragma once

#include <deque>
#include <string>
#include <variant>
#include <vector>

#include "expression.h"

class Event;
class Dialog;
class Change;

// 一次Script::MatchEvent匹配命中后要执行的一个动作，Dialog*/Change*都是引用（本体永远挂在
// 某个Milestone上，见dialog.h/change.h整体注释），这里绝不delete。
using ScriptAction = std::variant<const Dialog*, const Change*>;

// 阶段3骨架:ScriptMod只声明GetType()/GetName()两个纯虚接口,用于验证Mod发现/加载/
// 注册机制本身。真正的story域业务接口留到阶段4迁移story系统时才补上,完整21个
// concept x 8个domain对照表见 Source/Dependence/README.md。
class ScriptMod {
public:
	ScriptMod() = default;
	virtual ~ScriptMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// config.json里"script_mods"数组中该mod id后面的命令行式参数字符串(如
	// "pengzhan --density 1.0"里的"--density 1.0"),由<Concept>Factory::Create<Concept>
	// 创建实例后立刻调用一次。默认空实现,不需要参数的mod不用重写,格式解析完全由重写者
	// 自己决定。
	virtual void ApplyArgs(const std::string& args) {}

	// 脚本逻辑重载入口，Script::MatchEvent匹配出一批actions之后、返回给调用方之前会调用一次，
	// 让mod有机会用自己长期持有的Dialog*/Change*改写/插入内容（json表达不了的复杂剧情逻辑走
	// 这里，见Script.md"顶层原则"）。重载时应在函数开头先调用AutoCopy(actions)，把这一层要
	// 处理的动作先搬进actionStack，再按需替换/插入内容。默认实现：AutoCopy(actions)，即原样
	// 透传，不做任何改写；真正按PlaceHolderChange标签替换成mod自己持有的Dialog/Change这套
	// 改写逻辑，留到用户点名WrapScript时再实现。
	//
	// **不能改成返回std::vector<ScriptAction>（按值）**——这里是虚方法，不override时vtable
	// 槽位指向的默认实现会被编译进"没有override它的那个mod DLL"自己的目标文件里（每个mod DLL
	// 各自持有一份这个inline函数的机器码，用各自DLL的CRT堆分配返回值的vector缓冲区）。实测
	// 验证过：改成按值返回vector后，Empty.dll默认实现里`return actions`构造的新vector在
	// Empty.dll的堆上分配，被Core/Forever.dll这边的代码接收后迟早要析构，两边CRT堆不一致，
	// 触发EXCEPTION_ACCESS_VIOLATION（PIE里实测复现，见script_mod.md）。改成返回void+读
	// actionStack.back()引用，能保证vector的分配和析构全程发生在同一侧（构造这个ScriptMod
	// 实例的那个模块），是和[[memory:cross_dll_allocator_crash]]同一类问题的对应解法。
	// @event: 触发这批actions的运行时事件
	// @actions: 只读动作列表
	// @context: 变量路由上下文
	virtual void WrapScript(const Event* event,
		const std::vector<ScriptAction>& actions, const ScriptContext& context) {
		AutoCopy(actions);
	}

	// 处理完当前层的所有动作之后，Core会调用它来弹出WrapScript这次压入的那一层；是虚函数，
	// 即使mod不重载，也会走mod自己所在模块链接的这份默认实现，弹出操作（vector析构、堆释放）
	// 天然发生在构造这个ScriptMod对象的那个模块里，不需要mod额外做任何事。
	virtual void AutoPop() {
		if (!actionStack.empty()) {
			actionStack.pop_back();
		}
	}

	// 在actionStack里新增一层，把这一层要处理的动作列表搬进去；应在WrapScript重载开头调用。
	// 非虚：只会被(已经虚分派到mod侧的)WrapScript重载从里面调用，天然已经跑在mod自己的模块里。
	// @actions: 只读动作列表
	void AutoCopy(const std::vector<ScriptAction>& actions) {
		actionStack.push_back(actions);
	}

	// 脚本动作栈：每次WrapScript调用对应一层，栈本身和栈里每个vector的分配/释放都发生在
	// 构造这个ScriptMod实例的那个模块里（见AutoCopy/AutoPop），Core只通过引用读取
	// actionStack.back()，绝不直接持有/析构它。
	std::deque<std::vector<ScriptAction>> actionStack;
};
