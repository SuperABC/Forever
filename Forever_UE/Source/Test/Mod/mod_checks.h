#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


// ModTest对单个mod的检查，只依赖Dependence(不碰Core/UE)，设计见ModTest.md。

// 一个mod跑完的结果。
struct ModOutcome {
	bool created = false;
	bool passed = false;
	bool nameUnique = true;
	unsigned long long unscoped = 0;
	std::string error;
	std::vector<std::string> problems; // 按concept的数值/字段合法性检查(宿主一侧，不改mod)
	// 这个mod在字段里写到的别的mod的id：(哪个字段, 被引用的id)。检查函数只管往里记，RunMod最后统一核对。
	std::vector<std::pair<std::string, std::string>> references;
	// 引用核对的结果：被引用的id没有任何加载进来的dll提供。
	std::vector<std::string> referenceProblems;
};

// 检查一个mod时需要的环境。
struct ModCheckContext {
	std::string modId;
	// 占位模板(Forever_Mod/Empty，按提供它的dll识别)：名字固定、字段留空，个别检查对它放宽。
	bool isTemplate = false;
	// 这个id有没有被加载进来的任何一个dll提供(被测的+--with的)。字段里引用别的mod时用它校验。
	std::function<bool(const std::string&)> isKnownId;
};

// 一个concept在ModTest里的全部操作：ModTestHost按Dependence的23个Factory各生成一份。
struct ModConceptOps {
	std::string conceptKey;
	std::function<void(const std::unordered_map<std::string, std::string>&)> setArgs;
	// GetProcAddress拿到的Register/Finish导出函数(可空)，在这里转成对应Factory类型的函数指针并调用。
	std::function<void(void* registerSymbol, void* finishSymbol)> registerDll;
	std::function<bool(const std::string&)> isRegistered;
	// 创建实例 -> 在mod自己的随机流里跑UnitTest -> 字段检查(顺带收集引用) -> 核对引用 -> 再建一个实例比名字 -> 销毁。
	std::function<void(const ModCheckContext&, ModOutcome&)> run;
};

// 持有23个Factory(和游戏里Registry持有的是同一批类，只是这里是ModTest自己的实例)。
class ModTestHost {
public:
	ModTestHost();
	~ModTestHost();
	ModTestHost(const ModTestHost&) = delete;
	ModTestHost& operator=(const ModTestHost&) = delete;

	const std::vector<ModConceptOps>& GetOps() const;
	const ModConceptOps* FindOps(const std::string& conceptKey) const;

private:
	struct Factories;
	Factories* factories;
	std::vector<ModConceptOps> ops;
};
