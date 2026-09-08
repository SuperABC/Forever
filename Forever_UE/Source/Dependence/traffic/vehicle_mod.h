#pragma once

#include <string>

// 阶段3骨架:VehicleMod只声明GetType()/GetName()两个纯虚接口,用于验证Mod发现/加载/
// 注册机制本身。真正的traffic域业务接口留到阶段4迁移traffic系统时才补上,完整21个
// concept x 8个domain对照表见 Source/Dependence/README.md。
class VehicleMod {
public:
	VehicleMod() = default;
	virtual ~VehicleMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// config.json里"vehicle_mods"数组中该mod id后面的命令行式参数字符串(如
	// "pengzhan --density 1.0"里的"--density 1.0"),由<Concept>Factory::Create<Concept>
	// 创建实例后立刻调用一次。默认空实现,不需要参数的mod不用重写,格式解析完全由重写者
	// 自己决定。
	virtual void ApplyArgs(const std::string& args) {}
};
