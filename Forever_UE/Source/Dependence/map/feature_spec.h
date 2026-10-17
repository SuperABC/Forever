#pragma once

#include "map/general_geometry.h"

#include <string>
#include <vector>


// Dependence/map里各种"feature"(门、植被……)的纯数据描述统一放在这一个文件里——mod填写、Core
// 读取，不含任何UE类型，资产都只是待LoadObject的软路径字符串。以后map域再有新的spec也往这里
// 加，不再一个spec一个文件。详见feature_spec.md。

// ---------------------------------------------------------------- 门

// DoorSpec：一扇门"长什么样、怎么开"的纯数据描述——供BuildingMod/RoomMod/ZoneMod共用，
// 填在各自的doorSpecs表/gate.door里，Core按tag查出对应的DoorSpec决定要不要真正放一扇门
// (ResolveDoorSpec，见door.h)。本身不含任何UE类型，mesh只是一个待LoadObject的软路径字符串。
//
// mesh为空="只是门洞，不放门"——这是这次门系统的核心设计：layout里没有标签的门洞不可选中，
// 永远只是个洞；有标签但查不到对应DoorSpec、或者查到了但mesh为空，同样只是个洞；只有mesh
// 非空才会真正创建Core的Door实例和UE的门组件，见door.h的ResolveDoorSpec。
struct DoorSpec {
	// ---- 外观 ----
	std::string mesh;                         // 门扇资产软路径；空=只是门洞，不放门
	std::string frameMesh;                    // 可选门框资产(静止不动)，空=不放门框

	// ---- 交互 ----
	std::string name;                         // 门名；非空才创建Script，见door.h"Script"一节
	std::string scriptModName = "empty";      // 仅name非空时使用
	std::vector<std::string> milestoneNames;  // 仅name非空时使用

	// ---- 门禁 ----
	int access = 0;                           // 0=Open 1=Owner 2=Locked，默认Open

	// ---- 开门动画参数(全部由Forever层代码驱动，资产不带动画) ----
	int style = 0;                            // 0=Slide推拉 1=Swing平开
	int leaves = 1;                           // 1=单扇 2=双扇对开(第二扇由代码镜像生成)
	float openAmount = 0.f;                   // Slide:滑出距离占扇宽比例(0=默认1)；
	                                           // Swing:开门角度，度(0=默认90)
	float openSeconds = 0.5f;                 // 开/关一次的时长，秒

	// 单扇门(leaves==1)的朝向随机化——双扇门不读这个字段(两扇对称镜像，没有"选哪一边"的
	// 歧义)。true=这扇门的"单侧朝向"(Slide的滑动方向/Swing的铰链边)在Door创建时随机决定
	// 一次，之后固定不变；同一扇门以后每次开关都用这个固定结果，不是每次开门重新随机。
	// 具体随机结果存在Core的Door实例上(Door::IsFlippedSide())，这个字段只是"要不要随机"
	// 的开关，不存结果本身。
	bool randomSide = false;
};

// ---------------------------------------------------------------- 植被

// 植被请求——mod只描述"在哪里种几棵、用什么mesh、基础缩放多少"，每棵树具体的位置/随机
// 旋转/随机缩放统一由Core的Vegetation展开(见Source/Core/map/vegetation.md)，规则只有一份。
// 坐标单位都是地图element(1=10m)。详见feature_spec.md"植被"一节。

// 沿from->to线段均匀种num棵(含两端，num==1时种在中点)。由RoadnetMod::AddTreeLine(在
// RoadnetMod::LayoutTrees里)记录，
// 坐标是世界坐标。
struct TreeLineRequest {
	float fromX = 0.f, fromY = 0.f;
	float toX = 0.f, toY = 0.f;
	int num = 0;
	float scale = 1.f;
	std::string meshPath;
};

// 在quad范围内均匀随机种num棵。由BuildingMod::AddTreeArea(在BuildingMod::LayoutTrees里)记录，
// quad就是LayoutTrees()拿到的那个
// building quad(中心是世界坐标，尺寸沿楼体局部轴向)，Core展开时再叠加building的旋转。
struct TreeAreaRequest {
	Quad quad;
	int num = 0;
	float scale = 1.f;
	std::string meshPath;
};

// ---------------------------------------------------------------- 建筑自定义模型

// DecorationSpec：建筑mod在场景里指定放置的一个StaticMesh(BuildingMod::AddDecoration记录)。
// 位置/yaw都按face=2(楼层模板"原样"那个朝向)定义，Core在Building::Layout()里按建筑最终朝向
// 用和楼层模板同一套InversePoint换算旋转，再叠加建筑自身的世界旋转——所以模型会跟着建筑
// 朝向一起转。z/缩放/pitch/roll不随朝向变化。详见feature_spec.md"建筑自定义模型"一节。
struct DecorationSpec {
	std::string meshPath;                        // StaticMesh资产软路径
	PointParams position{ 0.5f, 0.f, 0.5f, 0.f }; // 楼体局部(xRatio,xOffset,yRatio,yOffset)，和.layout模板PointParams同一套约定，默认楼体中心
	float z = 0.f;                               // 离地高度(地图单位)
	float scaleX = 1.f, scaleY = 1.f, scaleZ = 1.f; // 资产原尺寸的倍数(不做包围盒拟合)
	float yaw = 0.f, pitch = 0.f, roll = 0.f;    // 度；yaw按face=2定义，会叠加朝向和建筑旋转
};
