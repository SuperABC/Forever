#pragma once


// mod单元测试的print回调：裸函数指针，函数体在宿主(测试程序)一侧，跨DLL安全(不传std::string/std容器)。
typedef void (*TestPrintFn)(const char* line);

// 通用身份检查：type非空、name非空(不检查type/id的格式——mod id可以是任意字符串)。conceptName只用来拼输出信息。
// 每个mod都带这一条，深度测试各自覆盖在UnitTest里。
// 通过返回true；失败时用print输出原因并返回false。
bool CheckModIdentity(const char* conceptName, const char* type, const char* name, TestPrintFn print);

// 每个XxxMod基类声明了纯虚 virtual bool UnitTest(TestPrintFn print) = 0;。
// 没有深度测试的mod在类体里写这一行，得到通用身份检查；有深度测试的mod自己实现UnitTest。
#define FOREVER_MOD_GENERIC_UNIT_TEST(conceptName) \
	virtual bool UnitTest(TestPrintFn print) override { return CheckModIdentity(conceptName, GetType(), GetName(), print); }
