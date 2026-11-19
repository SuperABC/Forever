#pragma once

#include <string>


// 解析山/海/河地形mod参数里的"--distribute=false"(也认"--distribute false"、0/no/off)：
// 为false时mod跳过DistributeTerrain，只提供贴图。没写或其它值都是true。详见terrain_args.md。
inline bool ParseDistributeArg(const std::string& args) {
	size_t position = args.find("--distribute");
	if (position == std::string::npos) return true;
	position += 12;
	while (position < args.size() && (args[position] == '=' || args[position] == ' ' || args[position] == '\t')) position++;
	size_t end = position;
	while (end < args.size() && args[end] != ' ' && args[end] != '\t') end++;
	std::string value = args.substr(position, end - position);
	for (char& c : value) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
	return !(value == "false" || value == "0" || value == "no" || value == "off");
}
