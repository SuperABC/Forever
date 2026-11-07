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

// 分支选项：值类型(会被拷贝)，dialogs/changes只是引用，所有权说明见dialog.md
class Option {
public:
	Option(std::string condition, std::string option,
		std::vector<Dialog*> dialogs, std::vector<const Change*> changes);

	~Option();

	const std::string& GetCondition() const;

	const std::string& GetOption() const;

	std::vector<Dialog*> GetDialogs() const;

	std::vector<const Change*> GetChanges() const;

private:
	// 显示条件
	std::string condition;

	// 选项文本
	std::string option;

	// 选中后追加的对话列表（引用，本体由所属的Dialog在析构时释放）
	std::vector<Dialog*> dialogs;

	// 选中后触发的变化列表（引用，本体由所属的Dialog在析构时释放）
	std::vector<const Change*> changes;

};

// 对话段：普通台词或分支选项二选一。台词字段延迟求值，见dialog.md
class Section {
public:
	Section(std::string speaker, std::string content, std::string label, std::string voice, float background = 0.f);

	Section(std::vector<Option> options);

	~Section();

	bool IsBranch() const;

	void EvaluateText(const ScriptContext& context);

	std::tuple<std::string, std::string, std::string, std::string> GetSpeaking() const;

	std::vector<Option> GetOptions() const;

	float GetBackground() const;

	void SetOwnerScript(Script* script);

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

	// 后台对话的显示秒数，0表示前台对话
	float background = 0.f;

	// 所属脚本（引用，不持有所有权）
	Script* ownerScript;

};

// 对话：本体只由Milestone持有，别处用裸指针引用、绝不delete，见dialog.md
class Dialog {
public:
	Dialog();

	~Dialog();

	void AddDialog(std::string speaker, std::string content, std::string label, std::string voice, float background = 0.f);

	void AddDialog(std::vector<Option> options);

	std::vector<Section> GetDialogs() const;

	void SetCondition(std::string condition);

	const std::string& GetCondition() const;

private:
	// 对话段列表
	std::vector<Section> list;

	// 显示条件
	std::string condition;

};
