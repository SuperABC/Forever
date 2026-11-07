#pragma once

#include "map/general_geometry.h"

#include <string>
#include <vector>


// map域各种feature(门、植被、路灯、自定义模型)的纯数据描述,mod填写、Core读取,详见feature_spec.md。

// ---------------------------------------------------------------- 门

// 一扇门的外观/交互/开门动画描述,mesh为空=只是门洞不放门,见feature_spec.md。
struct DoorSpec {
	// ---- 外观 ----
	std::string mesh;                         // 门扇资产软路径；空=只是门洞，不放门
	std::string frameMesh;                    // 可选门框资产(静止不动)，空=不放门框

	// ---- 交互 ----
	std::string name;                         // 门名；非空才创建Script，见door.h"Script"一节
	std::string scriptModName = "script_empty";      // 仅name非空时使用
	std::vector<std::string> milestoneNames;  // 仅name非空时使用

	// ---- 门禁 ----
	int access = 0;                           // 0=Open 1=Owner 2=Locked，默认Open

	// ---- 开门动画参数(全部由Forever层代码驱动，资产不带动画) ----
	int style = 0;                            // 0=Slide推拉 1=Swing平开
	int leaves = 1;                           // 1=单扇 2=双扇对开(第二扇由代码镜像生成)
	float openAmount = 0.f;                   // Slide:滑出距离占扇宽比例(0=默认1)；
	                                           // Swing:开门角度，度(0=默认90)
	float openSeconds = 0.5f;                 // 开/关一次的时长，秒

	// 单扇门(leaves==1)是否在创建时随机一次朝向(只是开关,结果存在Door实例上),见feature_spec.md。
	bool randomSide = false;
};

// ---------------------------------------------------------------- 植被

// 植被请求:mod只描述位置/数量/mesh/基础缩放,展开由Core的Vegetation统一做;
// 坐标参考系由宿主决定,详见feature_spec.md"植被"一节。

// 单棵树：种在position处。
struct TreeSingleSpec {
	PointParams position{};
	float scale = 1.f;
	std::string meshPath;
};

// 一排树：沿from->to均匀种num棵(含两端，num==1时种在中点)。
struct TreeLineSpec {
	PointParams from{};
	PointParams to{};
	int num = 0;
	float scale = 1.f;
	std::string meshPath;
};

// 一片树：from/to是一个矩形的两个对角，在矩形内均匀随机种num棵。矩形沿宿主参考系的轴向，
// 宿主旋转时跟着一起转。
struct TreeAreaSpec {
	PointParams from{};
	PointParams to{};
	int num = 0;
	float scale = 1.f;
	std::string meshPath;
};

// 三种请求的统一存储,RoadnetMod/ZoneMod/BuildingMod各持有一份(成员trees)。num<=0或meshPath为空的请求直接忽略。
struct TreeRequests {
	std::vector<TreeSingleSpec> singles;
	std::vector<TreeLineSpec> lines;
	std::vector<TreeAreaSpec> areas;

	void AddSingle(const PointParams& position, float scale, const std::string& meshPath);
	void AddLine(const PointParams& from, const PointParams& to, int num, float scale, const std::string& meshPath);
	void AddArea(const PointParams& from, const PointParams& to, int num, float scale, const std::string& meshPath);
};

// ---------------------------------------------------------------- 路灯

// 路灯请求,和植被同一套模式,见feature_spec.md"路灯"一节。
// rotationDegrees是宿主局部系下的朝向(度,数学角:0=+X、逆时针)。

// 单盏灯：放在position处。
struct LampSingleSpec {
	PointParams position{};
	float rotationDegrees = 0.f;
	std::string blueprintPath;
};

// 一排灯：沿from->to均匀放num盏(含两端，num==1时放在中点)，朝向相同。
struct LampLineSpec {
	PointParams from{};
	PointParams to{};
	int num = 0;
	float rotationDegrees = 0.f;
	std::string blueprintPath;
};

// 一片灯：from/to是矩形的两个对角，在矩形内均匀随机放num盏，朝向相同。
struct LampAreaSpec {
	PointParams from{};
	PointParams to{};
	int num = 0;
	float rotationDegrees = 0.f;
	std::string blueprintPath;
};

// 三种请求的统一存储,宿主mod持有一份(成员lamps)。num<=0或blueprintPath为空的请求直接忽略。
struct LampRequests {
	std::vector<LampSingleSpec> singles;
	std::vector<LampLineSpec> lines;
	std::vector<LampAreaSpec> areas;

	void AddSingle(const PointParams& position, float rotationDegrees, const std::string& blueprintPath);
	void AddLine(const PointParams& from, const PointParams& to, int num, float rotationDegrees, const std::string& blueprintPath);
	void AddArea(const PointParams& from, const PointParams& to, int num, float rotationDegrees, const std::string& blueprintPath);
};

// ---------------------------------------------------------------- 建筑自定义模型

// 建筑mod放置的一个StaticMesh(BuildingMod::AddDecoration记录),位置/yaw按face=2定义,
// 详见feature_spec.md"建筑自定义模型"一节。
struct DecorationSpec {
	std::string meshPath;                        // StaticMesh资产软路径
	PointParams position{ 0.5f, 0.f, 0.5f, 0.f }; // 楼体局部(xRatio,xOffset,yRatio,yOffset)，和.layout模板PointParams同一套约定，默认楼体中心
	float z = 0.f;                               // 离地高度(地图单位)
	float scaleX = 1.f, scaleY = 1.f, scaleZ = 1.f; // 资产原尺寸的倍数(不做包围盒拟合)
	float yaw = 0.f, pitch = 0.f, roll = 0.f;    // 度；yaw按face=2定义，会叠加朝向和建筑旋转
};

// DecorationSpec换算后的世界摆位(Core算好,UE层只读):x/y/z地图单位,yaw弧度(已叠加朝向和宿主旋转),
// pitch/roll度。建筑和园区共用。
struct DecorationInstance {
	std::string meshPath;
	float x = 0.f, y = 0.f, z = 0.f;
	float yaw = 0.f;
	float pitch = 0.f, roll = 0.f;
	float scaleX = 1.f, scaleY = 1.f, scaleZ = 1.f;
};
