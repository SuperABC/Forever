#pragma once

#include <string>
#include <vector>
#include <utility>


// mod参数串的统一解析器。config.json里 "<concept>_mods" 的条目写成 "id --key value --flag --key=value"，
// 工厂把"id"之后的参数串原样交给mod的creator；mod在构造函数里 `ModArgs args(argString);` 然后按key取值。
// 语法、取值规则、为什么不会越界传递见mod_args.md。
class ModArgs {
public:
	ModArgs() = default;
	explicit ModArgs(const std::string& args);

	// key不带"--"。同一个key出现多次取最后一次。
	bool Has(const std::string& key) const;

	// 取不到(没写)返回def。写了但没有值("--flag")：GetString返回""，GetBool返回true，数字类返回def。
	std::string GetString(const std::string& key, const std::string& def = "") const;
	// 数字必须整个值都能解析，否则当没写(返回def)；带min/max的重载会把结果夹到[min,max]。
	int GetInt(const std::string& key, int def) const;
	int GetInt(const std::string& key, int def, int minValue, int maxValue) const;
	float GetFloat(const std::string& key, float def) const;
	float GetFloat(const std::string& key, float def, float minValue, float maxValue) const;
	double GetDouble(const std::string& key, double def) const;
	// false/0/no/off(大小写不敏感)是false；没有值或其它值是true；没写返回def。
	bool GetBool(const std::string& key, bool def = false) const;

	// 没挂在任何--key后面的散装参数(出现在第一个--key之前的部分)，按空白切分。
	const std::vector<std::string>& Positionals() const { return positionals; }
	// 出现过的key(按首次出现顺序，不带"--")。
	std::vector<std::string> Keys() const;

	// 返回args里去掉"--key ..."(含它的值)之后的参数串，其余部分按单个空格重新拼接；
	// found/value给出被去掉的那一项的值(没找到则found=false，返回原串不动)。Core用它从参数串里剥离--seed。
	static std::string Remove(const std::string& args, const std::string& key, bool& found, std::string& value);

private:
	struct Option {
		std::string key;
		std::string value;
		bool hasValue = false;
	};
	std::vector<Option> options;
	std::vector<std::string> positionals;

	const Option* Find(const std::string& key) const;
};
