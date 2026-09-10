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
	const std::vector<std::pair<Lot*, std::unordered_map<int, Road*>>>& GetLots() const;
	const std::vector<RoadJunction*>& GetJunctions() const;
	Lot* LocateLot(const std::string& road, int index) const;

	// 车道分裂/开口(要求4/6/5):在roadName这条路上、距起点forward弧长比例t处，为vehicle(true)
	// 或pedestrian(false)图新增一个访问点。useForwardSide=true取该路正向一侧(side0)，false取
	// 反向一侧(side1)。openingWidth是开口沿道路方向的长度，供Forever层mesh生成时挖空对应长度、
	// 放置开口cube(记在road自己的openings里，见geometry.h的Road::AddOpening)。
	// 分裂规则：该侧该类车道数==1时直接把贯通线在t处切两段；>=2时贯通线(内侧车道)不动，
	// 另外新增一条两段的外侧车道线。返回新创建的访问点Node，找不到对应Road或该侧没有对应车道
	// 时返回nullptr。
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

	// 每条Road当前的"贯通线"记录(建图时创建，AddRoadAccessNode拆分单车道时会清空对应槽位)，
	// 下标0=车行side0，1=车行side1，2=行人side0，3=行人side1。fromAnchor/toAnchor按该方向
	// 实际通行方向排列(side0:沿Road Start->End；side1:沿End->Start)。
	struct ThroughLine {
		Connection* edge = nullptr;
		Node* fromAnchor = nullptr;
		Node* toAnchor = nullptr;
	};
	std::unordered_map<Road*, std::array<ThroughLine, 4>> throughLines;
};
