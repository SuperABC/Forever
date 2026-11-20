#pragma once

#include "class.h"

#include "map/green_mod.h"
#include "map/green_factory.h"
#include "map/general_geometry.h"

#include <string>
#include <utility>
#include <vector>


// 已落地的绿化区(路网指定的多边形)，独占一个GreenMod实例，设计见green.md。
class Green {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
	// 导航节点由Map::InitGreens建，直接读写圆的节点字段。
	friend class Map;
public:
	Green() = delete;

	// 独占持有mod实例；Layout()在构造后单独调用
	Green(GreenFactory* factory, GreenMod* mod);
	~Green();

	// 绿化区连到道路的一条边：多边形第edge条边(points[edge]->points[(edge+1)%n])连road。
	struct Link {
		int edge = 0;
		Road* road = nullptr;
	};

	// 保留下来的市民活动圆(mod指定的圆里通过了"连接线在多边形内"检查的)。
	struct Circle {
		float x = 0.f, y = 0.f, r = 0.f;
		int edge = -1;            // 选中的连路的边(-1=绿化区没连路，或这个圆没有可用的边)
		float footX = 0.f, footY = 0.f; // 圆心在这条边上的最近点
		Node* centerNode = nullptr;     // 行人导航节点，归Map::navAnchorNodes所有，不连路时为空
		Node* footNode = nullptr;
	};

	std::string GetType() const;
	std::string GetName() const;
	GreenMod* GetMod() const;

	// 转发mod->Layout，之后快照circles/decorations：多边形外的装饰物丢弃，圆按下面规则过滤、重新编号。
	// xy是已规整成有符号面积>0的顶点，links是连路的边(road已非空)。
	// 圆过滤：圆心必须在多边形内；绿化区连了路时，对每个圆按"圆心到连路边的线段最短距离"从近到远试，
	// 取第一条使"圆心->线段最近点"整条连线都在多边形内的边，都不行就丢弃这个圆；没连路的绿化区圆原样保留。
	void Layout(const std::vector<float>& xy, const std::vector<Link>& links);

	const std::vector<float>& GetPolygon() const;
	void GetBounds(float& minX, float& minY, float& maxX, float& maxY) const;
	bool IsLinked() const;
	const std::vector<Link>& GetLinks() const;
	const std::vector<Circle>& GetCircles() const;

	// 地址：连了路=="<首条连路的路名> -1 <绿化名>"；没连路==绿化名(不支持寻址)。
	std::string GetAddress() const;
	// 连了路时：GetAddress()+" "+圆序号，和Map::LocateGreen对应；没连路返回空串。
	std::string GetCircleAddress(int circleIndex) const;

	// mod->decorations按包围盒换算成世界摆位后的快照(多边形外的已丢弃)，z=离地高度，UE层加地形高度。
	const std::vector<DecorationInstance>& GetDecorations() const;

	// 圆内均匀随机一点，用于导航终点；圆序号越界返回圆心。调用方负责随机作用域。
	std::pair<float, float> RandomPointInCircle(int circleIndex) const;

private:
	GreenMod* mod;
	GreenFactory* factory;
	std::string type;
	std::string name;

	// 快照(读档时mod是空实例，运行时只读这些)
	std::vector<float> polygon;
	float minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;
	std::vector<Link> links;
	std::vector<Circle> circles;
	std::vector<DecorationInstance> decorations;
};
