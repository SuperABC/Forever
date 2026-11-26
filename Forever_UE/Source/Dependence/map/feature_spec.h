#pragma once

#include "map/general_geometry.h"

#include <string>
#include <vector>


// map域各种feature(门、植被、路灯、自定义模型)的纯数据描述,mod填写、Core读取,详见feature_spec.md。

// ---------------------------------------------------------------- 门

// 一扇门的外观/交互描述,mesh为空=只是门洞不放门,见feature_spec.md。
// 门是骨骼动画：Forever层只把"开合度"(0=全关,1=全开)喂给动画蓝图,模型怎么动完全由骨骼网格+动画蓝图决定。
struct DoorSpec {
	// ---- 外观 ----
	std::string mesh;                         // 门的骨骼网格资产软路径(含门框，会动的部分绑在骨上)；空=只是门洞，不放门
	std::string animClass;                    // 动画蓝图类软路径(…_C，父类是UDoorAnimInstance)；空=用基类，门不动

	// ---- 交互 ----
	std::string name;                         // 门名；非空才创建Script，见door.h"Script"一节
	std::string scriptModName;                       // 仅name非空时使用；空=脚本没有ScriptMod
	std::vector<std::string> milestoneNames;  // 仅name非空时使用

	// ---- 门禁 ----
	int access = 0;                           // 0=Open 1=Owner 2=Locked，默认Open

	// ---- 开门动画 ----
	float openSeconds = 0.5f;                 // 开/关一次的时长，秒(开合度从0到1所用时间)

	// ---- 缩放(Forever层把骨骼网格摆进门洞时用，详见feature_spec.md"门的缩放") ----
	// 宽度缩放：0=让网格包围盒宽度匹配门洞宽度；非0=直接用这个值(相对网格原始尺寸的倍数)。
	float widthScale = 0.f;
	// 高度缩放：非0=直接用这个值；0=建筑/房间门让网格包围盒高度匹配门洞高度，园区门没有门洞高度，跟随宽度缩放。
	float heightScale = 0.f;
	// 厚度方向的缩放跟随谁：false=跟随宽度缩放(平开/推拉门，绕竖直轴转动时不会被拉变形)；
	// true=跟随高度缩放(掀开的车库门，门板沿高度方向翻转)。
	bool thicknessFollowsHeight = false;
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

// ---------------------------------------------------------------- 房间家具

// 房间局部系里的墙(见room.md"房间局部坐标系"：u沿正面墙，v朝正面为正，原点在角上)。
enum ROOM_WALL : int {
	ROOM_WALL_FRONT = 0, // v = depth(正面墙，门/临路一侧)
	ROOM_WALL_BACK = 1,  // v = 0
	ROOM_WALL_U_MIN = 2, // u = 0
	ROOM_WALL_U_MAX = 3, // u = width
};

// 房间墙上的一个门/窗洞(米)。a0<a1是沿墙的范围：FRONT/BACK墙上是u，U_MIN/U_MAX墙上是v；z0<z1是离地高度。
struct RoomOpening {
	int wall = ROOM_WALL_FRONT;
	float a0 = 0.f, a1 = 0.f;
	float z0 = 0.f, z1 = 0.f;
	std::string tag; // 门洞的tag(如"home")，窗为空
};

// Core在调RoomMod::Layout时交给房间mod的几何信息(米)：房间局部矩形width(u轴)x depth(v轴)，层高，门窗洞。
struct RoomLayoutInfo {
	float width = 0.f;  // u轴长(米)
	float depth = 0.f;  // v轴长(米)
	float height = 0.f; // 层高(米)
	std::vector<RoomOpening> doors;
	std::vector<RoomOpening> windows;
};

// 房间mod放置的一件家具(RoomMod::AddFurniture记录)，位置是房间局部系(米，原点在房间角上)，
// 和DecorationSpec不同，不需要考虑朝向：Core按房间朝向换算成DecorationInstance。
struct FurnitureSpec {
	std::string meshPath;                           // StaticMesh资产软路径
	float u = 0.f, v = 0.f;                         // 家具原点(资产原点，通常是底面中心)在房间局部系的位置(米)
	float z = 0.f;                                  // 离地板高度(米)
	float yaw = 0.f;                                // 度，0=朝正面(+v)
	float scaleX = 1.f, scaleY = 1.f, scaleZ = 1.f; // 资产原尺寸的倍数
};

// 房间mod在自己房间里放的一件物品(RoomMod::PlaceAsset记录)：由AssetMod生成的普通物品，带物理、能被捡走。
// 坐标约定和FurnitureSpec完全一样(房间局部米，yaw度、0=朝+v)。z是物品底面离地板的高度。
// assetType对应的物品mod没启用时Core跳过这一件并打警告。
struct RoomAssetSpec {
	std::string assetType; // AssetMod的id
	int count = 1;         // 这一件的堆叠数量(1个Asset实例)；要N个独立的物品就PlaceAsset N次
	float u = 0.f, v = 0.f;
	float z = 0.f;
	float yaw = 0.f;
};

// 房间mod在自己房间里放的一件固定物品(RoomMod::PlaceFixture记录)：没有AssetMod，只有一个显示用的网格、
// 一个弹选项用的检测盒(UE层按网格包围盒外扩)和一组脚本描述；不能移动、不做物理模拟，但像家具一样挡人。
// 坐标约定和FurnitureSpec完全一样；网格原点在底面中心、+X朝正面。
struct RoomFixtureSpec {
	std::string type;      // 这类固定物品叫什么(比如"bed")，脚本里的$$self.type，也用来拼名字
	std::string meshPath;  // StaticMesh资产软路径
	float u = 0.f, v = 0.f;
	float z = 0.f;
	float yaw = 0.f;
	float scaleX = 1.f, scaleY = 1.f, scaleZ = 1.f;
	std::string scriptModName;               // 空=脚本没有ScriptMod(逻辑全在里程碑文件里)
	std::vector<std::string> milestoneNames; // 和scriptModName都空=没有脚本，也就没有选项
};
