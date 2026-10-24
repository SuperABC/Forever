#pragma once

#include "common/mod_state.h"
#include "map/general_geometry.h"
#include "map/feature_spec.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>


// 地形贴合请求(RoadnetMod::AddTerrainFit产出)：路面高度由mod指定，宿主(Map::ApplyTerrainFits)把
// 路面下和路边的地形高度贴到路面上——路面内地形高度=路面高度，路面边缘向外falloff距离内按到边缘的
// 距离平滑渐变回原地形，不管路面比地形高还是低都生效。存的是采样好的中轴线折线(纯值类型)，不是
// Connection指针：mod的roads会扩容，指针会失效。
struct TerrainFitSample {
	float x = 0.f;
	float y = 0.f;
	float z = 0.f; // 这一点的路面高度(地图element单位)
};
struct TerrainFitRequest {
	std::vector<TerrainFitSample> centerline;
	float width = 0.f;   // 路面整宽(地图element单位)
	float falloff = 0.f; // 路面边缘向外渐变回原地形的距离
};

// RoadnetMod:路网布局方案的Mod扩展点。和Terrain(可以多个mod叠加分发)不同,一次只应该有
// 一个路网布局方案生效,由RoadnetFactory::SetConfig/GetRoadnet做单选,详见roadnet_factory.h。
class RoadnetMod {
public:
	RoadnetMod() = default;
	virtual ~RoadnetMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 构建路网:根据地图宽高及地形/水域采样回调,把结果填进下面externs/intersections/roads/lots
	// 四个成员。nodeStaticCount是宿主传入的当前Node::count计数器值,实现开头必须先
	// Node::SetCount(nodeStaticCount)再开始构造任何Node/Intersection/Road,否则跨DLL的id会冲突;
	// 不要在mod里留下未通过这四个成员返回的Node实例。
	// getHeight按地图element单位的连续坐标返回当前地形高度(含细化高度)，路面想跟着山抬升时用它采样，
	// 再用AddTerrainFit把地形贴到路面上。道路高度由mod自己决定，默认可以不采样固定为0；隧道段靠
	// 固定TUNNEL_HEIGHT常量下探，见Basic层JingRoadnet实现。
	virtual void DistributeRoadnet(int width, int height,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		const std::function<float(float, float)>& getHeight,
		int nodeStaticCount) = 0;

	// 布局行道树:在整张地图(路网+园区+建筑+房间)全部生成完之后由Map::InitVegetation调用一次,
	// 这时roads上已经有Zone/Building/小路加上的全部开口(Road::GetOpenings),可以避开出入口。
	// hostRoads是宿主(Core)持有的路网深拷贝(不是this->roads那份mod自己的旧拷贝),只读访问,不要
	// 调用任何会修改它们的方法。在这里用AddTreeLine提交请求。默认不种树。
	virtual void LayoutTrees(const std::vector<Road*>& hostRoads) {}

	// 工具方法(非虚,不需要重写):把connection上[t1,t2]这一段包成一个Quad追加进hatches——
	// 隧道口挖地形洞用,照抄老工程RoadnetMod::AddHatch语义。width是Quad垂直于connection方向
	// 的宽度，Quad朝向沿t1->t2端点连线方向。
	void AddHatch(const Connection* connection, float t1, float t2, float width);

	// 工具方法(非虚,不需要重写):请求把地形高度贴合到connection上[t1,t2]这一段的路面高度——路面本身
	// 高度用的是connection的Node Z(GetPoint的Z)，width是路面整宽，falloff是路面边缘向外渐变回原地形
	// 的距离。立即沿弧长采样成折线追加进terrainFits，不保留connection指针。
	void AddTerrainFit(const Connection* connection, float t1, float t2, float width, float falloff);

	std::vector<Node> externs;
	std::vector<Intersection> intersections;
	std::vector<Road> roads;

	// 每个lot自己（通过Lot::SetBoundaryRoad或构造函数的boundary参数）登记好边界Road映射
	// (int键=FACE_DIRECTION 0-3,缺失/找不到表示那一侧没有路)。不含边界Intersection映射——
	// 车行/行人导航图直接挂在Road/Intersection上,不需要通过lot中转。边界Road存指针，必须
	// 指向this->roads里的元素本身，不能另外new/复制一份——全图只应该有一份"这条物理路"的
	// Road对象，lot的边界只是记一下"我贴着哪条路"，不是另起一份自己的拷贝。这是因为
	// Zone/Building裁剪Lot空间时(Lot::SplitWithPath)会真的在这条边界Road上加RoadOpening
	// 标记开口，如果边界指向的是独立拷贝，改动就传不到roads里真正会被渲染的那个对象上
	// (开口加了但画不出来)。实现里给某个方向登记指针之前，必须确保
	// this->roads不会再增长(vector扩容会让之前取的地址失效)，也就是说构建lots之前要先把所有
	// addRoad-类的调用做完，再取&roads[i]这种地址。
	// 这里直接是vector<Lot>而不是额外拿一个pair/map和Lot并排存边界信息——Lot本来就有
	// SetBoundaryRoad/GetBoundaryRoads这套API，边界信息应该就记在Lot自己身上；构造时可以
	// 直接把boundary map传给Lot的构造函数（等价于构造完再逐个调SetBoundaryRoad，见general_geometry.h
	// 的Lot构造函数注释），不需要在旁边另开一份等价的数据结构。
	std::vector<Lot> lots;

	// AddHatch产出的地形挖洞标记(Quad+旋转角)，宿主(Map::InitRoadnet)会把这里的每一项转发进
	// Map::AddHatch，复用Terrain已有的挖洞渲染机制，见map.md。
	std::vector<std::pair<Quad, float>> hatches;

	// AddTerrainFit产出的地形贴合请求，宿主(Map::InitRoadnet)按值拷贝走，见Map::ApplyTerrainFits。
	std::vector<TerrainFitRequest> terrainFits;

	// Zone/Building裁剪Lot自由空间时自动生成的小路材质路径。留空表示不指定，Forever层渲染
	// 小路时退化用RoadPlain，见roadnet.md。
	std::string pathRoadMaterial;

	// 工具方法(非虚,不需要重写):在LayoutTrees里提交单棵/一排/一片树的请求,只记进trees,每棵树的
	// 随机旋转/缩放、跳过水面和挖洞格子都由Core统一展开,见feature_spec.h的TreeRequests。路网的参考
	// 系是整张地图(不旋转),写世界坐标就用PointParams{0,x,0,y}。宿主(Roadnet::LayoutTrees)在调完
	// LayoutTrees之后按值拷贝走trees。
	void AddTreeSingle(const PointParams& position, float scale, const std::string& meshPath);
	void AddTreeLine(const PointParams& from, const PointParams& to, int num, float scale,
		const std::string& meshPath);
	void AddTreeArea(const PointParams& from, const PointParams& to, int num, float scale,
		const std::string& meshPath);
	TreeRequests trees;

	// ---- 存档(见Source/Dependence/common/mod_state.md) ----
	// mod私有数据的存档/读档钩子，默认不存任何东西。基类里的公共声明字段由Core直接快照，
	// 这里只放mod自己的私有数据。
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	// SaveState写出数据的格式版本：改了自己SaveState的格式就加一，读档时从
	// StateReader::GetSavedVersion()拿到当时的版本自己迁移。默认0。
	virtual int GetStateVersion() const { return 0; }
	// 这个mod类用来生成唯一实例名的静态计数器——存档保存、读档后恢复，避免读档后新建的实例和
	// 恢复出来的名字撞车。没有计数器的类保持默认(返回-1、Set不做事)。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
