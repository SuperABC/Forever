#pragma once

#include <string>
#include <memory>
#include <set>
#include <mutex>
#include <codecvt>
#include <locale>
#include <exception>

#include <windows.h>
#include <strsafe.h>

#define REPLACE_PATH(path) (path)

#ifdef _DEBUG
#define MOD_TEST
#endif // DEBUG


// 调试控制台输出
void debugf(LPCSTR format, ...);

// 获取[0, range - 1]的随机整数
int GetRandom(int range);
