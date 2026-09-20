#pragma once

#include <string>


// 阶段3骨架:SchedulerMod只声明GetType()/GetName()两个纯虚接口,用于验证Mod发现/加载/
// 注册机制本身。真正的populace域业务接口留到阶段4迁移populace系统时才补上,完整21个
// concept x 8个domain对照表见 Source/Dependence/README.md。
class SchedulerMod {
public:
	SchedulerMod() = default;
	virtual ~SchedulerMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;
};
