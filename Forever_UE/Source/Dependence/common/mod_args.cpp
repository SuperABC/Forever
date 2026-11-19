#include "mod_args.h"

#include <cstdlib>
#include <cctype>
#include <cmath>
#include <algorithm>

using namespace std;

namespace {
	struct Token {
		string text; // 去掉引号后的内容
		bool quoted = false; // token以引号开头："--x"这样整个包起来的不算key
		size_t begin = 0, end = 0; // 在原串里的范围(Remove原样保留其余参数用)
	};

	// 按空白切分；"..."或'...'里的空白不切(引号本身去掉)，可以出现在token中间(--path="C:/a b/x")。
	vector<Token> Tokenize(const string& args) {
		vector<Token> tokens;
		size_t i = 0;
		while (i < args.size()) {
			while (i < args.size() && isspace(static_cast<unsigned char>(args[i]))) i++;
			if (i >= args.size()) break;
			Token token;
			token.begin = i;
			while (i < args.size() && !isspace(static_cast<unsigned char>(args[i]))) {
				if (args[i] == '"' || args[i] == '\'') {
					char quote = args[i++];
					if (i - 1 == token.begin) token.quoted = true; // 只有token以引号开头才算"被引号包着"：--path="a b"仍是option
					while (i < args.size() && args[i] != quote) token.text += args[i++];
					if (i < args.size()) i++; // 吃掉收尾的引号(没收尾就读到串尾)
				} else {
					token.text += args[i++];
				}
			}
			token.end = i;
			tokens.push_back(move(token));
		}
		return tokens;
	}

	// "--key"开头(至少一个字符的key)且没被引号包过的token才是option；单个"-"、"--"、"-5"都是普通值。
	bool IsOptionToken(const Token& token) {
		return !token.quoted && token.text.size() > 2 && token.text[0] == '-' && token.text[1] == '-';
	}

	string Lower(string text) {
		for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
		return text;
	}
}

ModArgs::ModArgs(const string& args) {
	vector<Token> tokens = Tokenize(args);
	size_t i = 0;
	while (i < tokens.size() && !IsOptionToken(tokens[i])) positionals.push_back(tokens[i++].text);
	while (i < tokens.size()) {
		Option option;
		string body = tokens[i].text.substr(2);
		size_t equal = body.find('=');
		i++;
		if (equal != string::npos) {
			option.key = body.substr(0, equal);
			option.value = body.substr(equal + 1);
			option.hasValue = true;
		} else {
			option.key = body;
			// 值可以有多个token(没引号的路径里有空格)：一直吃到下一个--key，用单个空格拼起来。
			while (i < tokens.size() && !IsOptionToken(tokens[i])) {
				if (option.hasValue) option.value += ' ';
				option.value += tokens[i].text;
				option.hasValue = true;
				i++;
			}
		}
		if (!option.key.empty()) options.push_back(move(option));
	}
}

const ModArgs::Option* ModArgs::Find(const string& key) const {
	for (auto it = options.rbegin(); it != options.rend(); ++it) {
		if (it->key == key) return &*it;
	}
	return nullptr;
}

bool ModArgs::Has(const string& key) const {
	return Find(key) != nullptr;
}

string ModArgs::GetString(const string& key, const string& def) const {
	const Option* option = Find(key);
	return option ? option->value : def;
}

int ModArgs::GetInt(const string& key, int def) const {
	const Option* option = Find(key);
	if (!option || !option->hasValue) return def;
	const char* begin = option->value.c_str();
	char* end = nullptr;
	long value = strtol(begin, &end, 10);
	if (end == begin || *end != '\0') return def;
	return static_cast<int>(value);
}

int ModArgs::GetInt(const string& key, int def, int minValue, int maxValue) const {
	return max(minValue, min(maxValue, GetInt(key, def)));
}

double ModArgs::GetDouble(const string& key, double def) const {
	const Option* option = Find(key);
	if (!option || !option->hasValue) return def;
	const char* begin = option->value.c_str();
	char* end = nullptr;
	double value = strtod(begin, &end);
	if (end == begin || *end != '\0' || !isfinite(value)) return def;
	return value;
}

float ModArgs::GetFloat(const string& key, float def) const {
	return static_cast<float>(GetDouble(key, def));
}

float ModArgs::GetFloat(const string& key, float def, float minValue, float maxValue) const {
	return max(minValue, min(maxValue, GetFloat(key, def)));
}

bool ModArgs::GetBool(const string& key, bool def) const {
	const Option* option = Find(key);
	if (!option) return def;
	if (!option->hasValue) return true;
	string value = Lower(option->value);
	return !(value == "false" || value == "0" || value == "no" || value == "off");
}

vector<string> ModArgs::Keys() const {
	vector<string> keys;
	for (const Option& option : options) {
		if (find(keys.begin(), keys.end(), option.key) == keys.end()) keys.push_back(option.key);
	}
	return keys;
}

string ModArgs::Remove(const string& args, const string& key, bool& found, string& value) {
	found = false;
	value.clear();
	vector<Token> tokens = Tokenize(args);
	string rest;
	size_t i = 0;
	auto append = [&rest, &args](const Token& token) {
		if (!rest.empty()) rest += ' ';
		rest += args.substr(token.begin, token.end - token.begin); // 原样(含引号)
	};
	while (i < tokens.size()) {
		const Token& token = tokens[i];
		bool match = false;
		if (IsOptionToken(token)) {
			string body = token.text.substr(2);
			size_t equal = body.find('=');
			string name = equal == string::npos ? body : body.substr(0, equal);
			if (name == key) {
				match = true;
				found = true;
				value.clear();
				i++;
				if (equal != string::npos) value = body.substr(equal + 1);
				else {
					bool first = true;
					while (i < tokens.size() && !IsOptionToken(tokens[i])) {
						if (!first) value += ' ';
						value += tokens[i].text;
						first = false;
						i++;
					}
				}
			}
		}
		if (!match) { append(token); i++; }
	}
	return found ? rest : args;
}
