#pragma once

#include <string>
#include <vector>


// 当前exe的绝对路径。
std::string SelfPath();

// 以同一个exe启动子进程(args不含exe本身)，stdout/stderr重定向到logPath，等待结束。
// 返回子进程退出码；启动失败返回-1，超时(杀掉子进程)返回-2。
int RunSelf(const std::vector<std::string>& args, const std::string& logPath, int timeoutSeconds);

std::string ReadWholeFile(const std::string& path);

// 崩溃时(访问违例等)把异常码和调用栈(模块名+RVA)打到stderr：exe静态链接了Core/Dependence，
// 对着链接生成的CoreTest.map能直接查到函数。
void InstallCrashHandler();
