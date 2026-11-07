#pragma once

#include "common/mod_state.h"
#include "map/general_geometry.h"
#include "map/feature_spec.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>


// 设计见roadnet_mod.md。
class RoadnetMod {
public:
	RoadnetMod() = default;
	virtual ~RoadnetMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 构建路网,结果填进下面externs/intersections/roads/lots。nodeStaticCount是宿主的Node::count,
	// 实现开头必须先Node::SetCount(nodeStaticCount),否则跨DLL的id会冲突。
	// getHeight按地图element单位的连续坐标返回地形高度(含细化高度)。
	virtual void DistributeRoadnet(int width, int height,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		const std::function<float(float, float)>& getHeight,
		int nodeStaticCount) = 0;

	// 整张地图生成完后由Map::InitVegetation调用一次,hostRoads是Core持有的路网(只读,已带全部开口)。
	// 在这里用AddTree*提交请求。默认不种树。
	virtual void LayoutTrees(const std::vector<Road*>& hostRoads) {}
	// 同LayoutTrees的时机和只读约定,用AddLamp*提交路灯请求(见feature_spec.h的LampRequests)。默认不放灯。
	virtual void LayoutLamps(const std::vector<Road*>& hostRoads) {}

	// 工具方法(非虚):把connection上[t1,t2]这一段包成Quad追加进hatches,挖地形洞用。
	void AddHatch(const Connection* connection, float t1, float t2, float width);

	// 工具方法(非虚):隧道口专用,挖洞并带侧壁;其它挖洞用AddHatch。
	void AddTunnelHatch(const Connection* connection, float t1, float t2, float width);

	// 工具方法(非虚):请求把地形贴合到connection上[t1,t2]的路面高度,立即采样成折线进terrainFits,不保留指针。
	void AddTerrainFit(const Connection* connection, float t1, float t2, float width, float falloff);

	std::vector<Node> externs;
	std::vector<Intersection> intersections;
	std::vector<Road> roads;

	// 每个lot自己登记边界Road映射。边界Road*必须指向this->roads里的元素本身(不能复制),
	// 且登记前roads不能再增长,原因见roadnet_mod.md。
	std::vector<Lot> lots;

	// AddHatch/AddTunnelHatch产出的挖洞标记,宿主(Map::InitRoadnet)转发进Map::AddHatch。
	std::vector<TerrainHatch> hatches;

	// AddTerrainFit产出的请求,宿主按值拷贝走,见Map::ApplyTerrainFits。
	std::vector<TerrainFitRequest> terrainFits;

	// Zone/Building裁剪Lot时自动生成的小路材质路径,留空由Forever层退化用默认材质。
	std::string pathRoadMaterial;

	// 工具方法(非虚):在LayoutTrees里提交树请求(记进trees),参考系为整张地图,见roadnet_mod.md。
	void AddTreeSingle(const PointParams& position, float scale, const std::string& meshPath);
	void AddTreeLine(const PointParams& from, const PointParams& to, int num, float scale,
		const std::string& meshPath);
	void AddTreeArea(const PointParams& from, const PointParams& to, int num, float scale,
		const std::string& meshPath);
	TreeRequests trees;

	// 工具方法(非虚):在LayoutLamps里提交路灯请求(记进lamps),参考系同树。
	void AddLampSingle(const PointParams& position, float rotationDegrees, const std::string& blueprintPath);
	void AddLampLine(const PointParams& from, const PointParams& to, int num, float rotationDegrees,
		const std::string& blueprintPath);
	void AddLampArea(const PointParams& from, const PointParams& to, int num, float rotationDegrees,
		const std::string& blueprintPath);
	LampRequests lamps;

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
