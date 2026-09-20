#pragma once

#include "../common/utility.h"
#include "../common/error.h"
#include "expression.h"
#include "change.h"

#include <vector>
#include <string>
#include <tuple>


class Dialog;
class Script;

// 分支选项。dialogs/changes都是引用（不持有所有权）——Option自己是被Milestone/Dialog本体
// 持有的值类型，它引用的Dialog*/Change*本体永远挂在某个Milestone上，见Dialog.h整体注释。
class Option {
public:
	/*
	* 构造选项
	* @condition: 显示条件
	* @option: 选项文本
	* @dialogs: 选中后追加的对话列表
	* @changes: 选中后触发的变化列表
	*/
	Option(std::string condition, std::string option,
		std::vector<Dialog*> dialogs, std::vector<const Change*> changes);

	/*
	* 析构选项
	*/
	~Option();

	/*
	* 获取显示条件
	*/
	const std::string& GetCondition() const;

	/*
	* 获取选项文本（未求值，剧情json里的表达式原文）
	*/
	const std::string& GetOption() const;

	/*
	* 获取选中后追加的对话列表
	*/
	std::vector<Dialog*> GetDialogs() const;

	/*
	* 获取选中后触发的变化列表
	*/
	std::vector<const Change*> GetChanges() const;

private:
	// 显示条件
	std::string condition;

	// 选项文本
	std::string option;

	// 选中后追加的对话列表（引用，不持有所有权）
	std::vector<Dialog*> dialogs;

	// 选中后触发的变化列表（引用，不持有所有权）
	std::vector<const Change*> changes;

};

// 对话段：普通台词或分支选项二选一。保持老工程的延迟求值约定——Dialog::GetDialogs()每次
// 返回的是Section的拷贝，EvaluateText在这份拷贝上求值、把结果写进speaking缓存，不会回写
// Dialog本体持有的原始未求值版本，所以同一个Dialog反复播放时，表达式（尤其是引用了self./
// system.等易变变量的部分）每次都会用当时最新的变量值重新求值。
class Section {
public:
	/*
	* 构造对话段（普通台词）
	* @speaker, content, label, voice: 发言者、内容、标签与语音资产路径
	*/
	Section(std::string speaker, std::string content, std::string label, std::string voice);

	/*
	* 构造对话段（分支选项）
	* @options: 选项列表
	*/
	Section(std::vector<Option> options);

	/*
	* 析构对话段
	*/
	~Section();

	/*
	* 判断是否为分支选项段
	*/
	bool IsBranch() const;

	/*
	* 对台词文本中的表达式求值，结果写入内部缓存供GetSpeaking()读取
	* @context: 变量路由上下文
	*/
	void EvaluateText(const ScriptContext& context);

	/*
	* 获取台词（发言者, 内容, 标签, 语音资产路径），返回上一次EvaluateText的求值结果；
	* 未调用过EvaluateText时返回全空字符串
	*/
	std::tuple<std::string, std::string, std::string, std::string> GetSpeaking() const;

	/*
	* 获取选项列表
	*/
	std::vector<Option> GetOptions() const;

	/*
	* 设置所属脚本
	* @script: 脚本对象
	*/
	void SetOwnerScript(Script* script);

	/*
	* 获取所属脚本
	*/
	Script* GetOwnerScript() const;

private:
	// 是否为分支选项
	bool branch;

	// 台词表达式（未求值）
	std::string speakerExpr;
	std::string contentExpr;
	std::string labelExpr;
	std::string voiceExpr;

	// 上一次EvaluateText的求值缓存（发言者, 内容, 标签, 语音资产路径）
	std::tuple<std::string, std::string, std::string, std::string> speaking;

	// 选项列表
	std::vector<Option> options;

	// 所属脚本（引用，不持有所有权）
	Script* ownerScript;

};

// 对话。只在Milestone里持有本体（Milestone::dialogs是OBJECT_HOLDER），别处（Option::dialogs、
// Script::MatchEvent返回的ScriptAction等）一律用裸指针引用，绝不delete。
class Dialog {
public:
	/*
	* 构造对话
	*/
	Dialog();

	/*
	* 析构对话
	*/
	~Dialog();

	/*
	* 添加普通台词段
	* @speaker, content, label, voice: 发言者、内容、标签与语音资产路径
	*/
	void AddDialog(std::string speaker, std::string content, std::string label, std::string voice);

	/*
	* 添加分支选项段
	* @options: 选项列表
	*/
	void AddDialog(std::vector<Option> options);

	/*
	* 获取所有对话段（拷贝，未求值——延迟求值约定见Section注释）
	*/
	std::vector<Section> GetDialogs() const;

	/*
	* 设置显示条件
	* @condition: 条件表达式
	*/
	void SetCondition(std::string condition);

	/*
	* 获取显示条件
	*/
	const std::string& GetCondition() const;

private:
	// 对话段列表
	std::vector<Section> list;

	// 显示条件
	std::string condition;

};
