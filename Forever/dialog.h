#pragma once

#include "condition.h"

#include <vector>
#include <string>


class Dialog {
public:
	Dialog();
	~Dialog();

	// 添加/获取对话列表
	void AddDialog(std::string speaker, std::string content);
	std::vector<std::pair<std::string, std::string>> GetDialogs();

	// 设置/获取控制条件
	void SetCondition(Condition condition);
	Condition& GetCondition();

private:
	std::vector<std::pair<std::string, std::string>> list;

	Condition condition;
};