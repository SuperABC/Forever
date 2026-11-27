// JunctionTest：路口通行管理(Core/traffic/junction.h)的单测。不需要Map/Registry，手工摆一个十字路口的连线，
// 用例见README.md。返回值=失败用例数。
#include "traffic/junction.h"
#include "map/general_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <random>
#include <vector>

using namespace std;

namespace {
	int failures = 0;
	int checks = 0;

	void Check(bool ok, const char* what, int line) {
		checks++;
		if (!ok) { failures++; printf("  FAIL line %d: %s\n", line, what); }
	}
#define CHECK(cond) Check((cond), #cond, __LINE__)

	using E = JunctionTraffic;

	int64_t Car(int id) { return E::MakeEntity(E::EntityKind::Vehicle, id); }
	int64_t Ped(int id) { return E::MakeEntity(E::EntityKind::Pedestrian, id); }

	// 十字路口：中心(0,0)，+x东、+y南，四条腿长1(=10m)。每条腿一个进口锚点、一个出口锚点(车道中心偏移0.1)。
	struct Cross {
		unique_ptr<Node> inW, outW, inE, outE, inN, outN, inS, outS;
		unique_ptr<Node> pw0, pw1; // 西腿两侧人行锚点(横道)
		vector<unique_ptr<Connection>> conns;
		Connection *we, *ew, *ns, *wn, *wToS, *nToE, *swCross;
		Connection* crosswalkW;
		JunctionTraffic jt;

		// approach=从第几条腿驶入(红绿灯分组用)：西0、北1、东2、南3。
		Connection* Add(Node* a, Node* b, E::Kind kind = E::Kind::Vehicle, int approach = -1) {
			conns.push_back(make_unique<Connection>(*a, *b));
			jt.AddConnection(conns.back().get(), kind, 1, approach);
			return conns.back().get();
		}
		static unique_ptr<Node> N(float x, float y) { return make_unique<Node>("t", x, y, 0.f); }

		explicit Cross(const E::Config& config = E::Config()) : jt(config) {
			inW = N(-1.f, -0.1f); outW = N(-1.f, 0.1f);
			inE = N(1.f, 0.1f);  outE = N(1.f, -0.1f);
			inN = N(0.1f, -1.f); outN = N(-0.1f, -1.f);
			inS = N(-0.1f, 1.f); outS = N(0.1f, 1.f);
			pw0 = N(-1.f, -0.5f); pw1 = N(-1.f, 0.5f);
			we = Add(inW.get(), outE.get(), E::Kind::Vehicle, 0);
			ew = Add(inE.get(), outW.get(), E::Kind::Vehicle, 2);
			ns = Add(inN.get(), outS.get(), E::Kind::Vehicle, 1);
			wn = Add(inW.get(), outN.get(), E::Kind::Vehicle, 0);
			wToS = Add(inW.get(), outS.get(), E::Kind::Vehicle, 0);
			nToE = Add(inN.get(), outE.get(), E::Kind::Vehicle, 1);
			swCross = Add(inS.get(), outN.get(), E::Kind::Vehicle, 3);
			crosswalkW = Add(pw0.get(), pw1.get(), E::Kind::Crosswalk, 0); // 横穿西腿
			jt.BuildConflicts();
		}
	};

	void TestConflictTable() {
		printf("conflict table\n");
		Cross x;
		CHECK(x.jt.AreConflicting(x.we, x.ns));   // 直行交叉
		CHECK(x.jt.AreConflicting(x.ns, x.we));   // 对称
		CHECK(!x.jt.AreConflicting(x.we, x.ew));  // 对向直行平行
		CHECK(!x.jt.AreConflicting(x.we, x.wn));  // 同一进车道分叉
		CHECK(!x.jt.AreConflicting(x.we, x.we));  // 自己
		CHECK(x.jt.AreConflicting(x.we, x.nToE)); // 汇入同一出车道
		CHECK(x.jt.AreConflicting(x.we, x.crosswalkW)); // 车行连线起点压在横道线上
		CHECK(!x.jt.AreConflicting(x.ns, x.crosswalkW)); // 南北直行不碰西腿横道
		CHECK(x.jt.FindCrosswalk(-1.f, -0.5f, -1.f, 0.5f) == x.crosswalkW);   // 市民一段路 = 横道
		CHECK(x.jt.FindCrosswalk(-1.f, 0.5f, -1.f, -0.5f) == x.crosswalkW);   // 反向也认
		CHECK(x.jt.FindCrosswalk(-1.f, -0.5f, -1.f, 0.9f) == nullptr);        // 终点对不上
		CHECK(x.jt.FindCrosswalk(0.f, 0.f, 3.f, 3.f) == nullptr);
	}

	void TestMutualExclusion() {
		printf("mutual exclusion + sticky + starvation\n");
		Cross x;
		double t = 0.0;
		CHECK(x.jt.Request(x.we, Car(1), t, 0.2f, 10.f));
		CHECK(!x.jt.Request(x.ns, Car(2), t + 0.05, 0.2f, 10.f)); // 交叉方等
		// 同连线后车连续放行，冲突方车辆饿死(设计如此)
		for (int i = 0; i < 20; i++) {
			t += 0.1;
			CHECK(x.jt.Request(x.we, Car(1), t, 0.2f, 10.f));
			if (i % 2 == 0) CHECK(x.jt.Request(x.we, Car(10 + i), t, 0.2f, 10.f));
			CHECK(!x.jt.Request(x.ns, Car(2), t, 0.2f, 10.f));
			CHECK(x.jt.CheckInvariant(t));
		}
		// 全部主动Leave之后，交叉方能进
		x.jt.Leave(x.we, Car(1));
		for (int i = 0; i < 20; i += 2) x.jt.Leave(x.we, Car(10 + i));
		CHECK(x.jt.Request(x.ns, Car(2), t + 0.1, 0.2f, 10.f));
		CHECK(!x.jt.Request(x.we, Car(30), t + 0.1, 0.2f, 10.f)); // 反过来轮到另一方等
		CHECK(x.jt.CheckInvariant(t + 0.1));
	}

	void TestFirstComeFirstServed() {
		printf("first come first served\n");
		Cross x;
		CHECK(x.jt.Request(x.wn, Car(5), 0.0, 0.2f, 10.f)); // 空路口直接进
		x.jt.Leave(x.wn, Car(5));
		// 两辆车几乎同时到交叉方向：先请求的先走，后请求的即使连线空着也要等
		CHECK(x.jt.Request(x.we, Car(1), 1.00, 0.2f, 10.f));
		x.jt.Leave(x.we, Car(1));
		// 构造"先来的在等"：先让一辆车占着we，ns的车先开始排队，之后we放空，ns应该比后来的we先走
		CHECK(x.jt.Request(x.we, Car(1), 2.0, 0.2f, 10.f));
		CHECK(!x.jt.Request(x.ns, Car(2), 2.1, 0.2f, 10.f)); // t0=2.1
		x.jt.Leave(x.we, Car(1));
		CHECK(!x.jt.Request(x.we, Car(3), 2.3, 0.2f, 10.f));  // 更晚的we车被更早排队的ns车挡住
		CHECK(x.jt.Request(x.ns, Car(2), 2.4, 0.2f, 10.f));   // ns车进
		CHECK(!x.jt.Request(x.we, Car(3), 2.5, 0.2f, 10.f));
		CHECK(x.jt.CheckInvariant(2.5));
	}

	void TestPedestrianPriority() {
		printf("pedestrian priority\n");
		Cross x;
		CHECK(x.jt.Request(x.we, Car(1), 0.0, 0.2f, 10.f));
		CHECK(x.jt.Request(x.we, Car(1), 0.5, 0.2f, 10.f));
		CHECK(!x.jt.Request(x.crosswalkW, Ped(1), 1.0, 0.f, 1e9f)); // 人等，t0=1.0
		CHECK(x.jt.Request(x.we, Car(1), 1.0, 0.2f, 10.f));
		CHECK(!x.jt.Request(x.crosswalkW, Ped(1), 1.5, 0.f, 1e9f));
		CHECK(x.jt.Request(x.we, Car(1), 1.5, 0.2f, 10.f));
		// 连线已开，但有更早在等的冲突方的人 -> 新来的车不放行
		CHECK(!x.jt.Request(x.we, Car(3), 2.0, 0.2f, 10.f));
		// 已持有的车继续续租(粘性)
		CHECK(x.jt.Request(x.we, Car(1), 2.0, 0.2f, 10.f));
		CHECK(!x.jt.Request(x.crosswalkW, Ped(1), 2.0, 0.f, 1e9f));
		// 车走了，人过
		x.jt.Leave(x.we, Car(1));
		CHECK(x.jt.Request(x.crosswalkW, Ped(1), 2.1, 0.f, 1e9f));
		CHECK(!x.jt.Request(x.we, Car(3), 2.2, 0.2f, 10.f)); // 人在横道上，车等
		// 比车更晚到的人：连线开着的横道直接放行
		CHECK(x.jt.Request(x.crosswalkW, Ped(2), 2.3, 0.f, 1e9f));
		x.jt.Leave(x.crosswalkW, Ped(1));
		x.jt.Leave(x.crosswalkW, Ped(2));
		CHECK(x.jt.Request(x.we, Car(3), 2.4, 0.2f, 10.f));
		CHECK(x.jt.CheckInvariant(2.4));
	}

	void TestLease() {
		printf("lease expiry\n");
		Cross x;
		CHECK(x.jt.Request(x.we, Car(1), 0.0, 0.2f, 10.f));
		CHECK(!x.jt.Request(x.ns, Car(2), 0.5, 0.2f, 10.f));
		CHECK(!x.jt.Request(x.ns, Car(2), 0.9, 0.2f, 10.f)); // Car1租约到1.0
		CHECK(x.jt.Request(x.ns, Car(2), 1.2, 0.2f, 10.f));  // 租约过期，释放
		CHECK(x.jt.CheckInvariant(1.2));
		// 排队者不再请求，等待记录也过期，不会永远挡路
		Cross y;
		CHECK(y.jt.Request(y.we, Car(1), 0.0, 0.2f, 10.f));
		CHECK(!y.jt.Request(y.ns, Car(2), 0.5, 0.2f, 10.f));
		y.jt.Leave(y.we, Car(1));
		CHECK(y.jt.Request(y.we, Car(3), 2.0, 0.2f, 10.f)); // Car2在0.5后没再请求，1.5过期
	}

	void TestExitCheck() {
		printf("exit check + reservation\n");
		Cross x;
		CHECK(!x.jt.Request(x.we, Car(1), 0.0, 0.4f, 0.3f)); // 出口放不下
		CHECK(x.jt.Request(x.we, Car(1), 0.1, 0.4f, 1.0f));  // 放得下
		CHECK(x.jt.Request(x.we, Car(2), 0.1, 0.4f, 1.0f));  // 预留0.4，剩0.6>=0.4
		CHECK(!x.jt.Request(x.we, Car(3), 0.1, 0.4f, 1.0f)); // 预留0.8，剩0.2<0.4
		x.jt.Leave(x.we, Car(1));
		CHECK(x.jt.Request(x.we, Car(3), 0.2, 0.4f, 1.0f));  // 预留释放
		// 汇入同一出口的另一连线也计入预留(同一出口锚点)
		Cross y;
		CHECK(y.jt.Request(y.we, Car(1), 0.0, 0.6f, 1.0f));
		CHECK(!y.jt.Request(y.nToE, Car(2), 0.1, 0.6f, 1.0f)); // 冲突+出口都不过
	}

	void TestMaxOpen() {
		printf("max open time\n");
		E::Config config;
		config.maxOpenSeconds = 2.f;
		Cross x(config);
		CHECK(x.jt.Request(x.we, Car(1), 0.0, 0.2f, 10.f));
		CHECK(!x.jt.Request(x.ns, Car(2), 0.1, 0.2f, 10.f));
		bool stopped = false;
		for (double t = 0.1; t < 5.0; t += 0.1) {
			x.jt.Request(x.we, Car(1), t, 0.2f, 10.f);
			stopped |= !x.jt.Request(x.we, Car(100 + static_cast<int>(t * 10)), t, 0.2f, 10.f);
			x.jt.Request(x.ns, Car(2), t, 0.2f, 10.f);
		}
		CHECK(stopped); // 超过2秒后不再放新车进we
	}

	// 红绿灯分组：每次挑夹角最接近180°的两条路成一组，剩一条单独一组；没有驶入车道的路不分组。
	void TestSignalPairing() {
		printf("signal pairing\n");
		const float pi = 3.14159265f;
		auto deg = [pi](float d) { return d * pi / 180.f; };
		// 十字(离开路口方向：西pi、北-pi/2、东0、南pi/2)：西东一组、北南一组。
		vector<int> cross = E::PairApproaches({ pi, -pi / 2, 0.f, pi / 2 }, { true, true, true, true });
		CHECK(cross[0] == cross[2] && cross[1] == cross[3] && cross[0] != cross[1]);
		// T字：西东直的两条一组，南单独一组。
		vector<int> tee = E::PairApproaches({ pi, 0.f, pi / 2 }, { true, true, true });
		CHECK(tee[0] == tee[1] && tee[2] != tee[0] && tee[2] >= 0);
		// 五岔(每72°一条)：两对加一条单独，一共三组。
		vector<int> five = E::PairApproaches({ deg(0), deg(72), deg(144), deg(216), deg(288) }, { true, true, true, true, true });
		int groups = 0;
		for (int g : five) groups = max(groups, g + 1);
		vector<int> sizes(groups, 0);
		for (int g : five) if (g >= 0) sizes[g]++;
		CHECK(groups == 3);
		CHECK(count(sizes.begin(), sizes.end(), 2) == 2 && count(sizes.begin(), sizes.end(), 1) == 1);
		// 歪一点的十字(80°和100°)：仍然配成两条"最直"的。
		vector<int> skew = E::PairApproaches({ deg(180), deg(-80), deg(0), deg(100) }, { true, true, true, true });
		CHECK(skew[0] == skew[2] && skew[1] == skew[3]);
		// 只出不进的单行道不分组。
		vector<int> oneWay = E::PairApproaches({ pi, -pi / 2, 0.f, pi / 2 }, { true, false, true, true });
		CHECK(oneWay[1] == -1 && oneWay[0] == oneWay[2] && oneWay[3] >= 0 && oneWay[3] != oneWay[0]);
	}

	// 红绿灯放行：激活后各组轮流绿灯(绿15秒、黄3秒)；红/黄灯不放新车、被拒的车不进排队；已放行的续租照常；
	// 换组时上一组还在路口里的车挡住新组的冲突方；没激活=当作没有红绿灯。
	void TestSignalRequest() {
		printf("signal request\n");
		const double G = E::kGreenSeconds, Y = E::kYellowSeconds;
		Cross x;
		x.jt.SetSignalGroups(1, { 0, 1, 0, 1 }); // 西东一组、北南一组
		CHECK(x.jt.HasSignal(1) && !x.jt.IsSignalActive(1));
		CHECK(x.jt.GetSignal(1, 0, 0.0) == E::Signal::None);
		// 没激活：和没有红绿灯一样
		CHECK(x.jt.Request(x.ns, Car(1), 0.0, 0.2f, 10.f));
		x.jt.Leave(x.ns, Car(1));

		x.jt.SetSignalActive(1, true, 100.0);
		CHECK(x.jt.GetSignal(1, 0, 101.0) == E::Signal::Green && x.jt.GetSignal(1, 2, 101.0) == E::Signal::Green);
		CHECK(x.jt.GetSignal(1, 1, 101.0) == E::Signal::Red && x.jt.GetSignal(1, 3, 101.0) == E::Signal::Red);
		CHECK(x.jt.GetSignal(1, 0, 100.0 + G + 1.0) == E::Signal::Yellow);
		CHECK(x.jt.GetSignal(1, 1, 100.0 + G + Y + 1.0) == E::Signal::Green && x.jt.GetSignal(1, 0, 100.0 + G + Y + 1.0) == E::Signal::Red);
		CHECK(x.jt.GetSignal(1, 0, 100.0 + 2 * (G + Y) + 1.0) == E::Signal::Green); // 一个周期后回到第0组

		double t = 101.0;
		CHECK(!x.jt.Request(x.ns, Car(2), t, 0.2f, 10.f));         // 北是红灯
		CHECK(x.jt.Request(x.we, Car(3), t + 0.1, 0.2f, 10.f));    // 西是绿灯
		x.jt.Leave(x.we, Car(3));
		// 红灯车没进排队：之后来的绿灯冲突车不会被它挡住
		CHECK(x.jt.Request(x.we, Car(4), t + 0.2, 0.2f, 10.f));
		CHECK(x.jt.Request(x.ew, Car(5), t + 0.2, 0.2f, 10.f));    // 同组对向直行，不冲突
		// 黄灯：不放新车，已放行的续租(租约1秒，下面每0.5秒续一次)
		t = 100.0 + G - 0.5;
		CHECK(x.jt.Request(x.we, Car(4), t, 0.2f, 10.f));
		CHECK(x.jt.Request(x.ew, Car(5), t, 0.2f, 10.f));
		t = 100.0 + G + 0.3;
		CHECK(x.jt.Request(x.we, Car(4), t, 0.2f, 10.f));
		CHECK(!x.jt.Request(x.we, Car(6), t, 0.2f, 10.f));
		// 换到北南组：上一组还在路口里的车(Car4续租着)挡住冲突的北进车；它走了才放
		for (; t < 100.0 + G + Y + 0.5; t += 0.5) {
			CHECK(x.jt.Request(x.we, Car(4), t, 0.2f, 10.f));
			CHECK(x.jt.Request(x.ew, Car(5), t, 0.2f, 10.f));
		}
		CHECK(x.jt.Request(x.we, Car(4), t, 0.2f, 10.f));
		CHECK(!x.jt.Request(x.ns, Car(2), t, 0.2f, 10.f));
		CHECK(x.jt.CheckInvariant(t));
		x.jt.Leave(x.we, Car(4));
		x.jt.Leave(x.ew, Car(5));
		CHECK(x.jt.Request(x.ns, Car(2), t + 0.1, 0.2f, 10.f));
		CHECK(!x.jt.Request(x.we, Car(7), t + 0.1, 0.2f, 10.f));   // 西现在是红灯
		// 西腿红灯：行人可以横穿西腿(细则见TestSignalPedestrian)
		x.jt.Leave(x.ns, Car(2));
		CHECK(x.jt.Request(x.crosswalkW, Ped(1), t + 0.2, 0.f, 1e9f));
		x.jt.Leave(x.crosswalkW, Ped(1));
		// 关掉：马上当作没有红绿灯；再激活从第0组重新开始
		x.jt.SetSignalActive(1, false, t + 0.3);
		CHECK(x.jt.Request(x.we, Car(7), t + 0.3, 0.2f, 10.f));
		x.jt.Leave(x.we, Car(7));
		x.jt.SetSignalActive(1, true, 500.0);
		CHECK(x.jt.GetSignal(1, 0, 500.5) == E::Signal::Green && x.jt.GetSignal(1, 1, 500.5) == E::Signal::Red);
		// 没给道路的连线(approach=-1)和没有信号表的路口不受影响
		Cross y;
		CHECK(!y.jt.HasSignal(1));
		y.jt.SetSignalActive(1, true, 0.0);
		CHECK(!y.jt.IsSignalActive(1) && y.jt.Request(y.ns, Car(1), 1.0, 0.2f, 10.f));
	}

	// 行人按灯：只有横穿的那条路是红灯才放；绿/黄灯被拒且不进排队(不挡车)；已经在横道上的人续租走完，
	// 而且挡住刚变绿的那条路的车；转角和没激活的路口不受影响。
	void TestSignalPedestrian() {
		printf("signal pedestrian\n");
		const double G = E::kGreenSeconds, Y = E::kYellowSeconds;
		Cross x;
		x.jt.SetSignalGroups(1, { 0, 1, 0, 1 }); // 西东一组、北南一组
		// 没激活：和没有红绿灯一样
		CHECK(x.jt.Request(x.crosswalkW, Ped(1), 0.0, 0.f, 1e9f));
		x.jt.Leave(x.crosswalkW, Ped(1));

		x.jt.SetSignalActive(1, true, 100.0);
		double t = 101.0; // 西东绿
		CHECK(!x.jt.Request(x.crosswalkW, Ped(1), t, 0.f, 1e9f));        // 西腿绿灯，人不能过
		CHECK(x.jt.Request(x.we, Car(1), t + 0.1, 0.2f, 10.f));          // 被拒的人没进排队，不挡西进的车
		x.jt.Leave(x.we, Car(1));
		CHECK(!x.jt.Request(x.crosswalkW, Ped(1), 100.0 + G + 1.0, 0.f, 1e9f)); // 黄灯也不能过
		t = 100.0 + G + Y + 1.0; // 北南绿，西腿红
		CHECK(x.jt.Request(x.crosswalkW, Ped(1), t, 0.f, 1e9f));
		// 已经在横道上：一直续租到西腿变回绿灯之后也照常
		for (; t < 100.0 + 2 * (G + Y) + 1.0; t += 0.5) CHECK(x.jt.Request(x.crosswalkW, Ped(1), t, 0.f, 1e9f));
		CHECK(x.jt.GetSignal(1, 0, t) == E::Signal::Green);
		CHECK(!x.jt.Request(x.crosswalkW, Ped(2), t, 0.f, 1e9f));        // 新来的人不放
		CHECK(!x.jt.Request(x.we, Car(2), t, 0.2f, 10.f));              // 西腿绿了，但人还在横道上，车等
		x.jt.Leave(x.crosswalkW, Ped(1));
		CHECK(x.jt.Request(x.we, Car(2), t + 0.1, 0.2f, 10.f));
		CHECK(x.jt.CheckInvariant(t + 0.1));
		// 转角不受红绿灯管(即使传了道路下标也忽略)
		Cross y;
		y.jt.SetSignalGroups(1, { 0, 1, 0, 1 });
		y.jt.SetSignalActive(1, true, 0.0);
		Connection* yCorner = y.Add(y.pw1.get(), y.pw0.get(), E::Kind::Corner, 0);
		y.jt.BuildConflicts();
		CHECK(y.jt.Request(yCorner, Ped(3), 1.0, 0.f, 1e9f)); // 西腿绿灯时转角照走
	}

	// 路径点找路口连线：起点(驶入锚点)和终点(驶出锚点)先后出现在路径里；同一驶入车道的几个转向按终点区分。
	void TestFindVehicleSpans() {
		printf("find vehicle spans\n");
		Cross x;
		vector<pair<float, float>> straight = { { -3.f, -0.1f }, { -2.f, -0.1f }, { -1.f, -0.1f }, { 0.f, -0.1f }, { 1.f, -0.1f }, { 2.f, -0.1f } };
		vector<E::PathSpan> a = x.jt.FindVehicleSpans(straight);
		CHECK(a.size() == 1 && a[0].connection == x.we && a[0].fromIndex == 2 && a[0].toIndex == 4);
		vector<pair<float, float>> left = { { -2.f, -0.1f }, { -1.f, -0.1f }, { -0.5f, -0.5f }, { -0.1f, -1.f }, { -0.1f, -2.f } };
		vector<E::PathSpan> b = x.jt.FindVehicleSpans(left);
		CHECK(b.size() == 1 && b[0].connection == x.wn && b[0].fromIndex == 1 && b[0].toIndex == 3);
		// 起点后面没出现任何终点(路径在路口里截断了)：不算
		vector<pair<float, float>> cut = { { -2.f, -0.1f }, { -1.f, -0.1f }, { -0.5f, -0.1f } };
		CHECK(x.jt.FindVehicleSpans(cut).empty());
		// 不经过路口
		CHECK(x.jt.FindVehicleSpans({ { 5.f, 5.f }, { 6.f, 6.f } }).empty());
	}

	// 随机压力：很多实体随机请求/离开/消失，每步检查不变量，最后停止请求看能否全部排空。
	void TestRandomStress() {
		printf("random stress\n");
		Cross x;
		mt19937 rng(12345);
		vector<Connection*> vehicleConns = { x.we, x.ew, x.ns, x.wn, x.wToS, x.nToE, x.swCross };
		struct Agent { int64_t id; Connection* conn; bool ped; };
		vector<Agent> agents;
		for (int i = 0; i < 16; i++) agents.push_back({ Car(i), vehicleConns[rng() % vehicleConns.size()], false });
		for (int i = 0; i < 6; i++) agents.push_back({ Ped(i), x.crosswalkW, true });
		bool ok = true;
		double t = 0.0;
		for (int step = 0; step < 4000 && ok; step++) {
			t += 0.1;
			for (Agent& a : agents) {
				int r = static_cast<int>(rng() % 100);
				if (x.jt.Holds(a.conn, a.id)) {
					if (r < 25) { x.jt.Leave(a.conn, a.id); if (!a.ped) a.conn = vehicleConns[rng() % vehicleConns.size()]; }
					else if (r < 28) { /* 消失：不Leave也不再续租，靠租约过期 */ }
					else x.jt.Request(a.conn, a.id, t, 0.2f, 5.f);
				}
				else if (r < 70) {
					x.jt.Request(a.conn, a.id, t, a.ped ? 0.f : 0.2f, a.ped ? 1e9f : 5.f);
				}
			}
			ok = x.jt.CheckInvariant(t);
		}
		CHECK(ok);
		// 排空：停止所有请求，只推进时间
		for (Agent& a : agents) x.jt.Leave(a.conn, a.id);
		bool allFree = true;
		for (Agent& a : agents) allFree &= !x.jt.Holds(a.conn, a.id);
		CHECK(allFree);
		CHECK(x.jt.Request(x.we, Car(99), t + 5.0, 0.2f, 5.f));
	}
}

int main() {
	TestConflictTable();
	TestMutualExclusion();
	TestFirstComeFirstServed();
	TestPedestrianPriority();
	TestLease();
	TestExitCheck();
	TestMaxOpen();
	TestSignalPairing();
	TestSignalRequest();
	TestSignalPedestrian();
	TestFindVehicleSpans();
	TestRandomStress();
	printf("\n%d checks, %d failures\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
