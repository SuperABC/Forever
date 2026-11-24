#include "suites.h"

#include <cmath>
#include <filesystem>
#include <fstream>

#include "test_framework.h"
#include "world_build.h"

#include "common/json.h"
#include "common/random.h"
#include "common/registry.h"
#include "common/save_archive.h"
#include "map/building.h"
#include "map/green.h"
#include "map/map.h"
#include "map/map_save.h"
#include "map/polygon_util.h"
#include "map/room.h"
#include "populace/citizen.h"
#include "populace/populace.h"
#include "populace/populace_save.h"

using namespace std;

// green套件：绿化区(路网指定的多边形)。
//   1) polygon_util 纯函数；
//   2) 直接构造Green(green_park)：凹多边形里的最大圆、连路时圆心->边连线穿出多边形的圆被丢弃；
//   3) 用Tools/MapEditor/samples/demo.roadnet(带一块连路的绿化+一块不连路的凹多边形绿化)生成整个世界，检查圆/导航/地址/存档。
// 套件在子进程里跑(世界生成)，父进程负责把config里的roadnet_mods/terrain_mods换成demo文件。

namespace {
	const vector<float> kEll = { 0, 0, 20, 0, 20, 10, 10, 10, 10, 30, 0, 30 };             // L形(凹)
	const vector<float> kBowtie = { 10, 10, 30, 30, 30, 10, 10, 30 };                       // 自交
	// 方形大厅[0,30]^2 + 往上伸出的窄走廊[26,30]x[30,60]：最大圆在大厅，圆心->走廊顶端的连线穿出多边形。
	const vector<float> kHallway = { 0, 0, 30, 0, 30, 60, 26, 60, 26, 30, 0, 30 };

	float PolygonArea(const vector<float>& xy) { return fabsf(PolygonSignedArea(xy)); }

	void CheckPolygonUtil() {
		TestRun& run = TestRun::Get();
		// 有符号面积/规整
		vector<float> square = { 0, 0, 10, 0, 10, 10, 0, 10 };
		const float area = PolygonSignedArea(square);
		run.Check(fabsf(fabsf(area) - 100.f) < 1e-3f, "polygon signed area equals the area in magnitude", to_string(area));
		vector<float> ensured = square;
		EnsurePositiveArea(ensured);
		run.Check(PolygonSignedArea(ensured) > 0.f, "EnsurePositiveArea makes the signed area positive");
		vector<float> reversed;
		for (int i = static_cast<int>(square.size() / 2) - 1; i >= 0; i--) { reversed.push_back(square[2 * i]); reversed.push_back(square[2 * i + 1]); }
		run.Check((PolygonSignedArea(square) > 0.f) != (PolygonSignedArea(reversed) > 0.f), "reversing the vertices flips the sign");

		// 点在多边形内(凹口里面不算)
		run.Check(PointInPolygon(kEll, 5, 25), "L polygon contains a point in the tall arm");
		run.Check(!PointInPolygon(kEll, 15, 25), "L polygon does not contain a point in the notch");
		run.Check(PointInPolygon(kEll, 15, 5), "L polygon contains a point in the foot");
		run.Check(!PointInPolygon(kEll, -1, 5), "point outside the bounds is outside");

		// 线段整体在多边形内
		run.Check(SegmentInsidePolygon(kEll, 5, 5, 5, 28), "segment along the tall arm is inside");
		run.Check(!SegmentInsidePolygon(kEll, 15, 5, 5, 25), "segment across the notch corner leaves the polygon");
		run.Check(!SegmentInsidePolygon(kEll, 25, 5, 5, 25), "segment through the notch leaves the polygon");

		// 自交
		run.Check(PolygonSelfIntersects(kBowtie), "bowtie polygon self-intersects");
		run.Check(!PolygonSelfIntersects(kEll), "L polygon does not self-intersect");
		run.Check(SegmentsIntersect(0, 0, 10, 10, 0, 10, 10, 0), "crossing segments intersect");
		run.Check(!SegmentsIntersect(0, 0, 10, 0, 0, 5, 10, 5), "parallel segments do not intersect");

		// 最近点：垂足在段外取端点
		float cx = 0, cy = 0;
		float dist = ClosestPointOnSegment(0, 0, 10, 0, 5, 3, cx, cy);
		run.Check(fabsf(dist - 3.f) < 1e-4f && fabsf(cx - 5.f) < 1e-4f && fabsf(cy) < 1e-4f, "foot inside the segment is the perpendicular foot");
		dist = ClosestPointOnSegment(0, 0, 10, 0, 14, 3, cx, cy);
		run.Check(fabsf(dist - 5.f) < 1e-4f && fabsf(cx - 10.f) < 1e-4f && fabsf(cy) < 1e-4f, "foot outside the segment clamps to the end point");

		// 到边界距离
		run.Check(fabsf(DistanceToPolygonBoundary(square, 5, 4) - 4.f) < 1e-4f, "distance to polygon boundary");
		run.Check(SignedDistanceToPolygon(square, 5, 5) > 4.99f && SignedDistanceToPolygon(square, -2, 5) < -1.99f, "signed distance: inside positive, outside negative");

		// 三角化：三角形面积和 = 多边形面积(凹多边形也成立)
		for (const vector<float>* poly : { &kEll, &kHallway }) {
			vector<int> triangles;
			const bool ok = TriangulatePolygon(*poly, triangles);
			run.Check(ok && triangles.size() % 3 == 0 && !triangles.empty(), "triangulation succeeds");
			float sum = 0.f;
			for (size_t i = 0; i + 2 < triangles.size(); i += 3) {
				const int a = triangles[i], b = triangles[i + 1], c = triangles[i + 2];
				const float ax = (*poly)[2 * a], ay = (*poly)[2 * a + 1], bx = (*poly)[2 * b], by = (*poly)[2 * b + 1], ccx = (*poly)[2 * c], ccy = (*poly)[2 * c + 1];
				sum += fabsf((bx - ax) * (ccy - ay) - (ccx - ax) * (by - ay)) * 0.5f;
			}
			run.Check(fabsf(sum - PolygonArea(*poly)) < 1e-2f, "triangle areas add up to the polygon area", to_string(sum) + " vs " + to_string(PolygonArea(*poly)));
		}
		vector<int> none;
		run.Check(!TriangulatePolygon(vector<float>{ 0, 0, 1, 1 }, none), "fewer than three vertices cannot be triangulated");
	}

	// 一个独立的Green(green_park)，不进Map：用于测圆过滤。links的road随便给个非空指针(Layout不解引用它)。
	Green* LayoutGreen(const vector<float>& xy, const vector<Green::Link>& links) {
		GreenFactory& factory = Registry::Get().GetGreenFactory();
		GreenMod* mod = factory.CreateGreen("green_park");
		if (!mod) return nullptr;
		Green* green = new Green(&factory, mod);
		RandomScope scope(RandomRegistry::GetDomainStream("core.test.green"));
		vector<float> ordered = xy;
		EnsurePositiveArea(ordered);
		green->Layout(ordered, links);
		return green;
	}

	void CheckParkLayout() {
		TestRun& run = TestRun::Get();
		GreenFactory& factory = Registry::Get().GetGreenFactory();
		run.Check(factory.CheckRegistered("green_park"), "green_park is registered");
		run.Check(factory.GetPowerFree("green_park") > 0.f && factory.GetPowerLinked("green_park") > 0.f, "green_park has positive free/linked weights");
		run.Check(factory.GetPowerFree("green_not_registered") == 0.f && factory.GetPowerLinked("green_not_registered") == 0.f, "unregistered green id has zero weight");

		static char dummyRoad;
		Road* road = reinterpret_cast<Road*>(&dummyRoad);

		// 不连路：L形里放一个圆，圆在多边形内、贴不出去。
		if (Green* green = LayoutGreen(kEll, {})) {
			run.Check(green->GetCircles().size() == 1, "free L polygon gets exactly one circle", to_string(green->GetCircles().size()));
			for (const Green::Circle& c : green->GetCircles()) {
				run.Check(SignedDistanceToPolygon(green->GetPolygon(), c.x, c.y) >= c.r - 1e-3f, "circle stays inside the concave polygon");
				run.Check(c.r > 3.f, "circle radius is a good fraction of the largest inscribed circle", to_string(c.r));
				run.Check(c.centerNode == nullptr && c.edge < 0, "free green circles have no navigation node");
			}
			delete green;
		}
		// 连路，边0(底边)：圆心->底边垂足在多边形内，圆保留且选中边0。
		if (Green* green = LayoutGreen(kHallway, { { 0, road } })) {
			run.Check(green->GetCircles().size() == 1, "hall circle with the bottom edge linked is kept", to_string(green->GetCircles().size()));
			if (!green->GetCircles().empty()) {
				const Green::Circle& c = green->GetCircles()[0];
				run.Check(c.edge == 0, "circle picks the linked bottom edge", to_string(c.edge));
				run.Check(fabsf(c.footY) < 1e-3f, "circle foot lies on the bottom edge", to_string(c.footY));
			}
			delete green;
		}
		// 连路，边2(走廊顶端的帽)：圆心->该边的连线穿出多边形，圆被丢弃。
		if (Green* green = LayoutGreen(kHallway, { { 2, road } })) {
			run.Check(green->GetCircles().empty(), "circle whose connection to the linked edge leaves the polygon is dropped", to_string(green->GetCircles().size()));
			delete green;
		}
		// 不连路的走廊多边形：圆保留。
		if (Green* green = LayoutGreen(kHallway, {})) {
			run.Check(green->GetCircles().size() == 1, "free hall polygon keeps its circle");
			delete green;
		}
	}

	bool CircleInsidePolygon(const Green* green, const Green::Circle& c) {
		return SignedDistanceToPolygon(green->GetPolygon(), c.x, c.y) >= c.r - 1e-2f;
	}

	Room* FindRoomWithNode(Map& map) {
		for (const auto& [name, building] : map.GetBuildings()) {
			for (Room* room : building->GetRooms()) if (room && room->GetNavigationNode()) return room;
		}
		return nullptr;
	}

	void CheckWorldGreens(TestWorld& world) {
		TestRun& run = TestRun::Get();
		Map& map = *world.map;
		const vector<Green*>& greens = map.GetGreens();
		run.Metric("greens", static_cast<double>(greens.size()), "");
		run.Check(greens.size() == 2, "demo roadnet produces two greens", to_string(greens.size()));
		Green* linked = nullptr;
		Green* free = nullptr;
		for (Green* green : greens) (green->IsLinked() ? linked : free) = green;
		run.Check(linked != nullptr && free != nullptr, "one linked and one free green");
		for (Green* green : greens) {
			run.Check(PolygonSignedArea(green->GetPolygon()) > 0.f, "green polygon is normalised to positive signed area");
			run.Check(green->GetType() == "green_park", "green uses green_park");
			run.Check(!green->GetName().empty() && green->GetName().find(' ') == string::npos, "green name is non-empty and has no spaces", green->GetName());
			run.Check(map.GetGreen(green->GetName()) == green, "GetGreen(name) finds the green");
			const vector<Green::Circle>& circles = green->GetCircles();
			run.Check(!circles.empty(), "green has at least one activity circle", green->GetName());
			for (size_t i = 0; i < circles.size(); i++) {
				run.Check(CircleInsidePolygon(green, circles[i]), "activity circle is fully inside its polygon");
				for (size_t j = i + 1; j < circles.size(); j++) {
					const float dx = circles[i].x - circles[j].x, dy = circles[i].y - circles[j].y;
					run.Check(sqrtf(dx * dx + dy * dy) >= circles[i].r + circles[j].r - 1e-2f, "activity circles do not overlap");
				}
			}
			for (const DecorationInstance& decoration : green->GetDecorations()) {
				run.Check(PointInPolygon(green->GetPolygon(), decoration.x, decoration.y), "decoration lies inside the polygon");
			}
		}
		if (free) for (const Green::Circle& c : free->GetCircles()) run.Check(c.centerNode == nullptr, "free green circles are not on the navigation graph");
		if (!linked) return;

		// 连路绿化：圆有导航节点、地址往返。
		const vector<Green::Circle>& circles = linked->GetCircles();
		for (size_t i = 0; i < circles.size(); i++) {
			run.Check(circles[i].centerNode && circles[i].footNode, "linked green circle has centre and foot navigation nodes");
			const string address = linked->GetCircleAddress(static_cast<int>(i));
			run.Check(!address.empty() && address.find(" -1 ") != string::npos, "linked circle address contains the -1 marker", address);
			int index = -2;
			Green* found = map.LocateGreen(address, index);
			run.Check(found == linked && index == static_cast<int>(i), "LocateGreen round trips the circle address", address);
		}
		int index = -2;
		run.Check(map.LocateGreen("no_such_road -1 nothing 0", index) == nullptr, "LocateGreen rejects an unknown address");
		run.Check(map.LocateRoom(linked->GetCircleAddress(0)) == nullptr, "a green address is not a room address");

		// 导航：房间 -> 圆(最后一点是给定的圆内目标点)，圆 -> 房间。
		Room* room = FindRoomWithNode(map);
		run.Check(room != nullptr, "demo world has a room on the pedestrian graph");
		if (!room || circles.empty()) return;
		const Green::Circle& circle = circles[0];
		const float targetX = circle.x + circle.r * 0.5f, targetY = circle.y;
		Map::VehicleRoutePlan to = map.PlanPedestrianRouteToGreen(room, 0.f, 0.f, linked, 0, targetX, targetY);
		run.Check(to.status == Map::VehicleRoutePlan::OK && to.points.size() >= 2, "pedestrian route from a room to a green circle exists",
			"status " + to_string(static_cast<int>(to.status)) + ", points " + to_string(to.points.size()));
		if (to.status == Map::VehicleRoutePlan::OK && !to.points.empty()) {
			run.Check(fabsf(to.points.back().x - targetX) < 1e-3f && fabsf(to.points.back().y - targetY) < 1e-3f, "route ends at the requested point inside the circle");
			const Map::NavPoint& beforeLast = to.points[to.points.size() - 2];
			run.Check(fabsf(beforeLast.x - circle.x) < 1e-2f && fabsf(beforeLast.y - circle.y) < 1e-2f, "route passes through the circle centre before the final leg");
		}
		Map::VehicleRoutePlan from = map.PlanPedestrianRouteFromGreen(linked, 0, room);
		run.Check(from.status == Map::VehicleRoutePlan::OK && !from.points.empty(), "pedestrian route from a green circle to a room exists");
		if (from.status == Map::VehicleRoutePlan::OK && !from.points.empty()) {
			run.Check(fabsf(from.points.front().x - circle.x) < 1e-2f && fabsf(from.points.front().y - circle.y) < 1e-2f, "leaving a green starts at the circle centre");
		}
		if (free && !free->GetCircles().empty()) {
			run.Check(map.PlanPedestrianRouteToGreen(room, 0.f, 0.f, free, 0, free->GetCircles()[0].x, free->GetCircles()[0].y).status
				!= Map::VehicleRoutePlan::OK, "a free green has no pedestrian route");
		}
	}

	void CheckGreenSaveRoundTrip(TestWorld& world) {
		TestRun& run = TestRun::Get();
		SaveArchive first;
		MapSaveCodec::Save(*world.map, first);
		string error;
		Map* loaded = MapSaveCodec::Load(first, error);
		run.Check(loaded != nullptr, "map with greens loads back", error);
		if (!loaded) return;
		const vector<Green*>& a = world.map->GetGreens();
		const vector<Green*>& b = loaded->GetGreens();
		run.Check(a.size() == b.size(), "green count survives the save round trip", to_string(a.size()) + " vs " + to_string(b.size()));
		for (size_t i = 0; i < min(a.size(), b.size()); i++) {
			run.Check(a[i]->GetName() == b[i]->GetName() && a[i]->GetType() == b[i]->GetType(), "green name/type survive the round trip");
			run.Check(a[i]->GetPolygon() == b[i]->GetPolygon(), "green polygon survives the round trip");
			run.Check(a[i]->GetLinks().size() == b[i]->GetLinks().size(), "green links survive the round trip");
			run.Check(a[i]->GetDecorations().size() == b[i]->GetDecorations().size(), "green decorations survive the round trip");
			bool circlesSame = a[i]->GetCircles().size() == b[i]->GetCircles().size();
			for (size_t k = 0; circlesSame && k < a[i]->GetCircles().size(); k++) {
				const Green::Circle& ca = a[i]->GetCircles()[k];
				const Green::Circle& cb = b[i]->GetCircles()[k];
				circlesSame = ca.x == cb.x && ca.y == cb.y && ca.r == cb.r && ca.edge == cb.edge &&
					(ca.centerNode == nullptr) == (cb.centerNode == nullptr) &&
					(!ca.centerNode || ca.centerNode->GetId() == cb.centerNode->GetId());
			}
			run.Check(circlesSame, "green circles (with navigation nodes) survive the round trip");
		}
		SaveArchive second;
		MapSaveCodec::Save(*loaded, second);
		run.Check(first.Find(SAVE_SECTION_MAP_STRUCTURES) && second.Find(SAVE_SECTION_MAP_STRUCTURES) &&
			first.Find(SAVE_SECTION_MAP_STRUCTURES)->data == second.Find(SAVE_SECTION_MAP_STRUCTURES)->data, "map structures section (greens included) is byte identical after load+save");
	}

	// 市民站在/走向绿化区的圆：Core行进到达、存档往返(当前绿化+圆序号、行进目的地)。
	void CheckCitizenGreen(TestWorld& world) {
		TestRun& run = TestRun::Get();
		Map& map = *world.map;
		Green* linked = nullptr;
		for (Green* green : map.GetGreens()) if (green->IsLinked() && !green->GetCircles().empty()) linked = green;
		if (!linked) { run.Check(false, "citizen green test needs a linked green with a circle"); return; }
		const vector<Citizen*>& citizens = world.populace->GetCitizens();
		Citizen* standing = nullptr;
		Citizen* walking = nullptr;
		for (Citizen* citizen : citizens) {
			if (!citizen) continue;
			if (!standing) standing = citizen;
			else if (!walking) { walking = citizen; break; }
		}
		run.Check(standing && walking, "world has two citizens to test with");
		if (!standing || !walking) return;
		const Green::Circle& circle = linked->GetCircles()[0];

		// 站着：currentGreen与currentRoom互斥，位置保留。
		standing->SetPosition(circle.x, circle.y, 1.f);
		standing->SetCurrentGreen(linked, 0);
		run.Check(standing->GetCurrentGreen() == linked && standing->GetCurrentGreenCircle() == 0 && standing->GetCurrentRoom() == nullptr,
			"standing in a green clears the current room");
		run.Check(standing->HasPosition(), "a citizen standing in a green keeps its position");

		// 行进：出发即离开绿化；走完回到绿化区且位置是终点。
		vector<Citizen::WalkPoint> path = { { circle.x, circle.y, 1.f }, { circle.x + 1.f, circle.y, 1.f }, { circle.x + 1.f, circle.y + 1.f, 1.f } };
		walking->BeginWalkToGreen(path, linked, 0, 0.0, 1.f);
		run.Check(walking->IsWalking() && walking->GetWalkGreen() == linked && walking->GetWalkGreenCircle() == 0 && walking->GetWalkDestination() == nullptr,
			"BeginWalkToGreen walks toward the green circle");

		// 存档往返：站着的和在路上的。
		SaveArchive archive;
		MapSaveCodec::Save(map, archive);
		PopulaceSaveCodec::Save(*world.populace, map, nullptr, archive);
		string error;
		Map* map2 = MapSaveCodec::Load(archive, error);
		run.Check(map2 != nullptr, "map loads back for the citizen green check", error);
		if (map2) {
			vector<pair<JobExperience*, int>> pending;
			Populace* populace2 = PopulaceSaveCodec::Load(archive, *map2, pending, error);
			run.Check(populace2 != nullptr, "populace loads back for the citizen green check", error);
			if (populace2) {
				Citizen* standing2 = populace2->FindCitizenByName(standing->GetName());
				Citizen* walking2 = populace2->FindCitizenByName(walking->GetName());
				run.Check(standing2 && walking2, "both test citizens come back");
				if (standing2 && walking2) {
					run.Check(standing2->GetCurrentGreen() == map2->GetGreens()[0] || standing2->GetCurrentGreen() == map2->GetGreens()[1],
						"standing citizen is still in a loaded green");
					run.Check(standing2->GetCurrentGreen() && standing2->GetCurrentGreen()->GetName() == linked->GetName() && standing2->GetCurrentGreenCircle() == 0,
						"standing citizen keeps its green and circle index");
					run.Check(walking2->IsWalking() && walking2->GetWalkGreen() && walking2->GetWalkGreen()->GetName() == linked->GetName() &&
						walking2->GetWalkGreenCircle() == 0, "walking citizen keeps its green destination across the save");
				}
			}
		}

		// 走完：落在绿化区里，位置是路径终点。
		const bool arrived = walking->AdvanceWalk(1e9);
		run.Check(arrived && !walking->IsWalking() && walking->GetCurrentGreen() == linked && walking->GetCurrentGreenCircle() == 0, "finishing the walk puts the citizen in the green circle");
		float x = 0, y = 0, z = 0;
		walking->GetPosition(x, y, z);
		run.Check(walking->HasPosition() && fabsf(x - (circle.x + 1.f)) < 1e-3f && fabsf(y - (circle.y + 1.f)) < 1e-3f, "arrival keeps the end point as the position");
		// 进房间=离开绿化。
		Room* room = FindRoomWithNode(map);
		if (room) {
			walking->SetCurrentRoom(room);
			run.Check(walking->GetCurrentGreen() == nullptr, "entering a room leaves the green");
		}
	}
}

void GreenSuiteBody(TestWorld& world, const TestOptions& options) {
	(void)options;
	CheckPolygonUtil();
	CheckParkLayout();
	CheckWorldGreens(world);
	CheckGreenSaveRoundTrip(world);
	CheckCitizenGreen(world);
}

void RunGreenSuite(const TestOptions& options) {
	TestRun& run = TestRun::Get();
	run.Begin("green", 0, options.selfCommand + " --suite green");

	// demo地图：Source/../Tools/MapEditor/samples。
	filesystem::path samples = filesystem::path(options.sourceRoot).parent_path() / "Tools" / "MapEditor" / "samples";
	const filesystem::path roadnet = samples / "demo.roadnet", terrain = samples / "demo.terrain";
	if (!filesystem::exists(roadnet) || !filesystem::exists(terrain)) {
		run.Check(false, "demo map files exist", roadnet.string() + " / " + terrain.string());
		return;
	}

	// 派生config：只换路网和地形，其余照搬。
	JsonValue root;
	{
		ifstream in(options.configPath);
		JsonReader reader;
		if (!in || !reader.Parse(in, root)) {
			run.Check(false, "read base config", options.configPath);
			return;
		}
	}
	auto slashes = [](string text) { for (char& c : text) if (c == '\\') c = '/'; return text; };
	error_code ec;
	const filesystem::path dir = filesystem::path(options.outDir) / "child";
	filesystem::create_directories(dir, ec);

	// 给demo路网的两块绿化写上id：第一块指定green_park(指定路径)，第二块指定一个不存在的id(找不到就退化成按权重随机，不能被丢弃)。
	// 两块都还得是green_park(世界里只启用了这一个green mod)，CheckWorldGreens里"必须有2块绿化"同时证明了两条路径都落地了。
	filesystem::path idRoadnet = dir / "demo_green_ids.roadnet";
	{
		JsonValue rn;
		ifstream in(roadnet);
		JsonReader reader;
		if (!in || !reader.Parse(in, rn) || !rn["output"]["greens"].IsArray() || rn["output"]["greens"].size() != 2) {
			run.Check(false, "demo roadnet has two greens to tag with ids", roadnet.string());
			return;
		}
		rn["output"]["greens"][0u]["id"] = JsonValue("green_park");
		rn["output"]["greens"][1u]["id"] = JsonValue("green_not_registered");
		ofstream out(idRoadnet, ios::binary);
		StyledWriter writer;
		out << writer.Write(rn);
	}

	JsonValue roadnets(DATA_ARRAY), terrains(DATA_ARRAY);
	roadnets.append(JsonValue("roadnet_file --path=" + slashes(idRoadnet.string())));
	terrains.append(JsonValue("terrain_file --path=" + slashes(terrain.string())));
	root["roadnet_mods"] = roadnets;
	root["terrain_mods"] = terrains;
	const filesystem::path derived = dir / "config_green.json";
	{
		ofstream out(derived, ios::binary);
		StyledWriter writer;
		out << writer.Write(root);
	}
	TestOptions copy = options;
	copy.configPath = derived.string();
	RunSuiteInChildren(copy, "green", 256);
}
