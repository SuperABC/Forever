#pragma once

#include <filesystem>
#include <string>


// 文件路径 <-> UTF-8字符串。Windows上std::filesystem::path::string()给的是系统代码页(中文系统是GBK)，
// 而脚本、mod、UE都按UTF-8传文件名，所以凡是要和它们对上的路径(资源文件名索引、mod读图片)都走这两个函数。
// C++17(Core/Dependence/mod)和C++20(UE模块)都能用。
inline std::string PathToUtf8(const std::filesystem::path& path) {
#if defined(__cpp_char8_t)
	const std::u8string text = path.u8string();
	return std::string(text.begin(), text.end());
#else
	return path.u8string();
#endif
}

inline std::filesystem::path Utf8ToPath(const std::string& text) {
#if defined(__cpp_char8_t)
	return std::filesystem::path(std::u8string(text.begin(), text.end()));
#else
	return std::filesystem::u8path(text);
#endif
}
