#pragma once

#include "common/mod_state.h"
#include "map/feature_spec.h"
#include "map/general_geometry.h"

#include <string>
#include <unordered_map>
#include <vector>


// 围墙:沿direction参考边铺设,mesh/unit语义同Road::GetMesh()/GetUnit(),见zone_mod.md。
struct ZoneWallSpec {
	int direction = FACE_WEST;
	float marginStart = 0.f;
	float marginEnd = 0.f;
	float depth = 0.f;
	bool depthInward = true;
	std::string mesh;
	float unit = 0.f;
};

// 大门:位置数据(围墙据此让出空当)+door,door.mesh为空=只是空当,见feature_spec.h。
struct ZoneGateSpec {
	int direction = FACE_WEST;
	float marginStart = 0.f;
	float marginEnd = 0.f;
	float depth = 0.f;
	bool depthInward = true;
	DoorSpec door;
};

// 出入口:局部坐标,原点在Zone矩形中心(与Lot局部坐标原点在WEST-NORTH角不同)。
struct ZoneAccessPoint {
	float x = 0.f;
	float y = 0.f;
	float width = 0.f;
};

// 园区内部道路:局部坐标(同ZoneAccessPoint)两端点+单一车道,单行车道或人行道二选一,见zone_mod.md。
// isVehicle=true沿(x1,y1)->(x2,y2)单向;false为人行道(双向)。
struct ZoneInternalRoadSpec {
	float x1 = 0.f, y1 = 0.f, x2 = 0.f, y2 = 0.f;
	bool isVehicle = false;
	float width = 0.f;
};

// 园区内部建筑:x/y/sizeX/sizeY是局部坐标+尺寸,roadIndices是FACE_DIRECTION->internalRoads下标
// (道路实例化后Core再解析成Road*),direction传给该建筑的BuildingMod::Layout(),见zone_mod.md。
struct ZoneInternalBuildingSpec {
	std::string type;
	float x = 0.f, y = 0.f, sizeX = 0.f, sizeY = 0.f;
	float relativeRotation = 0.f;
	int direction = FACE_WEST;
	std::unordered_map<int, int> roadIndices;
};

// 设计见zone_mod.md。
class ZoneMod {
public:
	ZoneMod() = default;
	virtual ~ZoneMod() = default;

	virtual const char* GetType() const = 0;

	virtual const char* GetName() = 0;

	// 被政府(公有)拥有的概率[0,1],Map::Checkin读取。
	float stateOwnedChance = 0.2f;

	// 落地后由Zone::Layout()调用一次,在这里填下面的布局字段。direction是Assign选中的方向,
	// quad/boundaryRoads是纯只读输入。默认空实现。
	virtual void Layout(int direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) {}

	// 围墙/大门/出入口/内部道路/内部建筑,纯值数据,由Map::InitZones()读回处理,在Layout(...)里设置。
	std::vector<ZoneWallSpec> walls;
	std::vector<ZoneGateSpec> gates;
	// 园区地面(建筑和内部道路之外的空隙)的材质软路径,空=不铺。渲染层只铺quad扣掉内部建筑quad后的空隙,见zone_mod.md。
	std::string gapMaterial;
	std::vector<ZoneAccessPoint> vehicleEntries;
	std::vector<ZoneAccessPoint> vehicleExits;
	std::vector<ZoneAccessPoint> pedestrianAccess;
	std::vector<ZoneInternalRoadSpec> internalRoads;
	std::vector<ZoneInternalBuildingSpec> internalBuildings;

	// 园区的Script,milestoneNames非空才创建,见Core/map/map.md"实体脚本"一节。在Layout(...)里设置。
	std::string scriptModName = "script_empty";
	std::vector<std::string> milestoneNames;

	// 放一个自定义StaticMesh,在Layout()里调用,坐标约定见zone_mod.md。
	void AddDecoration(const DecorationSpec& spec);
	std::vector<DecorationSpec> decorations;

	// 整张地图生成完后由Map::InitVegetation调用一次,用AddTree*提交树请求。默认不种树。
	virtual void LayoutTrees(int direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) {}

	// 在LayoutTrees里提交树请求(记进trees),参考系是园区quad、按朝北写,见zone_mod.md。
	void AddTreeSingle(const PointParams& position, float scale, const std::string& meshPath);
	void AddTreeLine(const PointParams& from, const PointParams& to, int num, float scale,
		const std::string& meshPath);
	void AddTreeArea(const PointParams& from, const PointParams& to, int num, float scale,
		const std::string& meshPath);
	TreeRequests trees;

	// 子类还必须实现static Assign(lots, emit, context)(基类不声明),通过RegisterZone注册,见zone_mod.md。

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
