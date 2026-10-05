#pragma once

#include <string>
#include <vector>

// VehicleMod：一种具体车型的行为定义(比如VehicleBasic这个测试车型)。GetType()/GetName()
// 两个纯虚接口是Mod发现/加载/注册机制，完整21个concept x 8个domain对照表见
// Source/Dependence/README.md。
class VehicleMod {
public:
	VehicleMod() = default;
	virtual ~VehicleMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 具体子类必须在这里填下面这一整组字段，不要在构造函数里赋值——构造函数只负责
	// id/count这类登记，和StorageMod::SetProperty()同一套"两段式"约定。Core创建完mod
	// 实例后会立刻调一次这个方法，再读这些字段。
	virtual void SetProperty() {}

	// 指向一个继承AVehicleElement的蓝图(Blueprint)类的资源路径——和JobMod::
	// scriptModName同一个"mod自己的字段，Core只读转发"模式(见job_mod.h)。
	// Dependence/Basic层是纯C++、不依赖UE头文件，不能直接持有UClass指针，只能存一份
	// 路径字符串。
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

	// Script配置——和JobMod/SchedulerMod同一个"mod自己声明意图，Core只读"模式：两个普通
	// 成员字段，不通过返回值传递。scriptModName默认"empty"，milestoneNames默认为空，
	// 每项是不含路径/扩展名的bare文件名，Core通过
	// Config::GetScriptPath()解析实际路径，见job_mod.h"Script配置"一节。这次用来驱动
	// "靠近车辆弹出'上车'选项"这个效果——车辆自己的milestone脚本在game_start时调
	// add_option，选中后调enter_vehicle，见Resource/Story/vehicle_basic.script。
	std::string scriptModName = "empty";
	std::vector<std::string> milestoneNames;

	// 下车点——车身局部坐标系下的偏移(UE单位，X前进方向/Y右侧方向/Z竖直方向)，默认车身
	// 左侧(Y负方向)。下车前会用这个算出世界坐标、检测站不站得下人，站不下就拒绝下车，见
	// ForeverTrafficFrameworkComponent::ExitVehicle。
	//
	// exitOffsetZ默认100而不是0：车身的Actor原点在车轮接地点附近(贴地)，但碰撞检测用的
	// 胶囊体是"以给定点为中心"(FCollisionShape::MakeCapsule(42,96)，半高96)，如果直接
	// 用车身原点(贴地高度)当胶囊体中心，胶囊体下半截(96个单位)会整个扎进地板里，重叠检测
	// 不管旁边空不空都会报"被挡住"——照抄老工程下车逻辑"Z轴抬高100"的同一个理由(见
	// 原ToggleVehicle DISMOUNT分支的注释：胶囊体中心在脚底往上，不是贴地)。
	float exitOffsetX = 0.f;
	float exitOffsetY = -200.f;
	float exitOffsetZ = 100.f;

	// 车型分类——"car"是现在唯一可以上下车/驾驶的类型(VehicleBasic)，"bus"/"train"/"plane"
	// 是这次新增的公共交通线路车辆。Traffic::Init()的停车位分支只挑category=="car"的车型
	// (公交/火车/飞机不停在停车位里，由Route生成、沿线路运行)，见Core/traffic/traffic.md。
	std::string category = "car";

	// 操控/搭乘——这次公共交通车辆都是运动学地沿线路移动的纯展示物，不接受操控也不能上去，
	// 先声明这两个标志、capacity留着给字段，真正的搭乘/多人座位交互以后再实现(见
	// public_transport_plan.md"5. 新载具这次不做操控/搭乘，但预留接口")。
	bool drivable = false;
	bool boardable = false;
	int capacity = 0;

	// 立方体占位尺寸(UE单位)——transitMeshPath为空时，ATransitVehicleElement按这个缩放
	// /Engine/BasicShapes/Cube做外观，见Element/TransitVehicleElement.md。
	float sizeX = 100.f;
	float sizeY = 100.f;
	float sizeZ = 100.f;

	// ATransitVehicleElement(公交/火车/飞机这类"立方体占位"载具)专用的真实静态网格包路径，
	// 不是给VehicleBasic(小汽车，走blueprintPath+完整骨骼网格那条路)用的——两个字段服务两种
	// 完全不同的外观机制，不要混用。非空时ATransitVehicleElement::Init()直接LoadObject这个
	// 路径当外观，不再按sizeX/Y/Z缩放立方体(模型自己的尺寸就是对的)；留空(Bus/Train目前都是
	// 空)继续走立方体占位，见public_transport_plan.md"载具外观：立方体"一节和
	// PACKAGING_PLAN"阶段E"。资产来自一个独立mod(Forever_Mod/<ModName>/UE/<ModName>/
	// Plugins/<ModName>/Content/...)的Plugin，运行时由ForeverModSubsystem挂载成
	// "/<ModName>/..."包路径，见config.md"GetPlugins/GetPakFiles"一节。
	std::string transitMeshPath;

	// transitMeshPath指向的真实静态网格资产未必按UE"X轴朝前"的约定制作，也未必正好是
	// sizeX/Y/Z这个占位尺寸——这组字段专门修正transitMeshPath这条路径加载出来的mesh本身
	// 的显示效果(缩放+朝向修正)，只在transitMeshPath非空时使用，不影响立方体占位那条路径
	// (sizeX/Y/Z只在立方体占位时使用)。ATransitVehicleElement::Tick()每帧仍然按行驶方向
	// 设Actor自身的朝向(来自Route::Update()算好的yaw)，yawOffsetDegrees是叠加在模型本身
	// (bodyMesh的局部旋转)上的一次性修正，解决"模型自己的正前方"和"Actor正前方(行驶方向)"
	// 没对齐的问题——两者各管一段，不会互相覆盖。
	struct VehicleMeshTransform {
		float scale = 1.f;            // 统一缩放系数(相对模型原始大小，1=不缩放)
		float yawOffsetDegrees = 0.f; // 模型正前方相对行驶方向的夹角修正，角度制
	};
	VehicleMeshTransform meshTransform;
};
