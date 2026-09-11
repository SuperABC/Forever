#pragma once

#include "terrain.h"
#include "roadnet.h"
#include "map/terrain_factory.h"
#include "map/roadnet_factory.h"
#include "map/geometry.h"
#include "common/loader.h"

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <array>

// 10m*10m地图元素。当前只有Terrain域需要的字段;zone/building字段等Zone/Building阶段
// 迁移时再补。hatches现在就接好(挖洞用),但在Roadnet/Building迁移前始终为空,详见map.md。
struct Element {
	std::string terrain = "plain";
	float height = 0.f;
	std::pair<bool, float> water{ false, 0.f };
	std::vector<std::pair<Quad, float>> hatches;
};

// 阶段4-1 Map域的Map(聚合)雏形:只承担Terrain域需要的职责(尺寸、Element存储、
// TerrainFactory归属、地形分发、plain->construction晋升规则)。Zone/Block/Component/
// Room/Building/Roadnet迁移时会在这个类基础上继续扩展,不是最终形态,详见map.md。
class Map {
public:
	Map(int width, int height);
	~Map();

	// 用ModLoader发现/注册config.json配置的terrain mod dll,假定调用方已经完成过一次
	// Config::ReadConfig。
	void InitTerrains();

	// 按GetPriority()降序对所有已注册地形执行DistributeTerrain,再执行plain/construction
	// 的3x3晋升规则,最后重建terrainTextures索引表。
	void InitContents();

	std::pair<int, int> GetSize() const;

	std::string GetTerrain(int x, int y) const;
	bool SetTerrain(int x, int y, const std::string& terrain, std::pair<bool, float> water = { false, 0.f });
	float GetHeight(int x, int y) const;
	bool SetHeight(int x, int y, float height);
	std::pair<bool, float> GetWater(int x, int y) const;
	const std::vector<std::pair<Quad, float>>& GetHatches(int x, int y) const;

	// 自动分发到所有与q重叠的element(按q的旋转AABB计算重叠范围),和旧工程Map::AddHatch同语义。
	void AddHatch(Quad q, float rotation);

	// 地形类型 -> {纹理数组槽位索引, diffuse资产路径}
	const std::unordered_map<std::string, std::pair<int, std::string>>& GetTerrainTextures() const;

	// 用ModLoader发现/注册config.json配置的roadnet mod dll并构建路网,假定InitTerrains+InitContents
	// 已经跑完(DistributeRoadnet要采样已生成好的地形/水面)。构建顺序见map.md:深拷贝路网数据->
	// 地址编号->每个Intersection建RoadJunction(车行/行人锚点+路缘角点)->车行/行人双导航图
	// (每条Road的"最内侧车道贯通线"+每个RoadJunction的路口内部连接)。
	void InitRoadnet();

	const std::vector<Road*>& GetRoads() const;
	const std::vector<Intersection*>& GetIntersections() const;
	const std::vector<Node*>& GetExterns() const;
	const std::vector<Lot*>& GetLots() const;
	const std::vector<RoadJunction*>& GetJunctions() const;
	Lot* LocateLot(const std::string& road, int index) const;

	// 车道分裂/开口(要求4/6/5):在roadName这条路上、距起点forward弧长比例t处，为vehicle(true)
	// 或pedestrian(false)图新增一个访问点。useForwardSide含义分两种情况：该侧(side0/1)本身
	// 就有对应类别车道时，效果和原来一样(true=侧0/右手边，false=侧1/左手边)；该侧是单行道的
	// 空侧(对侧才有车道)时，重新解释成"要连最靠右(true)还是最靠左(false)的车道"，从对侧的
	// 车道里按实际横向位置挑——单行道不存在"正向/反向"这个参照了，只能按左右分。openingWidth
	// 是开口沿道路方向的长度，供Forever层mesh生成时挖空对应长度、放置开口cube(记在road自己的
	// openings里，见geometry.h的Road::AddOpening)。
	// 分裂规则：每条车道现在都有自己专属的贯通线（见ThroughLine注释），所以不管双向/单行，
	// 都是先选出目标车道再直接把它自己的贯通线在t处切两段——双向路车道数>=2时固定选最外侧
	// 车道（和原始设计"新访问点代表外侧车道"一致，不影响其余车道各自的贯通线）；单行路按
	// useForwardSide要求的左右方向选最靠右/最靠左的车道，见map.md"单行道开口"一节。返回
	// 新创建的访问点Node，两侧都没有对应类别车道时返回nullptr。
	Node* AddRoadAccessNode(const std::string& roadName, float t, bool isVehicle, bool useForwardSide, float openingWidth);

	// 车行/行人导航图只读访问，供Forever层可视化/未来Traffic域寻路使用。key/邻接id都是锚点
	// Node::GetId()，锚点本身的坐标通过GetNavAnchorNodes()（路口/车道分裂新增锚点）+
	// GetExterns()（地图边缘残端，也可能是graph里的端点）查到。
	const std::unordered_map<int, std::vector<std::pair<int, Connection*>>>& GetVehicleNavGraph() const;
	const std::unordered_map<int, std::vector<std::pair<int, Connection*>>>& GetPedestrianNavGraph() const;
	const std::vector<Node*>& GetNavAnchorNodes() const;

private:
	bool CheckXY(int x, int y) const;
	Element& At(int x, int y);
	const Element& At(int x, int y) const;

	int width;
	int height;
	std::vector<Element> elements; // 行主序扁平数组(y*width+x),取代旧工程Chunk分块

	TerrainFactory terrainFactory;
	RoadnetFactory roadnetFactory;

	// ModLoader持有mod dll句柄,必须活得至少和terrainFactory/roadnetFactory一样长——它们存的
	// creator/deleter函数指针指向这些dll的代码段,一旦ModLoader析构FreeLibrary掉dll,
	// 这些指针就悬空了(实测复现:CreateTerrain调用creator()时access violation)。所以这里
	// 是Map的成员,不是InitTerrains()/InitRoadnet()内的局部变量。
	ModLoader modLoader;

	std::unordered_map<std::string, std::pair<int, std::string>> terrainTextures;

	Roadnet* roadnet = nullptr;
	std::vector<RoadJunction*> junctions;

	// 车行/行人导航图：key是锚点Node::GetId()，value是(邻接锚点id, 边)列表。车行边只按实际
	// 通行方向单向插入；行人边(横道/转角/贯通线)双向插入，可能出现同一个Connection*被两条
	// 邻接记录共同引用，析构时用set去重删除一次。
	std::unordered_map<int, std::vector<std::pair<int, Connection*>>> vehicleNavGraph;
	std::unordered_map<int, std::vector<std::pair<int, Connection*>>> pedestrianNavGraph;

	// InitRoadnet/AddRoadAccessNode过程中创建的所有锚点Node(路口锚点+车道分裂新增的访问点)，
	// 析构时统一释放；导航图本身的Connection*从vehicleNavGraph/pedestrianNavGraph遍历去重释放。
	std::vector<Node*> navAnchorNodes;

	// 每条Road当前的"贯通线"记录(建图时创建，AddRoadAccessNode拆分某条车道时会清空对应
	// entry的edge)，下标0=车行side0，1=车行side1，2=行人side0，3=行人side1，每个下标
	// 对应一个vector，元素数量等于该side该类别的车道数——**每条物理车道都有自己独立的
	// entry**（第九轮迁移，起因是PIE导航图可视化验证时发现多车道路段只画出一条线：早期
	// 版本只保存"最内侧车道"或"单行道两端车道"，其余车道完全没有贯通线，车道数据和
	// 导航图对不上）。fromAnchor/toAnchor按该方向实际通行方向排列(side0:沿Road
	// Start->End；side1:沿End->Start)，车行取该车道在`RoadJunctionApproach::
	// vehicleInbound`/`vehicleOutbound`(现在也是逐车道的vector)里的专属锚点，不再是
	// 整个side共用一个锚点——否则哪怕每条车道各有一条Connection，几何上仍然会因为共用
	// 端点而重叠成一条看不出区别的线。laneIndex是这条贯通线对应该side车道数组
	// (vehicleLanes[side]/pedestrianLanes[side])里的下标，供AddRoadAccessNode按目标
	// 车道直接找到并断开对应entry，不再需要"内侧线不动、外侧新增分支"那套workaround
	// （每条车道现在都已经有自己专属的贯通线可以断），见roadnet.md"车道级导航锚点"一节。
	struct ThroughLine {
		Connection* edge = nullptr;
		Node* fromAnchor = nullptr;
		Node* toAnchor = nullptr;
		int laneIndex = 0;
	};
	std::unordered_map<Road*, std::array<std::vector<ThroughLine>, 4>> throughLines;
};
