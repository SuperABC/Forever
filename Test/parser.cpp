#include "parser.h"

#include <algorithm>
#include <sstream>

using namespace std;

void Parser::AddOption(const string& name, char abbreviation,
	const string& help, bool assigned, string value) {
	ValidateName(name);
	
	// 如果给定简写字符，则构建与名称的映射
	if (abbreviation != '\0') {
		if (abbreviation == '-') {
			throw invalid_argument("Short option cannot be '-'.\n");
		}

		if (names.find(abbreviation) != names.end()) {
			throw invalid_argument(string("Short option '") + abbreviation + "' already used.\n");
		}

		names[abbreviation] = name;
	}

	// 记录完整option信息
	options[name] = { abbreviation, value != "", assigned, value, help };
}

vector<string> Parser::ExtractCmd(const string& input) {
	vector<string> tokens;
	string token;
	bool inQuotes = false;
	bool escapeNext = false;

	// 按空格分割
	for (char c : input) {
		if (escapeNext) {
			token += c;
			escapeNext = false;
			continue;
		}

		switch (c) {
		case '\\':
			escapeNext = true;
			break;
		case '"':
			inQuotes = !inQuotes;
			break;
		case ' ':
		case '\t':
			if (inQuotes) {
				token += c;
			}
			else if (!token.empty()) {
				tokens.push_back(token);
				token.clear();
			}
			break;
		default:
			token += c;
		}
	}
	if (!token.empty()) {
		tokens.push_back(token);
	}

	return tokens;
}

void Parser::ParseCmd(vector<string> cmd) {
	if (cmd.size() < 1) return;

	for (size_t i = 0; i < cmd.size(); ++i) {
		const string& arg = cmd[i];

		// --为结束符
		if (arg == "--") break;

		// 通过长名称赋值
		if (arg.substr(0, 2) == "--") {
			string optionName;
			string value;
			size_t pos = arg.find('=');

			// 若存在无空格等号，则等号视为赋值
			if (pos != string::npos) {
				optionName = arg.substr(0, pos);
				value = arg.substr(pos + 1);
			}
			else {
				optionName = arg;
			}

			auto it = options.find(optionName);
			if (it == options.end()) {
				throw runtime_error("Unknown option: " + optionName + "\n");
			}

			Option& opt = it->second;
			opt.present = true;

			// 若长名称对应option需要赋值，如果存在无空格等号则取等号后子串，否则取下一个空格后的内容
			if (opt.assigned) {
				if (pos != string::npos) {
					opt.value = value;
				}
				else {
					if (i + 1 >= cmd.size() || cmd[i + 1][0] == '-') {
						throw runtime_error("Option " + optionName + " requires a value.\n");
					}
					opt.value = cmd[++i];
				}
			}
		}
		// 通过字符赋值
		else if (arg[0] == '-') {
			for (size_t j = 1; j < arg.size(); ++j) {
				char abbreviation = arg[j];
				auto it = names.find(abbreviation);
				if (it == names.end()) {
					throw runtime_error(string("Unknown option: -") + abbreviation + "\n");
				}

				const string& optionName = it->second;
				Option& opt = options[optionName];
				opt.present = true;

				//若字符对应option需要赋值，如果其后紧跟字符串则取其后子串，否则取下一个空格后的内容
				if (opt.assigned) {
					if (j + 1 < arg.size() || i + 1 < cmd.size()) {
						if (j + 1 < arg.size()) {
							opt.value = arg.substr(j + 1);
						}
						else {
							opt.value = cmd[++i];
						}
						break;
					}
					else {
						throw runtime_error("Option -" + string(1, abbreviation) + " requires a value.\n");
					}
				}
			}
		}
	}
}

bool Parser::HasOption(const string& name) const {
	auto it = options.find(name);
	if (it == options.end()) {
		throw invalid_argument("Unknown option: " + name + "\n");
	}
	return it->second.present;
}

const string& Parser::GetOption(const string& name) const {
	auto it = options.find(name);
	if (it == options.end()) {
		throw invalid_argument("Unknown option: " + name + "\n");
	}
	if (!it->second.assigned) {
		throw logic_error("Option " + name + " does not require a value.\n");
	}
	if (!it->second.present) {
		throw logic_error("Option " + name + " not present.\n");
	}
	return it->second.value;
}

CMD_TYPE Parser::GetAction(const string& action) const {
	if (action == "init") {
		return CMD_INIT;
	}
	if (action == "pass") {
		return CMD_PASS;
	}
	if (action == "event") {
		return CMD_EVENT;
	}
	if (action == "print") {
		return CMD_PRINT;
	}
	if (action == "exit") {
		return CMD_EXIT;
	}
	return CMD_UNKOWN;
}

void Parser::PrintHelp(CMD_TYPE type) const {
	// 自适应帮助信息
	cout << "Usage: init/pass/print [OPTIONS] [ARGS...]\n\n";
	cout << "Options:\n";

	// 输出当前已添加的参数信息
	for (const auto& pair : options) {
		const Option& opt = pair.second;
		cout << "  ";

		if (opt.abbreviation != '\0') {
			cout << '-' << opt.abbreviation << ", ";
		}
		else {
			cout << "    ";
		}

		cout << pair.first;

		if (opt.assigned) {
			cout << "=VALUE";
		}

		cout << "\n    " << opt.help << "\n\n";
	}
}

void Parser::ValidateName(const string& name) const {
	if (name.empty()) {
		throw invalid_argument("Option name cannot be empty.\n");
	}

	if (name.size() < 2 || name[0] != '-' || name[1] != '-') {
		throw invalid_argument("Long option name must start with '--'.\n");
	}

	if (name.find('=') != string::npos) {
		throw invalid_argument("Option name cannot contain '='.\n");
	}
}

