#pragma once

#include "traffic/station_mod.h"
#include "traffic/route_mod.h"

#include <array>
#include <string>
#include <utility>
#include <vector>


// 射线从(x,y)沿(dirX,dirY)方向和[0,sizeX]x[0,sizeY]地图边界的交点，dirX/dirY不同时为0。
std::pair<float, float> RayBoxIntersection(float x, float y, float dirX, float dirY, int sizeX, int sizeY);

// 一段三次贝塞尔(世界坐标)：起点(x1,y1,z1)沿航向(h1x,h1y)出发，终点(x2,y2,z2)沿航向(h2x,h2y)到达，
// 两个控制点各在端点沿航向arm处(arm<=0=退化成直线，控制点取三等分点)，控制点z取端点z。terrainFollow只给mod自己的BuildTrack看。
TrackSegment MakeBezierSegment(float x1, float y1, float z1, float h1x, float h1y,
	float x2, float y2, float z2, float h2x, float h2y, float arm, bool terrainFollow = false);

// 平面曲线上的一个采样点：位置 + 单位切线(行驶方向)。
struct CurveSample {
	float x = 0.f, y = 0.f;
	float tx = 1.f, ty = 0.f;
};

// 把一段三次贝塞尔(只看平面)按弧长大约每step采样，返回含两端的采样点(至少2个)。
// 某点导数退化成零向量时切线取(fallbackX,fallbackY)。
std::vector<CurveSample> SampleBezier(const TrackSegment& curve, float step, float fallbackX, float fallbackY);

// 把一组采样点沿右手法线(-ty,tx，+Y向南)偏移offset(正=右、负=左)，相邻点之间用Hermite三次贝塞尔连起来(控制臂=偏移弦长/3)，
// z从z1线性过渡到z2。reverse=true时倒着走(切线取反)，用来取同一条中轴线另一个方向的铁轨。
// 同一组采样点的两个方向共用偏移基准，所以两条铁轨永远平行、不会交叉。
std::vector<TrackSegment> OffsetRail(const std::vector<CurveSample>& samples, float offset, bool reverse,
	float z1, float z2, bool terrainFollow);


// ================= 手动线路(编辑器output.transit)的公共部分，见route_mod.md"手动线路" =================
// TrainRoute/AirRoute在--skip时用它把"线路环+实体线路"规划成：用到的连线、节点处的转向、要补的缺口、停靠线路，
// 各自再把这份规划落成自己的几何(火车=双轨+偏移铁轨，飞机=单条航线)。

// 一条连线的中轴线三次Bezier：平面二次→三次，z=两端节点z、控制点z取端点z(所以z剖面是扁S，同编辑器)。
TrackSegment ManualAxis(const TransitData& data, int conn);
// 三次Bezier在[t0,t1]上的子段(de Casteljau)。
TrackSegment SubCubic(const TrackSegment& curve, float t0, float t1);
// 三次Bezier的位置/平面切线(未归一化)。
void CubicPoint(const TrackSegment& curve, float t, float& x, float& y, float& z);
void CubicTangent(const TrackSegment& curve, float t, float& tx, float& ty);
// 连线在node这一端的向外单位切线(离开node沿连线的方向)；退化返回(1,0)。
std::pair<float, float> ManualEndDirection(const TransitData& data, int conn, int node);

// 手动线路里的"端口"：符号描述，由火车/飞机各自落成具体几何。
struct ManualPort {
	enum Kind : int { StationIn, StationOut, NodeIn, NodeOut, Hub, HubOut } kind = NodeIn;
	int station = -1;       // StationIn/StationOut：RouteStationInfo::stationIndex 对应的下标(stations vector下标)
	int track = 0;          // StationIn/StationOut：进/出站口下标(火车=轨道，飞机=跑道)
	int node = -1;          // NodeIn/NodeOut/Hub：TransitData节点下标
	int conn = -1;          // NodeIn/NodeOut：TransitData连线下标(该连线在node这一端的 到达端/离开端)
	float hx = 1.f, hy = 0.f; // Hub/HubOut：行驶方向。Hub=节点前后都是缺口时的中心点(到达=离开)；掉头时(前后项在节点的两侧同向折回)Hub是到达端、HubOut是离开端，朝向相反
};
// 线路上的一个必经点：停靠站(station>=0,track)或途经点(port，必经的node端口)。
struct ManualLineEntry {
	int station = -1;       // >=0 = 停靠站(stations下标)，停靠位下标=track
	int track = 0;
	ManualPort via;         // station<0时：必经的node端口
};
struct ManualGap {
	ManualPort from, to;
	bool withinStation = false; // 飞机同站转场
};
struct ManualPlan {
	std::vector<int> conns;                          // 用到的连线(TransitData::conns下标，去重、升序)
	std::vector<std::array<int, 3>> turns;           // 节点处的转向 {node, 到达的连线, 离开的连线}(到达==离开=掉头)，去重
	std::vector<ManualGap> gaps;                     // 编辑器里的虚线：端口到端口的缺口，去重
	std::vector<ManualPort> hubs;                    // 缺口→缺口夹着的节点需要一个中心点；掉头的节点同时有Hub(到达)和HubOut(离开)两项
	std::vector<std::vector<ManualLineEntry>> lines; // 每个环一条线路
	std::vector<std::string> warnings;               // 丢弃项/环的原因，调用方debugf
};
// 规划：stations是LayoutRoute收到的站点表(下标即上面的station)，presetIndex匹配线路环里的预设。
// allowTrackChange=true(飞机)：降落跑道和起飞跑道可以不同，不同时在站内加滑行缺口并停两条跑道；false(火车)：进出同一条轨道。
ManualPlan PlanManualRoute(const TransitData& data, const std::string& routeId, const std::vector<RouteStationInfo>& stations, bool allowTrackChange);
