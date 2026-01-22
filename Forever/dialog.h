#pragma once

#include "condition.h"
#include "change.h"

#include <vector>
#include <string>


class Dialog;

class Option {
public:
	Option(Condition condition, std::string option, std::vector<Dialog> dialogs,
		std::vector<std::shared_ptr<Change>> changes);
	~Option();

	Condition& GetCondition();
	std::string GetOption() const;
	std::vector<Dialog> GetDialogs() const;
	std::vector<std::shared_ptr<Change>> GetChanges() const;

private:
	Condition condition;
	std::string option;
	std::vector<Dialog> dialogs;
	std::vector<std::shared_ptr<Change>> changes;
};

class Section {
public:
	Section(std::string speaker, std::string content);
	Section(std::vector<Option> options);
	~Section();

	bool IsBranch() const;
	std::pair<std::string, std::string> GetSpeaking() const;
	std::vector<Option> GetOptions() const;

private:
	bool branch;

	std::pair<std::string, std::string> speaking;
	std::vector<Option> options;
};

class Dialog {
public:
	Dialog();
	~Dialog();

	// 添加/获取对话列表
	void AddDialog(std::string speaker, std::string content);
	void AddDialog(std::vector<Option> options);
	std::vector<Section> GetDialogs();

	// 设置/获取控制条件
	void SetCondition(Condition condition);
	Condition& GetCondition();

private:
	std::vector<Section> list;

	Condition condition;
};