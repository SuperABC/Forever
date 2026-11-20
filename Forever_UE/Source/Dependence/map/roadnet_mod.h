#pragma once

#include "common/mod_test.h"
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

	// 单元测试：print输出关键信息，返回是否通过。没有深度测试的mod用FOREVER_MOD_GENERIC_UNIT_TEST(见common/mod_test.h)。
	virtual bool UnitTest(TestPrintFn print) = 0;
	// 构建路网,结果填进下面externs/intersections/roads/lots。nodeStaticCount是宿主的Node::count,
	// 实现开头必须先Node::SetCount(nodeStaticCount),否则跨DLL的id会冲突。
	// getHeight按地图element单位的连续坐标返回地形高度(含细化高度)。
	// getTerrain返回原始地形名(越界返回空串)；isConstruction返回该element是否可建设(3x3范围内高度都为0，
	// 和地形类型、水都无关，越界为false)。放建筑/地块只认isConstruction，不要再拿地形名判断。
	virtual void DistributeRoadnet(int width, int height,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<bool(int, int)>& isConstruction,
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
	// startWallBottom/endWallBottom：洞的t1端/t2端的端墙底部绝对高度(地图element单位)，默认kNoEndWall=这端不做墙。
	// 端墙从该端头地形最高处一直封到墙底，用来堵住隧道资产房顶和地形之间的空洞。
	void AddTunnelHatch(const Connection* connection, float t1, float t2, float width,
		float startWallBottom = kNoEndWall, float endWallBottom = kNoEndWall);

	// 工具方法(非虚):请求把地形贴合到connection上[t1,t2]的路面高度,立即采样成折线进terrainFits,不保留指针。
	void AddTerrainFit(const Connection* connection, float t1, float t2, float width, float falloff);

	std::vector<Node> externs;
	std::vector<Intersection> intersections;
	std::vector<Road> roads;

	// 每个lot自己登记边界Road映射。边界Road*必须指向this->roads里的元素本身(不能复制),
	// 且登记前roads不能再增长,原因见roadnet_mod.md。
	std::vector<Lot> lots;

	// 路网指定的绿化区(多边形)，mod在DistributeRoadnet里填，宿主(Map::InitGreens)读走，见green_mod.md。
	// links里的road是roads下标(不是指针)，Core按下标翻译成自己的Road*副本。
	struct GreenSite {
		struct Link {
			int edge = 0;   // 多边形第edge条边：points[edge] -> points[(edge+1)%n]
			int road = -1;  // roads下标
		};
		std::vector<float> xy;   // 顶点{x0,y0,x1,y1,...}，地图单位，宿主会规整成有符号面积>0
		std::vector<Link> links;
	};
	std::vector<GreenSite> greens;

	// 工具方法(非虚):请求给一段桥(chains里每条链是按行驶顺序首尾相接的Connection，通常是同一座桥拆出的几段Road)放桥墩,
	// 宿主(Roadnet::PlacePiers)按defaultSpacing/minSpacing选位、避开其它道路。链按值拷贝保存。参数含义同RouteMod::AddPier(PierRequest)。
	void AddPier(const std::string& mesh, std::vector<std::vector<Connection>> chains, float defaultSpacing, float minSpacing, float radius);
	std::vector<PierRequest> piers;

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
