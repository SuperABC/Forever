#pragma once

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>

enum CMD_TYPE {
	CMD_UNKOWN, //未知命令
	CMD_INIT, //全局初始化
	CMD_PASS, //时钟前进
	CMD_EVENT, //触发事件
	CMD_LOOKUP, //查找对象
	CMD_PRINT, //状态输出
	CMD_EXIT //退出程序
};

class Parser {
public:
	// 添加参数选项
	void AddOption(const std::string& name, char abbreviation = '\0',
		const std::string& help = "", bool assigned = false, std::string value = "");

	// 从文本提取命令数组
	std::vector<std::string> ExtractCmd(const std::string& input);

	// 解析命令行参数
	void ParseCmd(std::vector<std::string> cmd);

	// 检查选项是否存在
	bool HasOption(const std::string& name) const;

	// 获取选项值
	const std::string& GetOption(const std::string& name) const;

	// 获取命令
	CMD_TYPE GetAction(const std::string& action) const;

	// 打印帮助信息
	void PrintHelp(CMD_TYPE type) const;

private:
	struct Option {
		char abbreviation = 0;
		bool present = false;
		bool assigned = false;
		std::string value;
		std::string help;
	};

	// --xxx, option
	std::map<std::string, Option> options;
	// -x, --xxx
	std::map<char, std::string> names;

	// 检查--xxx字符串
	void ValidateName(const std::string& name) const;
};


