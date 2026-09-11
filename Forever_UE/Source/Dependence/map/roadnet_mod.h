#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include "map/geometry.h"

// RoadnetMod:路网布局方案的Mod扩展点。和Terrain(可以多个mod叠加分发)不同,一次只应该有
// 一个路网布局方案生效,由RoadnetFactory::SetConfig/GetRoadnet做单选,详见roadnet_factory.h。
class RoadnetMod {
public:
	RoadnetMod() = default;
	virtual ~RoadnetMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;
	virtual void ApplyArgs(const std::string& args) {}

	// 构建路网:根据地图宽高及地形/水域采样回调,把结果填进下面externs/intersections/roads/lots
	// 四个成员。nodeStaticCount是宿主传入的当前Node::count计数器值,实现开头必须先
	// Node::SetCount(nodeStaticCount)再开始构造任何Node/Intersection/Road,否则跨DLL的id会冲突;
	// 不要在mod里留下未通过这四个成员返回的Node实例。
	// 道路高度默认固定为0,不采样地形高度起伏(这次范围裁剪,详见roadnet.md)；隧道段是唯一的
	// 例外,靠固定TUNNEL_HEIGHT常量下探,不是采样出来的真实深度,见Basic层JingRoadnet实现。
	virtual void DistributeRoadnet(int width, int height,
		const std::function<std::string(int, int)>& getTerrain,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		int nodeStaticCount) = 0;

	// 工具方法(非虚,不需要重写):把connection上[t1,t2]这一段包成一个Quad追加进hatches——
	// 隧道口挖地形洞用,照抄老工程RoadnetMod::AddHatch语义。width是Quad垂直于connection方向
	// 的宽度，Quad朝向沿t1->t2端点连线方向。
	void AddHatch(const Connection* connection, float t1, float t2, float width);

	std::vector<Node> externs;
	std::vector<Intersection> intersections;
	std::vector<Road> roads;

	// 每个lot连同它的边界Road映射(int键=FACE_DIRECTION 0-3,缺失/找不到表示那一侧没有路)。
	// 不含边界Intersection映射——车行/行人导航图直接挂在Road/Intersection上,不需要通过lot中转。
	// 边界Road存指针，必须指向this->roads里的元素本身，不能另外new/复制一份——全图只应该有
	// 一份"这条物理路"的Road对象，lot的边界只是记一下"我贴着哪条路"，不是另起一份自己的拷贝。
	// 这是因为Zone/Building裁剪Lot空间时(Lot::SplitWithPath)会真的在这条边界Road上加
	// RoadOpening标记开口，如果边界指向的是独立拷贝，改动就传不到roads里真正会被渲染的那个
	// 对象上(PIE验证发现过这个问题：开口加了但画不出来)。实现里往这个map塞指针之前，必须确保
	// this->roads不会再增长(vector扩容会让之前取的地址失效)，也就是说构建lots之前要先把所有
	// addRoad-类的调用做完，再取&roads[i]这种地址。
	std::vector<std::pair<Lot, std::unordered_map<int, Road*>>> lots;

	// AddHatch产出的地形挖洞标记(Quad+旋转角)，宿主(Map::InitRoadnet)会把这里的每一项转发进
	// Map::AddHatch，复用Terrain已有的挖洞渲染机制，见map.md。
	std::vector<std::pair<Quad, float>> hatches;

	// Zone/Building裁剪Lot自由空间时自动生成的小路材质路径。留空表示不指定，Forever层渲染
	// 小路时退化用RoadPlain，见roadnet.md。
	std::string pathRoadMaterial;
};
