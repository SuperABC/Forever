#include "route_common.h"

#include <algorithm>
#include <cmath>
#include <cfloat>



using namespace std;

// 射线从(x,y)沿(dirX,dirY)方向和[0,sizeX]x[0,sizeY]地图边界的交点——dirX/dirY不会同时为0
// (接口朝向不可能是零向量)。
pair<float, float> RayBoxIntersection(float x, float y, float dirX, float dirY, int sizeX, int sizeY) {
	float tBest = 1e9f;
	if (dirX > 0.f) tBest = min(tBest, (static_cast<float>(sizeX) - x) / dirX);
	else if (dirX < 0.f) tBest = min(tBest, (0.f - x) / dirX);
	if (dirY > 0.f) tBest = min(tBest, (static_cast<float>(sizeY) - y) / dirY);
	else if (dirY < 0.f) tBest = min(tBest, (0.f - y) / dirY);
	if (tBest < 0.f || tBest > 1e8f) tBest = 0.f; // 理论不会发生，防御性兜底(站点已经在图内)
	return { x + dirX * tBest, y + dirY * tBest };
}

// TrainRoute/AirRoute共用的单站双跑道(轨道)四接口环线拓扑，见route_common.md。
// overrideEdgeZ为true时边缘站点的z取edgeHeight(AirRoute用巡航高度)。
// edgeBase是本站8个边缘站点在Core站点总列表里的起始下标；结果追加到out*，多站点各调一次。
void LayoutDualTrackStationLoop(const vector<RouteStationInfo>& interfaces, int sizeX, int sizeY,
	bool overrideEdgeZ, float edgeHeight, int edgeBase,
	vector<RouteStationInfo>& outEdgeStations, vector<RouteLink>& outLinks,
	vector<vector<RouteStop>>& outLines) {
	if (interfaces.size() < 4) return; // 没有真实站点(或接口不全)，这条线路留空

	const RouteStationInfo& leftIn = interfaces[0];
	const RouteStationInfo& rightOut = interfaces[1];
	const RouteStationInfo& rightIn = interfaces[2];
	const RouteStationInfo& leftOut = interfaces[3];

	// 每个地图边界有两条线(出地图一条、进地图一条，分别在两条平行轨道的延长线上)，各自在边界上有一个
	// 端点(Out/In)，中间再插一个虚拟车站Mid：火车在Mid停靠但不触发进站/离站事件(RouteStop::silent)。
	// Out->Mid->In是贴着地图边界的一小段横向连线：Out到达朝图外、出发朝侧向(Out指向In)；Mid两头
	// 都朝侧向；In到达朝侧向、出发朝图内，所以三点之间都是直线，不会鼓出去。
	auto makeEdgeStations = [&](const RouteStationInfo& outSource, float outDirX, float outDirY,
		const RouteStationInfo& inSource, float inDirX, float inDirY,
		const char* outName, const char* midName, const char* inName,
		RouteStationInfo& outEdge, RouteStationInfo& midEdge, RouteStationInfo& inEdge) {
		auto [outX, outY] = RayBoxIntersection(outSource.x, outSource.y, outDirX, outDirY, sizeX, sizeY);
		auto [inX, inY] = RayBoxIntersection(inSource.x, inSource.y, inDirX, inDirY, sizeX, sizeY);
		float outZ = overrideEdgeZ ? edgeHeight : outSource.z;
		float inZ = overrideEdgeZ ? edgeHeight : inSource.z;

		float sideX = inX - outX, sideY = inY - outY;
		float sideLength = sqrtf(sideX * sideX + sideY * sideY);
		if (sideLength > 1e-6f) { sideX /= sideLength; sideY /= sideLength; }
		else { sideX = -outDirY; sideY = outDirX; } // 两点重合(理论不会)，任取一个垂直方向

		outEdge.name = outName;
		outEdge.x = outX; outEdge.y = outY; outEdge.z = outZ;
		outEdge.arriveDirX = outDirX; outEdge.arriveDirY = outDirY;
		outEdge.departDirX = sideX; outEdge.departDirY = sideY;
		outEdge.isEdge = true;

		midEdge.name = midName;
		midEdge.x = (outX + inX) * 0.5f; midEdge.y = (outY + inY) * 0.5f; midEdge.z = (outZ + inZ) * 0.5f;
		midEdge.arriveDirX = sideX; midEdge.arriveDirY = sideY;
		midEdge.departDirX = sideX; midEdge.departDirY = sideY;
		midEdge.isEdge = true;

		inEdge.name = inName;
		inEdge.x = inX; inEdge.y = inY; inEdge.z = inZ;
		inEdge.arriveDirX = sideX; inEdge.arriveDirY = sideY;
		inEdge.departDirX = -outDirX; inEdge.departDirY = -outDirY;
		inEdge.isEdge = true;
	};

	// 左边界：出地图的线是leftOut(沿它的出发方向出图)，进地图的线是leftIn(沿它出发方向的反向回溯到边界)。
	RouteStationInfo leftEdgeOut, leftEdgeMid, leftEdgeIn;
	makeEdgeStations(leftOut, leftOut.departDirX, leftOut.departDirY,
		leftIn, -leftIn.departDirX, -leftIn.departDirY,
		"edgeLeftOut", "edgeLeftMid", "edgeLeftIn", leftEdgeOut, leftEdgeMid, leftEdgeIn);
	// 右边界：出地图的线是rightOut，进地图的线是rightIn。
	RouteStationInfo rightEdgeOut, rightEdgeMid, rightEdgeIn;
	makeEdgeStations(rightOut, rightOut.departDirX, rightOut.departDirY,
		rightIn, -rightIn.departDirX, -rightIn.departDirY,
		"edgeRightOut", "edgeRightMid", "edgeRightIn", rightEdgeOut, rightEdgeMid, rightEdgeIn);

	// 真正的停靠点：跑道/轨道中点(只有这里dwell=true)，其余接口和边缘端点都是纯过路点。
	// isEdge=true只是复用Route::Build()创建合成站点的逻辑，不代表真的是地图边缘。
	// 两端共线同向，位置和朝向取平均即可，贝塞尔会自然退化成直线。
	RouteStationInfo stop1; // 跑道/轨道1(leftIn<->rightOut)中点
	stop1.name = "stop1";
	stop1.x = (leftIn.x + rightOut.x) * 0.5f;
	stop1.y = (leftIn.y + rightOut.y) * 0.5f;
	stop1.z = (leftIn.z + rightOut.z) * 0.5f;
	stop1.arriveDirX = leftIn.departDirX; stop1.arriveDirY = leftIn.departDirY;
	stop1.departDirX = leftIn.departDirX; stop1.departDirY = leftIn.departDirY;
	stop1.isEdge = true;

	RouteStationInfo stop2; // 跑道/轨道2(rightIn<->leftOut)中点
	stop2.name = "stop2";
	stop2.x = (rightIn.x + leftOut.x) * 0.5f;
	stop2.y = (rightIn.y + leftOut.y) * 0.5f;
	stop2.z = (rightIn.z + leftOut.z) * 0.5f;
	stop2.arriveDirX = rightIn.departDirX; stop2.arriveDirY = rightIn.departDirY;
	stop2.departDirX = rightIn.departDirX; stop2.departDirY = rightIn.departDirY;
	stop2.isEdge = true;

	outEdgeStations.insert(outEdgeStations.end(),
		{ leftEdgeOut, leftEdgeMid, leftEdgeIn, rightEdgeOut, rightEdgeMid, rightEdgeIn, stop1, stop2 });

	// edgeStations接在真实站点后面，下标顺序和上面列表里的顺序一一对应(stop1/stop2见route_common.h的常量)。
	int realStation = leftIn.stationIndex;
	auto edgeStop = [&](int k, bool dwell, bool silent) { return RouteStop{ edgeBase + k, 0, dwell, silent }; };

	RouteStop stopLeftEdgeOut = edgeStop(0, false, false);
	RouteStop stopLeftEdgeMid = edgeStop(1, true, true);  // 虚拟车站：停靠但不触发事件
	RouteStop stopLeftEdgeIn = edgeStop(2, false, false);
	RouteStop stopRightEdgeOut = edgeStop(3, false, false);
	RouteStop stopRightEdgeMid = edgeStop(4, true, true);
	RouteStop stopRightEdgeIn = edgeStop(5, false, false);
	RouteStop stopMid1 = edgeStop(kDualTrackStop1, true, false);
	RouteStop stopMid2 = edgeStop(kDualTrackStop2, true, false);
	RouteStop stopLeftIn{ realStation, leftIn.interfaceIndex, false };
	RouteStop stopRightOut{ realStation, rightOut.interfaceIndex, false };
	RouteStop stopRightIn{ realStation, rightIn.interfaceIndex, false };
	RouteStop stopLeftOut{ realStation, leftOut.interfaceIndex, false };

	outLines.push_back({ stopLeftEdgeIn, stopLeftIn, stopMid1, stopRightOut, stopRightEdgeOut, stopRightEdgeMid,
		stopRightEdgeIn, stopRightIn, stopMid2, stopLeftOut, stopLeftEdgeOut, stopLeftEdgeMid });

	outLinks.push_back({ stopLeftEdgeIn, stopLeftIn, false });
	outLinks.push_back({ stopLeftIn, stopMid1, false });       // 跑道/轨道1前半段
	outLinks.push_back({ stopMid1, stopRightOut, false });     // 跑道/轨道1后半段
	outLinks.push_back({ stopRightOut, stopRightEdgeOut, false });
	outLinks.push_back({ stopRightEdgeOut, stopRightEdgeMid, false });
	outLinks.push_back({ stopRightEdgeMid, stopRightEdgeIn, false });
	outLinks.push_back({ stopRightEdgeIn, stopRightIn, false });
	outLinks.push_back({ stopRightIn, stopMid2, false });       // 跑道/轨道2前半段
	outLinks.push_back({ stopMid2, stopLeftOut, false });       // 跑道/轨道2后半段
	outLinks.push_back({ stopLeftOut, stopLeftEdgeOut, false });
	outLinks.push_back({ stopLeftEdgeOut, stopLeftEdgeMid, false });
	outLinks.push_back({ stopLeftEdgeMid, stopLeftEdgeIn, false });
}
