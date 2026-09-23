#pragma once

#include <string>

// VehicleMod：一种具体车型的行为定义(比如VehicleBasic这个测试车型)。GetType()/GetName()
// 两个纯虚接口是Mod发现/加载/注册机制，完整21个concept x 8个domain对照表见
// Source/Dependence/README.md。
class VehicleMod {
public:
	VehicleMod() = default;
	virtual ~VehicleMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 指向一个继承AVehicleElement的蓝图(Blueprint)类的资源路径，具体子类构造函数里
	// 赋值——和JobMod::scriptModName同一个"mod自己的字段，Core只读转发"模式(见
	// job_mod.h)。Dependence/Basic层是纯C++、不依赖UE头文件，不能直接持有UClass指针，
	// 只能存一份路径字符串。
	//
	// 为什么是"蓝图类路径"而不是"骨骼网格资源路径"：UChaosWheeledVehicleMovementComponent
	// 要求骨骼网格必须在Actor构造函数阶段就绑定好(见VehicleElement.md"骨骼网格必须在
	// 组件注册之前就绑好"一节)，但SpawnActor<T>()不支持给构造函数传自定义参数——"这个
	// Actor该长什么样"这件事必须靠"用哪个UClass去生成"这个选择在SpawnActor之前就定下
	// 来，不能等生成之后再传数据。AVehicleElement因此只是一个提供驾驶/相机/输入公共
	// 逻辑的C++基类，具体每种车的骨骼网格/四个轮子的骨骼名由继承它的蓝图子类在编辑器
	// Details面板里设置(蓝图的这些属性覆盖在保存蓝图资产时就烘焙进了蓝图类自己的CDO，
	// 早于任何运行时SpawnActor调用，天然满足"构造函数阶段就定好外观"这个要求)。
	//
	// UForeverTrafficFrameworkComponent::ToggleVehicle生成车辆前用这个路径
	// LoadClass<AVehicleElement>()拿到具体该用哪个蓝图类，再拿这个类去SpawnActor。
	// 新增一种车型：做一个继承AVehicleElement的新蓝图(换骨骼网格资产、改轮子骨骼名)+
	// 在这里(或将来别的mod dll)注册一个新的VehicleMod子类指向这个蓝图路径，不需要碰
	// Forever这个UE模块的代码，见VehicleElement.md。
	std::string blueprintPath;
};
