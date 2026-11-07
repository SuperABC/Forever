#pragma once

#include "common/mod_state.h"

#include <string>
#include <vector>

// 设计见vehicle_mod.md。
class VehicleMod {
public:
	VehicleMod() = default;
	virtual ~VehicleMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 子类在这里填下面的字段(不在构造函数里),Core创建实例后立刻调一次。
	virtual void SetProperty() {}

	// 继承AVehicleElement的蓝图类资源路径(Dependence层不能持有UClass),为什么是蓝图类路径见vehicle_mod.md。
	std::string blueprintPath;

	// Script配置,驱动"靠近车辆弹出上车选项",见vehicle_mod.md。
	std::string scriptModName; // 空=无自定义ScriptMod；和milestoneNames都空则实例没有Script(GetScript()==nullptr)，见MakeScript
	std::vector<std::string> milestoneNames;

	// 下车点:车身局部坐标偏移(UE单位,X前/Y右/Z上),默认车身左侧;exitOffsetZ原因见vehicle_mod.md。
	float exitOffsetX = 0.f;
	float exitOffsetY = -200.f;
	float exitOffsetZ = 100.f;

	// 车型分类不是实例字段:子类提供static const char* GetCategory(),经RegisterVehicle注册,见vehicle_mod.md。

	// 操控/搭乘:公共交通车辆是纯展示物,真正的搭乘交互未实现。
	bool drivable = false;
	bool boardable = false;
	int capacity = 0;

	// 立方体占位尺寸(UE单位),transitMeshPath为空时ATransitVehicleElement按它缩放Cube。
	float sizeX = 100.f;
	float sizeY = 100.f;
	float sizeZ = 100.f;

	// 公交/火车/飞机类载具的真实静态网格路径(小汽车不用,走blueprintPath),非空时不再用立方体占位。
	std::string transitMeshPath;

	// 只修正transitMeshPath加载出的mesh:缩放和朝向修正(yawOffsetDegrees叠加在模型局部旋转上,Actor朝向仍由行驶方向决定)。
	struct VehicleMeshTransform {
		float scale = 1.f;            // 统一缩放系数(相对模型原始大小，1=不缩放)
		float yawOffsetDegrees = 0.f; // 模型正前方相对行驶方向的夹角修正，角度制
	};
	VehicleMeshTransform meshTransform;

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
