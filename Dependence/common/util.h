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

#define MOD_TEST
#define REPLACE_PATH(path) (path)


// 调试控制台输出
void debugf(LPCSTR format, ...);
