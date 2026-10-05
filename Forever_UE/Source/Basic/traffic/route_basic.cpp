#include "route_basic.h"

#include <algorithm>
#include <cmath>

using namespace std;

namespace {
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

	// TrainRoute/AirRoute共用的"单站双跑道(轨道)、四接口"拓扑：
	// 左边界->leftIn->rightOut->右边界->rightIn->leftOut->左边界，闭合成一个环。interfaces
	// 传入顺序固定为[leftIn, rightOut, rightIn, leftOut](TrainStation/AirStation::Layout
	// 保证)——leftIn/rightOut这两个接口朝向相同(沿frontage轴同一方向)，中间那条直线就是
	// "跑道/轨道1"；rightIn/leftOut同理朝相反方向，是"跑道/轨道2"。
	//
	// 左边界的位置沿leftIn朝向的反方向(即leftIn进站之前、飞机/火车从图外飞来的方向)射线出图
	// 边界得到；右边界同理沿rightOut朝向(飞机/火车离站后继续飞向图外的方向)射线出图。两个
	// 边界节点的arrive/depart朝向都直接复用相邻接口自己的朝向(同向的一段直接复用，掉头的
	// 那一段取反)，和原来"单接口"版本(已删除)的edge1/edge2同一个思路，只是现在左右边界各自
	// 固定对应着leftIn/rightOut这一组，不再各管各的一个独立接口。
	// edgeHeight覆盖边缘站点的z(AirRoute用巡航高度，TrainRoute用接口原本的z)。
	void LayoutDualTrackStationLoop(const vector<RouteStationInfo>& interfaces, int sizeX, int sizeY,
		bool overrideEdgeZ, float edgeHeight,
		vector<RouteStationInfo>& outEdgeStations, vector<RouteLink>& outLinks,
		vector<vector<RouteStop>>& outLines) {
		if (interfaces.size() < 4) return; // 没有真实站点(或接口不全)，这条线路留空

		const RouteStationInfo& leftIn = interfaces[0];
		const RouteStationInfo& rightOut = interfaces[1];
		const RouteStationInfo& rightIn = interfaces[2];
		const RouteStationInfo& leftOut = interfaces[3];

		auto [leftEdgeX, leftEdgeY] = RayBoxIntersection(leftIn.x, leftIn.y, -leftIn.departDirX, -leftIn.departDirY, sizeX, sizeY);
		auto [rightEdgeX, rightEdgeY] = RayBoxIntersection(rightOut.x, rightOut.y, rightOut.departDirX, rightOut.departDirY, sizeX, sizeY);

		RouteStationInfo leftEdge;
		leftEdge.name = "edgeLeft";
		leftEdge.x = leftEdgeX; leftEdge.y = leftEdgeY; leftEdge.z = overrideEdgeZ ? edgeHeight : leftIn.z;
		leftEdge.arriveDirX = -leftIn.departDirX; leftEdge.arriveDirY = -leftIn.departDirY; // 到达时继续朝图外(=leftOut朝向)
		leftEdge.departDirX = leftIn.departDirX; leftEdge.departDirY = leftIn.departDirY;   // 出发时朝leftIn
		leftEdge.isEdge = true;

		RouteStationInfo rightEdge;
		rightEdge.name = "edgeRight";
		rightEdge.x = rightEdgeX; rightEdge.y = rightEdgeY; rightEdge.z = overrideEdgeZ ? edgeHeight : rightOut.z;
		rightEdge.arriveDirX = rightOut.departDirX; rightEdge.arriveDirY = rightOut.departDirY;   // 到达时继续朝图外
		rightEdge.departDirX = -rightOut.departDirX; rightEdge.departDirY = -rightOut.departDirY; // 出发时朝rightIn(=rightIn朝向)
		rightEdge.isEdge = true;

		// 真正的停靠点——跑道/轨道中点，飞机/火车只在这里停dwellSeconds。leftIn/rightOut/
		// rightIn/leftOut(入站/出站接口)和leftEdge/rightEdge(地图边缘点)全部降级成纯过路点
		// (RouteStop::dwell=false，到达后不停留直接接着跑下一腿)，PIE实测反馈"现在火车和飞机
		// 都是在入站接口停一下，再在出站接口停一下，这不合理"——真实世界里乘客在跑道/站台中段
		// 上下客，不是在跑道两端分别停一次。stop1/stop2和leftEdge/rightEdge一样借用"没有
		// building的合成站点"机制(isEdge=true只是为了复用Route::Build()已有的创建+收集逻辑，
		// 不代表真的是地图边缘)。两点都落在各自跑道的直线上(同一朝向)，中点位置+朝向直接取
		// 两端平均即可，贝塞尔控制点会自然退化成直线，不会在中点产生拐弯。
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

		outEdgeStations = { leftEdge, rightEdge, stop1, stop2 };

		int realStation = leftIn.stationIndex;
		int leftEdgeIndex = realStation + 1;  // edgeStations接在真实站点后面，这条线路假定全图
		int rightEdgeIndex = realStation + 2; // 只有这一个真实站点(下标就是realStation)，顺序
		int stop1Index = realStation + 3;     // 和上面outEdgeStations列表里的顺序一一对应
		int stop2Index = realStation + 4;

		RouteStop stopLeftEdge{ leftEdgeIndex, 0, false };
		RouteStop stopLeftIn{ realStation, leftIn.interfaceIndex, false };
		RouteStop stopMid1{ stop1Index, 0, true };
		RouteStop stopRightOut{ realStation, rightOut.interfaceIndex, false };
		RouteStop stopRightEdge{ rightEdgeIndex, 0, false };
		RouteStop stopRightIn{ realStation, rightIn.interfaceIndex, false };
		RouteStop stopMid2{ stop2Index, 0, true };
		RouteStop stopLeftOut{ realStation, leftOut.interfaceIndex, false };

		outLines.push_back({ stopLeftEdge, stopLeftIn, stopMid1, stopRightOut, stopRightEdge, stopRightIn, stopMid2, stopLeftOut });

		outLinks.push_back({ stopLeftEdge, stopLeftIn, false });
		outLinks.push_back({ stopLeftIn, stopMid1, false });       // 跑道/轨道1前半段
		outLinks.push_back({ stopMid1, stopRightOut, false });     // 跑道/轨道1后半段
		outLinks.push_back({ stopRightOut, stopRightEdge, false });
		outLinks.push_back({ stopRightEdge, stopRightIn, false });
		outLinks.push_back({ stopRightIn, stopMid2, false });       // 跑道/轨道2前半段
		outLinks.push_back({ stopMid2, stopLeftOut, false });       // 跑道/轨道2后半段
		outLinks.push_back({ stopLeftOut, stopLeftEdge, false });
	}
}

int BusRoute::count = 0;

BusRoute::BusRoute() : id(count++) {
}

const char* BusRoute::GetName() {
	name = "BusRoute" + to_string(id);
	return name.data();
}

void BusRoute::SetProperty() {
	stationType = "bus";
	vehicleType = "vehicle_bus";
	useRoadnet = true;
	drawPath = false;

	// speed/dwellSeconds的单位是"地图单位/游戏内秒"、"游戏内秒"，不是真实秒——Traffic::Tick
	// 喂给Route::Update的elapsedSeconds来自Player::Tick()里的time->AddMilliseconds(delta*60*
	// 1000*timeFlowRatio)，默认timeFlowRatio=2.0，也就是1个真实秒=120个游戏内秒。继续用
	// RouteMod基类默认的speed=1.f/dwellSeconds=30.f相当于车跑120地图单位/真实秒、停靠只有
	// 0.25真实秒——PIE实测反馈"所有载具的移动速度都太快了，停站的时间也太短了"就是这个换算
	// 没做。这里按"车环一圈(公交站环线实测约120~180地图单位)大约跑20~30真实秒、停站约3真实秒"
	// 校准：speed=0.05(=6地图单位/真实秒)，dwellSeconds=360(=3真实秒)。如果以后time_flow_ratio
	// 被剧情脚本改了，这几个值的真实秒观感会跟着等比例变化，到时候要一起重新校准。用户要求
	// 所有公共交通速度减半，这里改成speed=0.025(=3地图单位/真实秒，跑一圈大约40~60真实秒)，
	// dwellSeconds不受影响。
	speed = 0.025f;
	dwellSeconds = 360.f;

	// easeSeconds默认3秒(RouteMod基类默认值)还是太突兀，PIE实测反馈三种公共交通的加速度
	// 都要大幅减小——公交每一站都停(dwellAtDestination恒为true)，"行驶区段"退化成单条leg，
	// 调大这个值时DriveVehicle()会自动把ta/td钳在"这条leg行驶时长的一半"，站间距短的leg
	// 会自然变成全程加速+全程减速、没有真正匀速的中段，这正是"尽量缓"这个要求在站间距有限
	// 时能做到的最大效果，不需要额外处理，直接给一个足够大的值即可。15秒PIE实测反馈还要
	// 继续减小，调到20(注意DriveVehicle()里这个字段真实秒->游戏内秒的换算系数之前漏乘了，
	// 之前几轮调大基本没生效，见route.md"DriveVehicle的缓动"一节，这次换算修好之后20秒
	// 的效果会比之前任何数值都明显更缓)。
	easeSeconds = 20.f;
}

void BusRoute::LayoutRoute(const vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) {
	// 之前两版都不对：第一版是通用"贪心最近邻聚类"，不知道接口贴在路的哪一侧/哪个朝向，绕法
	// 随机(PIE反馈"不是按井字中心的正方形行驶的")；第二版手推了"中山X路Start->End方向+
	// side0/side1物理位置"来判断外侧升序=逆时针、内侧降序=顺时针，但那次手推假设的是
	// "+Y=北"，后来确认这个项目实际是"+Y=南"(排查公交车行驶方向的bug时发现，见
	// Map::ComputeLaneAnchorPosition声明处的说明)——而且这种手推方式本身就很脆弱，Map层的
	// 左右手公式一旦订正，这里全部要跟着重新手推一遍，PIE实测反馈"原先靠左行驶的时候顺时针
	// 连接现在靠右行驶了需要逆时针连接"正是这个脆弱性的直接体现。
	//
	// 这次改成不依赖任何"哪个方向是南/北/顺时针/逆时针"的假设，直接从每个接口自己真实的
	// departDir反推它天生属于哪个绕行方向：一个点如果沿着"绕形心转圈"的切线方向走，它的
	// 朝向和"位置相对形心的半径向量"之间的2D叉积(radial.x*dir.y-radial.y*dir.x)符号是固定的
	// (纯数学事实，和地图哪边是南北、Y轴朝哪无关)——同号的一组自然构成同一个绕行方向，叉积
	// 符号本身就决定了这组应该按角度升序还是降序首尾相接(推导见下buildLoop调用处)。这样无论
	// Map层的左右手约定以后再怎么变，这里都会自动跟着真实车道方向重新分组，不需要再手推一遍。
	if (interfaces.size() < 4) return; // 凑不出两条至少2站的环线，留空

	float centerX = 0.f, centerY = 0.f;
	for (const RouteStationInfo& info : interfaces) { centerX += info.x; centerY += info.y; }
	centerX /= static_cast<float>(interfaces.size());
	centerY /= static_cast<float>(interfaces.size());

	auto angleAroundCenter = [&](const RouteStationInfo& info) {
		return atan2f(info.y - centerY, info.x - centerX);
	};
	// radial x departDir的2D叉积：>=0这一组的朝向和"角度增大方向"同向，必须按角度升序首尾
	// 相接才会贴着每个点自己的departDir走；<0这一组则相反，必须按角度降序——见下buildLoop。
	auto rotationSign = [&](const RouteStationInfo& info) {
		float rx = info.x - centerX, ry = info.y - centerY;
		return rx * info.departDirY - ry * info.departDirX;
	};

	vector<RouteStationInfo> groupAscending, groupDescending;
	for (const RouteStationInfo& info : interfaces) {
		(rotationSign(info) >= 0.f ? groupAscending : groupDescending).push_back(info);
	}
	sort(groupAscending.begin(), groupAscending.end(), [&](const RouteStationInfo& a, const RouteStationInfo& b) {
		return angleAroundCenter(a) < angleAroundCenter(b);
	});
	sort(groupDescending.begin(), groupDescending.end(), [&](const RouteStationInfo& a, const RouteStationInfo& b) {
		return angleAroundCenter(a) > angleAroundCenter(b);
	});

	auto buildLoop = [&](const vector<RouteStationInfo>& loopStations) {
		if (loopStations.size() < 2) return; // 单站凑不成环，丢弃

		vector<RouteStop> line;
		for (const RouteStationInfo& info : loopStations) {
			line.push_back({ info.stationIndex, info.interfaceIndex });
		}
		lines.push_back(line);

		for (size_t i = 0; i < loopStations.size(); i++) {
			const RouteStationInfo& from = loopStations[i];
			const RouteStationInfo& to = loopStations[(i + 1) % loopStations.size()];
			RouteLink link;
			link.from = { from.stationIndex, from.interfaceIndex };
			link.to = { to.stationIndex, to.interfaceIndex };
			link.bidirectional = false;
			links.push_back(link);
		}
	};
	buildLoop(groupAscending);
	buildLoop(groupDescending);
}

int TrainRoute::count = 0;

TrainRoute::TrainRoute() : id(count++) {
}

const char* TrainRoute::GetName() {
	name = "TrainRoute" + to_string(id);
	return name.data();
}

void TrainRoute::SetProperty() {
	stationType = "train";
	vehicleType = "vehicle_train";
	useRoadnet = false;
	drawPath = true;
	trackMesh = ""; // 这次不画铁轨

	// 单位换算见BusRoute::SetProperty()的注释——同样的timeFlowRatio=2.0默认值下1真实秒=
	// 120游戏内秒。火车的环线大部分长度是两段"车站到地图边缘"的长途
	// (LayoutDualTrackStationLoop，实测约2000地图单位)，按"跑一圈大约85真实秒、进站停约
	// 5真实秒"校准：speed=0.2(=24地图单位/真实秒)，dwellSeconds=600(=5真实秒)。用户要求
	// 所有公共交通速度减半，这里改成speed=0.1(=12地图单位/真实秒，跑一圈大约170真实秒)，
	// dwellSeconds不受影响。
	speed = 0.1f;
	dwellSeconds = 600.f;

	// 火车体型大，启动/刹车要更缓一点才符合实际手感——10秒/30秒PIE实测反馈"加速度还是
	// 太大"，调到40(DriveVehicle()里这个字段真实秒->游戏内秒的换算系数之前漏乘了，之前
	// 几轮调大基本没生效，见route.md"DriveVehicle的缓动"一节，这次换算修好之后效果会比
	// 之前任何数值都明显更缓)。
	easeSeconds = 40.f;
}

void TrainRoute::LayoutRoute(const vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) {
	LayoutDualTrackStationLoop(interfaces, sizeX, sizeY, false, 0.f, edgeStations, links, lines);
}

int AirRoute::count = 0;

AirRoute::AirRoute() : id(count++) {
}

const char* AirRoute::GetName() {
	name = "AirRoute" + to_string(id);
	return name.data();
}

void AirRoute::SetProperty() {
	stationType = "plane";
	vehicleType = "vehicle_plane";
	useRoadnet = false;
	drawPath = false;

	// 单位换算见BusRoute::SetProperty()的注释。飞机比火车快，环线也略长(实测约2050地图
	// 单位)，按"跑一圈大约50真实秒、停约6真实秒(登机/下客)"校准：speed=0.35(=42地图单位/
	// 真实秒)，dwellSeconds=720(=6真实秒)。用户要求所有公共交通速度减半，这里改成
	// speed=0.175(=21地图单位/真实秒，跑一圈大约100真实秒)，dwellSeconds不受影响。
	speed = 0.175f;
	dwellSeconds = 720.f;

	// 飞机比火车更大更重，启动/刹车要比火车还缓一点——15秒/40秒PIE实测反馈还是太突兀，
	// 调到50(DriveVehicle()里这个字段真实秒->游戏内秒的换算系数之前漏乘了，之前几轮调大
	// 基本没生效，见route.md"DriveVehicle的缓动"一节，这次换算修好之后效果会比之前任何
	// 数值都明显更缓；区段总时长大约是半圈减去停靠，DriveVehicle()会自动把ta/td钳在区段
	// 总时长一半以内，不会出现负的匀速段)。
	easeSeconds = 50.f;
}

void AirRoute::LayoutRoute(const vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) {
	constexpr float kCruiseHeight = 100.f; // 老工程AirRoute用的巡航高度(地图单位)
	LayoutDualTrackStationLoop(interfaces, sizeX, sizeY, true, kCruiseHeight, edgeStations, links, lines);
}
