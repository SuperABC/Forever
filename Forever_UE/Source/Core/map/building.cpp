#include "building.h"

#include "common/error.h"
#include "common/json.h"

#include "map/room.h"
#include "map/component.h"
#include "map/zone.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>


using namespace std;

namespace {
	constexpr float kDefaultFloorHeight = 0.4f; // 老工程Hotel同款兜底值，长度对不上时用它补齐
	constexpr int kRectParamCount = 8;
	constexpr int kPointParamCount = 4;

	// 照抄老工程Stair/Elevator/.../Hatch::InstanciateQuad共用的换算公式：两个对角点各自的
	// ratio*边长+offset算出绝对坐标，中心点=两点平均，尺寸=两点差的绝对值。
	void InstanciateRect(const RectParams& params, float width, float height,
		float& outPosX, float& outPosY, float& outSizeX, float& outSizeY) {
		float x1 = params[0] * width + params[1];
		float y1 = params[2] * height + params[3];
		float x2 = params[4] * width + params[5];
		float y2 = params[6] * height + params[7];
		outPosX = (x1 + x2) / 2.f;
		outPosY = (y1 + y2) / 2.f;
		outSizeX = fabsf(x2 - x1);
		outSizeY = fabsf(y2 - y1);
	}
}

// ===== Stair / Elevator / Ramp =====

Stair::Stair(RectParams params) : params(params) {}
int Stair::GetDirection() const { return direction; }
void Stair::SetDirection(int dir) { direction = dir; }
bool Stair::GetWall(int dir) const {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Stair wall direction out of range [0,3].\n");
	return walls[dir];
}
void Stair::AddWall(int dir) {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Stair wall direction out of range [0,3].\n");
	walls[dir] = true;
}
void Stair::InstanciateQuad(float width, float height) {
	float x, y, w, h;
	InstanciateRect(params, width, height, x, y, w, h);
	Quad::SetPosition(x, y, w, h);
}

Elevator::Elevator(RectParams params) : params(params) {}
int Elevator::GetDirection() const { return direction; }
void Elevator::SetDirection(int dir) { direction = dir; }
bool Elevator::GetWall(int dir) const {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Elevator wall direction out of range [0,3].\n");
	return walls[dir];
}
void Elevator::AddWall(int dir) {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Elevator wall direction out of range [0,3].\n");
	walls[dir] = true;
}
void Elevator::InstanciateQuad(float width, float height) {
	float x, y, w, h;
	InstanciateRect(params, width, height, x, y, w, h);
	Quad::SetPosition(x, y, w, h);
}

Ramp::Ramp(RectParams params) : params(params) {}
int Ramp::GetDirection() const { return direction; }
void Ramp::SetDirection(int dir) { direction = dir; }
bool Ramp::GetWall(int dir) const {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Ramp wall direction out of range [0,3].\n");
	return walls[dir];
}
void Ramp::AddWall(int dir) {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Ramp wall direction out of range [0,3].\n");
	walls[dir] = true;
}
void Ramp::InstanciateQuad(float width, float height) {
	float x, y, w, h;
	InstanciateRect(params, width, height, x, y, w, h);
	Quad::SetPosition(x, y, w, h);
}

// ===== Ceiling / Ground / Hatch =====

Ceiling::Ceiling(RectParams params) : params(params) {}
void Ceiling::InstanciateQuad(float width, float height) {
	float x, y, w, h;
	InstanciateRect(params, width, height, x, y, w, h);
	Quad::SetPosition(x, y, w, h);
}

Ground::Ground(RectParams params) : params(params) {}
void Ground::InstanciateQuad(float width, float height) {
	float x, y, w, h;
	InstanciateRect(params, width, height, x, y, w, h);
	Quad::SetPosition(x, y, w, h);
}

Hatch::Hatch(RectParams params) : params(params) {}
void Hatch::InstanciateQuad(float width, float height) {
	float x, y, w, h;
	InstanciateRect(params, width, height, x, y, w, h);
	Quad::SetPosition(x, y, w, h);
}

// ===== Corridor / Single / Row =====

Corridor::Corridor(RectParams params) : params(params) {}
bool Corridor::GetWall(int dir) const {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Corridor wall direction out of range [0,3].\n");
	return walls[dir];
}
void Corridor::AddWall(int dir) {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Corridor wall direction out of range [0,3].\n");
	walls[dir] = true;
}
const WallHole& Corridor::GetDoors() const { return doors; }
void Corridor::AddDoor(int dir, vector<RectParams> positions) {
	for (auto& p : positions) doors[dir].push_back(p);
}
const WallHole& Corridor::GetWindows() const { return windows; }
void Corridor::AddWindow(int dir, vector<RectParams> positions) {
	for (auto& p : positions) windows[dir].push_back(p);
}
void Corridor::InstanciateQuad(float width, float height) {
	float x, y, w, h;
	InstanciateRect(params, width, height, x, y, w, h);
	Quad::SetPosition(x, y, w, h);
}

Single::Single(RectParams params) : params(params) {}
int Single::GetDirection() const { return direction; }
void Single::SetDirection(int dir) { direction = dir; }
const WallHole& Single::GetDoors() const { return doors; }
void Single::AddDoor(int dir, vector<RectParams> positions) {
	for (auto& p : positions) doors[dir].push_back(p);
}
const WallHole& Single::GetWindows() const { return windows; }
void Single::AddWindow(int dir, vector<RectParams> positions) {
	for (auto& p : positions) windows[dir].push_back(p);
}
void Single::InstanciateQuad(float width, float height) {
	float x, y, w, h;
	InstanciateRect(params, width, height, x, y, w, h);
	Quad::SetPosition(x, y, w, h);
}

Row::Row(RectParams params) : params(params) {}
int Row::GetDirection() const { return direction; }
void Row::SetDirection(int dir) { direction = dir; }
const WallHole& Row::GetDoors() const { return doors; }
void Row::AddDoor(int dir, vector<RectParams> positions) {
	for (auto& p : positions) doors[dir].push_back(p);
}
const WallHole& Row::GetWindows() const { return windows; }
void Row::AddWindow(int dir, vector<RectParams> positions) {
	for (auto& p : positions) windows[dir].push_back(p);
}
void Row::InstanciateQuad(float width, float height) {
	float x, y, w, h;
	InstanciateRect(params, width, height, x, y, w, h);
	Quad::SetPosition(x, y, w, h);
}

// ===== Floor =====

Floor::Floor(int level, float width, float height) : Quad(), level(level) {
	SetVertices(0.f, 0.f, width, height);
}
int Floor::GetLevel() const { return level; }
const vector<Stair>& Floor::GetStairs() const { return stairs; }
const vector<Elevator>& Floor::GetElevators() const { return elevators; }
const vector<Ramp>& Floor::GetRamps() const { return ramps; }
const vector<Ceiling>& Floor::GetCeilings() const { return ceilings; }
const vector<Ground>& Floor::GetGrounds() const { return grounds; }
const vector<Corridor>& Floor::GetCorridors() const { return corridors; }
const vector<Single>& Floor::GetSingles() const { return singles; }
const vector<Row>& Floor::GetRows() const { return rows; }
const vector<Hatch>& Floor::GetHatches() const { return hatches; }
void Floor::AddStair(Stair stair) { stairs.push_back(std::move(stair)); }
void Floor::AddElevator(Elevator elevator) { elevators.push_back(std::move(elevator)); }
void Floor::AddRamp(Ramp ramp) { ramps.push_back(std::move(ramp)); }
void Floor::AddCeiling(Ceiling ceiling) { ceilings.push_back(std::move(ceiling)); }
void Floor::AddGround(Ground ground) { grounds.push_back(std::move(ground)); }
void Floor::AddCorridor(Corridor corridor) { corridors.push_back(std::move(corridor)); }
void Floor::AddSingle(Single single) { singles.push_back(std::move(single)); }
void Floor::AddRow(Row row) { rows.push_back(std::move(row)); }
void Floor::AddHatch(Hatch hatch) { hatches.push_back(std::move(hatch)); }
int Floor::AssignNumber() { return number++; }

// ===== BuildingLayoutLibrary =====

RectParams BuildingLayoutLibrary::InverseParams(const RectParams& params, int face) {
	if (face < 0 || face >= 4) THROW_EXCEPTION(InvalidArgumentException, "Facing direction out of range [0,3].\n");
	RectParams inversed = params;
	switch (face) {
	case 0:
		inversed[0] = params[2];
		inversed[1] = params[3];
		inversed[2] = 1.f - params[4];
		inversed[3] = -params[5];
		inversed[4] = params[6];
		inversed[5] = params[7];
		inversed[6] = 1.f - params[0];
		inversed[7] = -params[1];
		break;
	case 1:
		inversed[0] = 1.f - params[6];
		inversed[1] = -params[7];
		inversed[2] = params[0];
		inversed[3] = params[1];
		inversed[4] = 1.f - params[2];
		inversed[5] = -params[3];
		inversed[6] = params[4];
		inversed[7] = params[5];
		break;
	case 2:
		break;
	case 3:
		inversed[0] = 1.f - params[4];
		inversed[1] = -params[5];
		inversed[2] = 1.f - params[6];
		inversed[3] = -params[7];
		inversed[4] = 1.f - params[0];
		inversed[5] = -params[1];
		inversed[6] = 1.f - params[2];
		inversed[7] = -params[3];
		break;
	}
	return inversed;
}

int BuildingLayoutLibrary::InverseDirection(int direction, int face) {
	if (face < 0 || face >= 4) THROW_EXCEPTION(InvalidArgumentException, "Facing direction out of range [0,3].\n");
	switch (face) {
	case 0:
		if (direction >= 2) return direction - 2;
		else return 3 - direction;
	case 1:
		if (direction >= 2) return 3 - direction;
		else return direction + 2;
	case 2:
		return direction;
	case 3:
		return (5 - direction) % 4;
	default:
		return direction;
	}
}

PointParams BuildingLayoutLibrary::InversePoint(const PointParams& point, int face) {
	if (face < 0 || face >= 4) THROW_EXCEPTION(InvalidArgumentException, "Facing direction out of range [0,3].\n");
	switch (face) {
	case 0: return { point[2], point[3], 1.f - point[0], -point[1] };
	case 1: return { 1.f - point[2], -point[3], point[0], point[1] };
	case 2: return point;
	case 3: return { 1.f - point[0], -point[1], 1.f - point[2], -point[3] };
	default: return point;
	}
}

RectParams BuildingLayoutLibrary::InverseWall(const RectParams& pos, int direction, int face) {
	// face=0(逆时针90度+x翻转)：翻转水平墙(NORTH=2,SOUTH=3)；face=1(顺时针90度+x翻转)：
	// 翻转竖直墙(WEST=0,EAST=1)；face=2(不变)：不翻转；face=3(180度)：全部翻转——照抄
	// 老工程注释与实现。
	bool flip = false;
	switch (face) {
	case 0: flip = (direction == 2 || direction == 3); break;
	case 1: flip = (direction == 0 || direction == 1); break;
	case 3: flip = true; break;
	}
	if (!flip) return pos;
	return {
		1.f - pos[4], -pos[5], pos[2], pos[3],
		1.f - pos[0], -pos[1], pos[6], pos[7],
	};
}

namespace {
	RectParams ReadRectParams(const JsonValue& arr) {
		if (arr.size() != kRectParamCount) {
			THROW_EXCEPTION(JsonFormatException, "Layout rect must have 8 floats.\n");
		}
		RectParams params{};
		for (int i = 0; i < kRectParamCount; i++) params[i] = arr[i].AsFloat();
		return params;
	}

	PointParams ReadPointParams(const JsonValue& arr) {
		if (arr.size() != kPointParamCount) {
			THROW_EXCEPTION(JsonFormatException, "Layout point must have 4 floats.\n");
		}
		PointParams params{};
		for (int i = 0; i < kPointParamCount; i++) params[i] = arr[i].AsFloat();
		return params;
	}

	// 解析一个navigation/pedestrianNavigation/vehicleNavigation块，产出4个朝向各一份的
	// NavigationTemplate——照抄老工程Building::ReadTemplates对应段落。navRoot为空(节点
	// 不存在，比如vehicleNavigation这次多数模板都没画)时4份都是默认构造的空NavigationTemplate。
	void ParseNavigationBlock(const JsonValue& navRoot, array<NavigationTemplate, 4>& out) {
		vector<PointParams> rawNodes;
		for (auto& n : navRoot["nodes"]) {
			rawNodes.push_back(ReadPointParams(n["position"]));
		}

		vector<pair<PointParams, PointParams>> rawLines;
		for (auto& l : navRoot["lines"]) {
			rawLines.emplace_back(ReadPointParams(l["begin"]), ReadPointParams(l["end"]));
		}

		auto readEndpoint = [](const JsonValue& e) -> NavigationEndpointTemplate {
			NavigationEndpointTemplate endpoint;
			endpoint.type = e["type"].AsString();
			if (e.ValidMember("idx")) endpoint.idx = e["idx"].AsInt();
			if (e.ValidMember("vertex")) endpoint.vertex = e["vertex"].AsInt();
			return endpoint;
			};

		vector<NavigationConnectionTemplate> rawConnections;
		for (auto& c : navRoot["connections"]) {
			NavigationConnectionTemplate connTemplate;
			connTemplate.begin = readEndpoint(c["begin"]);
			connTemplate.end = readEndpoint(c["end"]);
			rawConnections.push_back(connTemplate);
		}

		for (int i = 0; i < 4; i++) {
			NavigationTemplate navTemplate;
			for (auto& position : rawNodes) {
				NavigationNodeTemplate nodeTemplate;
				nodeTemplate.position = BuildingLayoutLibrary::InversePoint(position, i);
				navTemplate.nodes.push_back(nodeTemplate);
			}
			for (auto& [begin, end] : rawLines) {
				NavigationLineTemplate lineTemplate;
				lineTemplate.begin = BuildingLayoutLibrary::InversePoint(begin, i);
				lineTemplate.end = BuildingLayoutLibrary::InversePoint(end, i);
				navTemplate.lines.push_back(lineTemplate);
			}
			navTemplate.connections = rawConnections;
			out[i] = std::move(navTemplate);
		}
	}
}

void BuildingLayoutLibrary::ReadTemplates(const vector<string>& paths) {
	stairTemplates.clear();
	elevatorTemplates.clear();
	rampTemplates.clear();
	ceilingTemplates.clear();
	groundTemplates.clear();
	corridorTemplates.clear();
	singleTemplates.clear();
	rowTemplates.clear();
	hatchTemplates.clear();
	pedestrianNavigationTemplates.clear();
	vehicleNavigationTemplates.clear();

	for (const auto& path : paths) {
		if (!filesystem::exists(path)) {
			THROW_EXCEPTION(IOException, "Path does not exist: " + path + ".\n");
		}

		filesystem::path p(path);
		if (p.extension().string() != ".layout") continue;
		string basename = p.stem().string();

		ifstream fin(path);
		if (!fin.is_open()) {
			THROW_EXCEPTION(IOException, "Failed to open file: " + path + "\n");
		}

		JsonReader reader;
		JsonValue root;
		if (!reader.Parse(fin, root)) {
			fin.close();
			THROW_EXCEPTION(JsonFormatException, "Json syntax error: " + reader.GetErrorMessages() + ".\n");
		}
		fin.close();

		array<vector<Stair>, 4> stairs4;
		for (auto& s : root["stairs"]) {
			RectParams rect = ReadRectParams(s["rect"]);
			int origDir = s["direction"].AsInt();
			for (int i = 0; i < 4; i++) {
				Stair stair(InverseParams(rect, i));
				stair.SetDirection(InverseDirection(origDir, i));
				for (auto& wall : s["walls"]) stair.AddWall(InverseDirection(wall.AsInt(), i));
				stairs4[i].push_back(stair);
			}
		}
		stairTemplates[basename] = std::move(stairs4);

		array<vector<Elevator>, 4> elevators4;
		for (auto& e : root["elevators"]) {
			RectParams rect = ReadRectParams(e["rect"]);
			int origDir = e["direction"].AsInt();
			for (int i = 0; i < 4; i++) {
				Elevator elevator(InverseParams(rect, i));
				elevator.SetDirection(InverseDirection(origDir, i));
				for (auto& wall : e["walls"]) elevator.AddWall(InverseDirection(wall.AsInt(), i));
				elevators4[i].push_back(elevator);
			}
		}
		elevatorTemplates[basename] = std::move(elevators4);

		array<vector<Ramp>, 4> ramps4;
		for (auto& r : root["ramps"]) {
			RectParams rect = ReadRectParams(r["rect"]);
			int origDir = r["direction"].AsInt();
			for (int i = 0; i < 4; i++) {
				Ramp ramp(InverseParams(rect, i));
				ramp.SetDirection(InverseDirection(origDir, i));
				for (auto& wall : r["walls"]) ramp.AddWall(InverseDirection(wall.AsInt(), i));
				ramps4[i].push_back(ramp);
			}
		}
		rampTemplates[basename] = std::move(ramps4);

		array<vector<Ceiling>, 4> ceilings4;
		for (auto& c : root["ceilings"]) {
			RectParams rect = ReadRectParams(c);
			for (int i = 0; i < 4; i++) ceilings4[i].emplace_back(InverseParams(rect, i));
		}
		ceilingTemplates[basename] = std::move(ceilings4);

		array<vector<Ground>, 4> grounds4;
		for (auto& g : root["grounds"]) {
			RectParams rect = ReadRectParams(g);
			for (int i = 0; i < 4; i++) grounds4[i].emplace_back(InverseParams(rect, i));
		}
		groundTemplates[basename] = std::move(grounds4);

		array<vector<Hatch>, 4> hatches4;
		for (auto& h : root["hatches"]) {
			RectParams rect = ReadRectParams(h);
			for (int i = 0; i < 4; i++) hatches4[i].emplace_back(InverseParams(rect, i));
		}
		hatchTemplates[basename] = std::move(hatches4);

		array<vector<Corridor>, 4> corridors4;
		for (auto& c : root["corridors"]) {
			RectParams rect = ReadRectParams(c["rect"]);
			for (int i = 0; i < 4; i++) {
				Corridor corridor(InverseParams(rect, i));
				for (auto& wall : c["walls"]) corridor.AddWall(InverseDirection(wall.AsInt(), i));
				for (auto& door : c["doors"]) {
					int origDir = door["direction"].AsInt();
					vector<RectParams> positions;
					for (auto& p : door["positions"]) positions.push_back(InverseWall(ReadRectParams(p), origDir, i));
					corridor.AddDoor(InverseDirection(origDir, i), positions);
				}
				for (auto& window : c["windows"]) {
					int origDir = window["direction"].AsInt();
					vector<RectParams> positions;
					for (auto& p : window["positions"]) positions.push_back(InverseWall(ReadRectParams(p), origDir, i));
					corridor.AddWindow(InverseDirection(origDir, i), positions);
				}
				corridors4[i].push_back(corridor);
			}
		}
		corridorTemplates[basename] = std::move(corridors4);

		array<vector<Single>, 4> singles4;
		for (auto& s : root["singles"]) {
			RectParams rect = ReadRectParams(s["rect"]);
			int origDir = s["direction"].AsInt();
			for (int i = 0; i < 4; i++) {
				Single single(InverseParams(rect, i));
				single.SetDirection(InverseDirection(origDir, i));
				for (auto& door : s["doors"]) {
					int doorDir = door["direction"].AsInt();
					vector<RectParams> positions;
					for (auto& p : door["positions"]) positions.push_back(InverseWall(ReadRectParams(p), doorDir, i));
					single.AddDoor(InverseDirection(doorDir, i), positions);
				}
				for (auto& window : s["windows"]) {
					int winDir = window["direction"].AsInt();
					vector<RectParams> positions;
					for (auto& p : window["positions"]) positions.push_back(InverseWall(ReadRectParams(p), winDir, i));
					single.AddWindow(InverseDirection(winDir, i), positions);
				}
				singles4[i].push_back(single);
			}
		}
		singleTemplates[basename] = std::move(singles4);

		array<vector<Row>, 4> rows4;
		for (auto& r : root["rows"]) {
			RectParams rect = ReadRectParams(r["rect"]);
			int origDir = r["direction"].AsInt();
			for (int i = 0; i < 4; i++) {
				Row row(InverseParams(rect, i));
				row.SetDirection(InverseDirection(origDir, i));
				for (auto& door : r["doors"]) {
					int doorDir = door["direction"].AsInt();
					vector<RectParams> positions;
					for (auto& p : door["positions"]) positions.push_back(InverseWall(ReadRectParams(p), doorDir, i));
					row.AddDoor(InverseDirection(doorDir, i), positions);
				}
				for (auto& window : r["windows"]) {
					int winDir = window["direction"].AsInt();
					vector<RectParams> positions;
					for (auto& p : window["positions"]) positions.push_back(InverseWall(ReadRectParams(p), winDir, i));
					row.AddWindow(InverseDirection(winDir, i), positions);
				}
				rows4[i].push_back(row);
			}
		}
		rowTemplates[basename] = std::move(rows4);

		array<NavigationTemplate, 4> pedestrianNav4;
		if (root.ValidMember("pedestrianNavigation")) {
			ParseNavigationBlock(root["pedestrianNavigation"], pedestrianNav4);
		}
		pedestrianNavigationTemplates[basename] = std::move(pedestrianNav4);

		array<NavigationTemplate, 4> vehicleNav4;
		if (root.ValidMember("vehicleNavigation")) {
			// 这次只解析、不使用，留给以后车辆域真正做"车辆能进建筑内部"这个玩法时再消费
			// (Building::Layout()不会读这份数据)。
			ParseNavigationBlock(root["vehicleNavigation"], vehicleNav4);
		}
		vehicleNavigationTemplates[basename] = std::move(vehicleNav4);
	}
}

namespace {
	template <typename T>
	const vector<T>& LookupTemplateList(const unordered_map<string, array<vector<T>, 4>>& map,
		const string& name, int face) {
		static const vector<T> empty;
		if (face < 0 || face >= 4) return empty;
		auto it = map.find(name);
		if (it == map.end()) return empty;
		return it->second[face];
	}
}

const vector<Stair>& BuildingLayoutLibrary::GetStairs(const string& name, int face) const {
	return LookupTemplateList(stairTemplates, name, face);
}
const vector<Elevator>& BuildingLayoutLibrary::GetElevators(const string& name, int face) const {
	return LookupTemplateList(elevatorTemplates, name, face);
}
const vector<Ramp>& BuildingLayoutLibrary::GetRamps(const string& name, int face) const {
	return LookupTemplateList(rampTemplates, name, face);
}
const vector<Ceiling>& BuildingLayoutLibrary::GetCeilings(const string& name, int face) const {
	return LookupTemplateList(ceilingTemplates, name, face);
}
const vector<Ground>& BuildingLayoutLibrary::GetGrounds(const string& name, int face) const {
	return LookupTemplateList(groundTemplates, name, face);
}
const vector<Corridor>& BuildingLayoutLibrary::GetCorridors(const string& name, int face) const {
	return LookupTemplateList(corridorTemplates, name, face);
}
const vector<Single>& BuildingLayoutLibrary::GetSingles(const string& name, int face) const {
	return LookupTemplateList(singleTemplates, name, face);
}
const vector<Row>& BuildingLayoutLibrary::GetRows(const string& name, int face) const {
	return LookupTemplateList(rowTemplates, name, face);
}
const vector<Hatch>& BuildingLayoutLibrary::GetHatches(const string& name, int face) const {
	return LookupTemplateList(hatchTemplates, name, face);
}

const NavigationTemplate& BuildingLayoutLibrary::GetPedestrianNavigation(const string& name, int face) const {
	static const NavigationTemplate empty;
	if (face < 0 || face >= 4) return empty;
	auto it = pedestrianNavigationTemplates.find(name);
	if (it == pedestrianNavigationTemplates.end()) return empty;
	return it->second[face];
}

const NavigationTemplate& BuildingLayoutLibrary::GetVehicleNavigation(const string& name, int face) const {
	static const NavigationTemplate empty;
	if (face < 0 || face >= 4) return empty;
	auto it = vehicleNavigationTemplates.find(name);
	if (it == vehicleNavigationTemplates.end()) return empty;
	return it->second[face];
}

// ===== Building =====

Building::Building(BuildingFactory* factory, BuildingMod* mod) :
	Quad(),
	mod(mod),
	factory(factory),
	type(),
	name() {
	if (!mod) {
		THROW_EXCEPTION(NullPointerException, "Building mod is null.\n");
	}

	type = mod->GetType();
	name = mod->GetName();
}

Building::~Building() {
	for (Room* room : rooms) delete room;
	for (Component* component : components) delete component;
	factory->DestroyBuilding(mod);
}

string Building::GetType() const { return type; }
string Building::GetName() const { return name; }
BuildingMod* Building::GetMod() const { return mod; }
int Building::GetDirection() const { return direction; }

void Building::Layout(int inputDirection, const BuildingLayoutLibrary& library,
	RoomFactory& roomFactory, ComponentFactory& componentFactory, BuildingNavResult& navOut) {
	int resolvedDirection = inputDirection;
	mod->Layout(resolvedDirection, *this, GetBoundaryRoads());
	direction = resolvedDirection;

	const BuildingFootprintSpec& fp = mod->footprint;
	bodyOffsetX = (fp.centerRatioX - 0.5f) * GetSizeX();
	bodyOffsetY = (fp.centerRatioY - 0.5f) * GetSizeY();
	bodySizeX = fp.sizeRatioX * GetSizeX();
	bodySizeY = fp.sizeRatioY * GetSizeY();

	basements = std::max(0, mod->basements);
	layers = std::max(1, mod->layers);
	int expected = basements + layers;
	floorHeights = mod->floorHeights;
	if (static_cast<int>(floorHeights.size()) != expected) {
		floorHeights.assign(expected, kDefaultFloorHeight);
	}
	lodMaterialPath = mod->lodMaterial;

	// 楼层：先按human-friendly层号(-basements..layers-1)全部占位建出空Floor，
	// 再用mod->floors记录的模板一个个填充——这样即使mod漏配了某一层，GetFloor(level)也
	// 返回一个"没有任何元素"的空Floor而不是nullptr，渲染层不用特殊处理这种情况。
	floors.clear();
	floors.reserve(expected);
	for (int idx = 0; idx < expected; idx++) {
		floors.emplace_back(idx - basements, bodySizeX, bodySizeY);
	}
	for (auto& [level, spec] : mod->floors) {
		ReadFloor(level, spec.face, spec.templateName, library);
	}

	// 组合+房间：按(component,id)去重创建Component，singles/rows里记录的每个槽位都实例化
	// 成真正的Room并挂到对应Component上。
	for (Room* room : rooms) delete room;
	for (Component* component : components) delete component;
	rooms.clear();
	components.clear();
	singleRoomBySlot.clear();
	rowRoomBySlot.clear();

	unordered_map<pair<string, int>, Component*, BuildingComponentKeyHash> componentMap;
	auto getOrCreateComponent = [&](const pair<string, int>& key) -> Component* {
		auto it = componentMap.find(key);
		if (it != componentMap.end()) return it->second;
		ComponentMod* componentMod = componentFactory.CreateComponent(key.first);
		if (!componentMod) return nullptr;
		Component* component = new Component(&componentFactory, componentMod, this);
		componentMap[key] = component;
		components.push_back(component);
		return component;
		};

	for (auto& [key, entries] : mod->singles) {
		Component* component = getOrCreateComponent(key);
		if (!component) continue;
		for (auto& [level, slot, roomType] : entries) {
			AssignRoom(level, slot, roomType, component, roomFactory);
		}
	}
	for (auto& [key, entries] : mod->rows) {
		Component* component = getOrCreateComponent(key);
		if (!component) continue;
		for (auto& [level, slot, roomType, acreage] : entries) {
			ArrangeRow(level, slot, roomType, acreage, component, roomFactory);
		}
	}

	// 每个Room自己中心点的导航锚点，供BuildPedestrianNavigation()里"single"/"row"类型的
	// 导航端点引用——照抄老工程LayoutBuilding()里"for (auto room : rooms) room->
	// SetNavigationNode(...)"这一步，这次没有ConfigRoom/PlacePivots(那些依赖furniture/
	// pivots，这次没有迁移)。
	for (Room* room : rooms) {
		auto [wx, wy] = LocalToWorld(room->GetPosX(), room->GetPosY());
		float z = GetFloorBaseZ(room->GetLayer());
		room->SetNavigationNode(new Node("building", wx, wy, z));
	}

	BuildPedestrianNavigation(library, resolvedDirection, navOut);
	BuildVehicleNavigation(library, resolvedDirection, navOut);
}

void Building::ReadFloor(int level, int face, const string& templateName, const BuildingLayoutLibrary& library) {
	int idx = basements + level;
	if (idx < 0 || idx >= static_cast<int>(floors.size())) return;

	float width = bodySizeX, height = bodySizeY;
	Floor floor(level, width, height);

	for (Stair stair : library.GetStairs(templateName, face)) {
		stair.InstanciateQuad(width, height);
		floor.AddStair(stair);
	}
	for (Elevator elevator : library.GetElevators(templateName, face)) {
		elevator.InstanciateQuad(width, height);
		floor.AddElevator(elevator);
	}
	for (Ramp ramp : library.GetRamps(templateName, face)) {
		ramp.InstanciateQuad(width, height);
		floor.AddRamp(ramp);
	}
	for (Ceiling ceiling : library.GetCeilings(templateName, face)) {
		ceiling.InstanciateQuad(width, height);
		floor.AddCeiling(ceiling);
	}
	for (Ground ground : library.GetGrounds(templateName, face)) {
		ground.InstanciateQuad(width, height);
		floor.AddGround(ground);
	}
	for (Corridor corridor : library.GetCorridors(templateName, face)) {
		corridor.InstanciateQuad(width, height);
		floor.AddCorridor(corridor);
	}
	for (Single single : library.GetSingles(templateName, face)) {
		single.InstanciateQuad(width, height);
		floor.AddSingle(single);
	}
	for (Row row : library.GetRows(templateName, face)) {
		row.InstanciateQuad(width, height);
		floor.AddRow(row);
	}
	for (Hatch hatch : library.GetHatches(templateName, face)) {
		hatch.InstanciateQuad(width, height);
		floor.AddHatch(hatch);
	}

	floors[idx] = std::move(floor);
}

void Building::AssignRoom(int level, int slot, const string& roomType, Component* component, RoomFactory& roomFactory) {
	int idx = basements + level;
	if (idx < 0 || idx >= static_cast<int>(floors.size())) return;
	const vector<Single>& singles = floors[idx].GetSingles();
	if (slot < 0 || slot >= static_cast<int>(singles.size())) return;

	RoomMod* roomMod = roomFactory.CreateRoom(roomType);
	if (!roomMod) return;
	Room* room = new Room(&roomFactory, roomMod, this, component, level);

	const Single& single = singles[slot];
	room->SetPosition(single.GetPosX(), single.GetPosY(), single.GetSizeX(), single.GetSizeY());
	room->SetDirection(single.GetDirection());
	room->SetDoors(single.GetDoors());
	room->SetWindows(single.GetWindows());
	room->SetNumber(level, floors[idx].AssignNumber());
	component->AddRoom(room);

	rooms.push_back(room);
	singleRoomBySlot[level][slot] = room;
}

void Building::ArrangeRow(int level, int slot, const string& roomType, float acreage, Component* component,
	RoomFactory& roomFactory) {
	int idx = basements + level;
	if (idx < 0 || idx >= static_cast<int>(floors.size())) return;
	const vector<Row>& rowList = floors[idx].GetRows();
	if (slot < 0 || slot >= static_cast<int>(rowList.size())) return;
	const Row& row = rowList[slot];

	if (acreage <= 0.f) return;
	float num = row.GetAcreage() / acreage;
	if (num < 1.f || num - static_cast<int>(num) >= 0.5f) num += 1.f;
	int count = std::max(1, static_cast<int>(num));

	bool alongY = (row.GetDirection() == FACE_WEST || row.GetDirection() == FACE_EAST);
	float div = alongY ? row.GetSizeY() / count : row.GetSizeX() / count;

	for (int i = 0; i < count; i++) {
		RoomMod* roomMod = roomFactory.CreateRoom(roomType);
		if (!roomMod) continue;
		Room* room = new Room(&roomFactory, roomMod, this, component, level);

		if (alongY) {
			room->SetVertices(row.GetLeft(), row.GetBottom() + div * i, row.GetRight(), row.GetBottom() + div * (i + 1));
		} else {
			room->SetVertices(row.GetLeft() + div * i, row.GetBottom(), row.GetLeft() + div * (i + 1), row.GetTop());
		}
		room->SetDirection(row.GetDirection());
		room->SetDoors(row.GetDoors());
		room->SetWindows(row.GetWindows());
		room->SetNumber(level, floors[idx].AssignNumber());
		component->AddRoom(room);

		rooms.push_back(room);
		rowRoomBySlot[level][slot].push_back(room);
	}
}

float Building::GetBodyOffsetX() const { return bodyOffsetX; }
float Building::GetBodyOffsetY() const { return bodyOffsetY; }
float Building::GetBodySizeX() const { return bodySizeX; }
float Building::GetBodySizeY() const { return bodySizeY; }
int Building::GetBasementCount() const { return basements; }
int Building::GetLayerCount() const { return layers; }
const vector<float>& Building::GetFloorHeights() const { return floorHeights; }
const string& Building::GetLodMaterialPath() const { return lodMaterialPath; }

const Floor* Building::GetFloor(int level) const {
	int idx = basements + level;
	if (idx < 0 || idx >= static_cast<int>(floors.size())) return nullptr;
	return &floors[idx];
}
const vector<Component*>& Building::GetComponents() const { return components; }
const vector<Room*>& Building::GetRooms() const { return rooms; }

float Building::GetFloorBaseZ(int level) const {
	if (floorHeights.empty()) return 0.f;
	int idx = basements + level;
	float z = 0.f;
	if (level >= 0) {
		for (int i = basements; i < idx && i < static_cast<int>(floorHeights.size()); i++) z += floorHeights[i];
	} else {
		for (int i = idx; i < basements && i >= 0; i++) z -= floorHeights[i];
	}
	return z;
}

pair<float, float> Building::LocalToWorld(float localX, float localY) const {
	float relX = localX - bodySizeX * 0.5f + bodyOffsetX;
	float relY = localY - bodySizeY * 0.5f + bodyOffsetY;
	float rot = GetRotation();
	float c = cosf(rot), s = sinf(rot);
	float worldX = GetPosX() + relX * c - relY * s;
	float worldY = GetPosY() + relX * s + relY * c;
	return { worldX, worldY };
}

float Building::GetRotation() const {
	return (parentLot ? parentLot->GetRotation() : 0.f) + relativeRotation;
}

string Building::GetAddress() const {
	if (parentZone) return parentZone->GetAddress() + " " + GetName();
	if (parentLot) return parentLot->GetAddress() + " " + GetName();
	return "";
}

Lot* Building::GetParentLot() const { return parentLot; }

void Building::SetParentLot(Lot* lot, float relativeRot) {
	parentLot = lot;
	relativeRotation = relativeRot;
}

Zone* Building::GetParentZone() const { return parentZone; }
void Building::SetParentZone(Zone* zone) { parentZone = zone; }

void Building::SetBoundaryRoad(int direction, Road* road) { boundaryRoads[direction] = road; }

Road* Building::GetBoundaryRoad(int direction) const {
	auto it = boundaryRoads.find(direction);
	return it != boundaryRoads.end() ? it->second : nullptr;
}

const unordered_map<int, Road*>& Building::GetBoundaryRoads() const { return boundaryRoads; }

Citizen* Building::GetOwner() const { return owner; }
void Building::SetOwner(Citizen* value) { owner = value; }
bool Building::GetStated() const { return stated; }
void Building::SetStated(bool value) { stated = value; }

float Building::ProjectOntoLine(float px, float py, float ax, float ay, float bx, float by) {
	float dx = bx - ax, dy = by - ay;
	float lenSq = dx * dx + dy * dy;
	if (lenSq <= 0.f) return 0.f;
	float t = ((px - ax) * dx + (py - ay) * dy) / lenSq;
	if (t < 0.f) t = 0.f;
	if (t > 1.f) t = 1.f;
	return t;
}

namespace {
	// 建筑导航图构建期间，一条"贯通线"(走廊骨架线段)的运行时状态：固定端点世界坐标缓存+
	// 沿line收集到的锚点(用于排序后串成走廊骨架)——照抄老工程LineRuntime。
	struct LineRuntime {
		float beginX = 0.f, beginY = 0.f;
		float endX = 0.f, endY = 0.f;
		Node* nodeAt0 = nullptr;
		Node* nodeAt1 = nullptr;
		vector<pair<float, Node*>> anchors;
	};
}

void Building::BuildPedestrianNavigation(const BuildingLayoutLibrary& library, int inputDirection, BuildingNavResult& navOut) {
	BuildNavigationGraph(library, inputDirection, /*isVehicle=*/false,
		navOut.nodes, navOut.connections, navOut.outsideNodes, /*registerRoomNodes=*/true);
}

void Building::BuildVehicleNavigation(const BuildingLayoutLibrary& library, int inputDirection, BuildingNavResult& navOut) {
	BuildNavigationGraph(library, inputDirection, /*isVehicle=*/true,
		navOut.vehicleNodes, navOut.vehicleConnections, navOut.vehicleOutsideNodes, /*registerRoomNodes=*/false);
}

void Building::BuildNavigationGraph(const BuildingLayoutLibrary& library, int inputDirection, bool isVehicle,
	vector<Node*>& newNodes, vector<Connection*>& newConnections, vector<Node*>& outsideNodes,
	bool registerRoomNodes) {
	// 楼层间楼梯端点登记，留待所有楼层处理完后按相邻层贪心匹配。
	vector<tuple<int, Node*, bool>> stairEndpoints;

	for (int idx = 0; idx < static_cast<int>(floors.size()); idx++) {
		int level = idx - basements;
		const Floor& floor = floors[idx];

		auto specIt = mod->floors.find(level);
		if (specIt == mod->floors.end()) continue;
		const string& templateName = specIt->second.templateName;
		int face = specIt->second.face;

		const NavigationTemplate& navTemplate = isVehicle
			? library.GetVehicleNavigation(templateName, face)
			: library.GetPedestrianNavigation(templateName, face);
		if (navTemplate.nodes.empty() && navTemplate.lines.empty() && navTemplate.connections.empty()) continue;

		float floorWidth = floor.GetSizeX();
		float floorHeight = floor.GetSizeY();
		float z = GetFloorBaseZ(level);

		auto makeNode = [this, z](float lx, float ly) -> Node* {
			auto [wx, wy] = LocalToWorld(lx, ly);
			return new Node("building", wx, wy, z);
			};

		vector<Node*> floorFixedNodes;
		vector<pair<float, float>> floorFixedNodePositions;
		for (auto& nodeTemplate : navTemplate.nodes) {
			float lx = nodeTemplate.position[0] * floorWidth + nodeTemplate.position[1];
			float ly = nodeTemplate.position[2] * floorHeight + nodeTemplate.position[3];
			Node* node = makeNode(lx, ly);
			newNodes.push_back(node);
			floorFixedNodes.push_back(node);
			floorFixedNodePositions.emplace_back(lx, ly);
		}

		vector<LineRuntime> lineRuntimes(navTemplate.lines.size());
		for (size_t i = 0; i < navTemplate.lines.size(); i++) {
			auto& lineTemplate = navTemplate.lines[i];
			lineRuntimes[i].beginX = lineTemplate.begin[0] * floorWidth + lineTemplate.begin[1];
			lineRuntimes[i].beginY = lineTemplate.begin[2] * floorHeight + lineTemplate.begin[3];
			lineRuntimes[i].endX = lineTemplate.end[0] * floorWidth + lineTemplate.end[1];
			lineRuntimes[i].endY = lineTemplate.end[2] * floorHeight + lineTemplate.end[3];
		}

		auto resolveEndpoint = [&](const NavigationEndpointTemplate& endpoint) -> vector<Node*> {
			if (endpoint.type == "node") {
				if (endpoint.idx < 0 || endpoint.idx >= static_cast<int>(floorFixedNodes.size())) return {};
				return { floorFixedNodes[endpoint.idx] };
			}
			if (endpoint.type == "single") {
				auto levelIt = singleRoomBySlot.find(level);
				if (levelIt == singleRoomBySlot.end()) return {};
				auto slotIt = levelIt->second.find(endpoint.idx);
				if (slotIt == levelIt->second.end()) return {};
				return { slotIt->second->GetNavigationNode() };
			}
			if (endpoint.type == "row") {
				auto levelIt = rowRoomBySlot.find(level);
				if (levelIt == rowRoomBySlot.end()) return {};
				auto slotIt = levelIt->second.find(endpoint.idx);
				if (slotIt == levelIt->second.end()) return {};
				vector<Node*> result;
				for (Room* room : slotIt->second) result.push_back(room->GetNavigationNode());
				return result;
			}
			if (endpoint.type == "line") {
				if (endpoint.idx < 0 || endpoint.idx >= static_cast<int>(lineRuntimes.size())) return {};
				auto& line = lineRuntimes[endpoint.idx];
				if (endpoint.vertex == 0) {
					if (!line.nodeAt0) {
						line.nodeAt0 = makeNode(line.beginX, line.beginY);
						newNodes.push_back(line.nodeAt0);
						line.anchors.emplace_back(0.f, line.nodeAt0);
					}
					return { line.nodeAt0 };
				}
				if (endpoint.vertex == 1) {
					if (!line.nodeAt1) {
						line.nodeAt1 = makeNode(line.endX, line.endY);
						newNodes.push_back(line.nodeAt1);
						line.anchors.emplace_back(1.f, line.nodeAt1);
					}
					return { line.nodeAt1 };
				}
				return {};
			}
			return {};
			};

		for (auto& connTemplate : navTemplate.connections) {
			bool beginIsProjection = connTemplate.begin.type == "line" && connTemplate.begin.vertex == -1;
			bool endIsProjection = connTemplate.end.type == "line" && connTemplate.end.vertex == -1;

			// line的vertex=-1端：把对端动态投影到line上生成锚点并与其连接。
			if (beginIsProjection || endIsProjection) {
				const NavigationEndpointTemplate& lineSide = beginIsProjection ? connTemplate.begin : connTemplate.end;
				const NavigationEndpointTemplate& srcSide = beginIsProjection ? connTemplate.end : connTemplate.begin;
				if (srcSide.type == "line" && srcSide.vertex == -1) continue;
				if (lineSide.idx < 0 || lineSide.idx >= static_cast<int>(lineRuntimes.size())) continue;
				auto& line = lineRuntimes[lineSide.idx];

				float nx = 0.f, ny = 0.f;
				Node* srcNode = nullptr;
				if (srcSide.type == "node") {
					if (srcSide.idx < 0 || srcSide.idx >= static_cast<int>(floorFixedNodes.size())) continue;
					tie(nx, ny) = floorFixedNodePositions[srcSide.idx];
					srcNode = floorFixedNodes[srcSide.idx];
				} else if (srcSide.type == "line") {
					if (srcSide.idx < 0 || srcSide.idx >= static_cast<int>(lineRuntimes.size())) continue;
					auto& srcLine = lineRuntimes[srcSide.idx];
					nx = (srcSide.vertex == 0) ? srcLine.beginX : srcLine.endX;
					ny = (srcSide.vertex == 0) ? srcLine.beginY : srcLine.endY;
					vector<Node*> srcNodes = resolveEndpoint(srcSide);
					if (srcNodes.empty()) continue;
					srcNode = srcNodes[0];
				} else if (srcSide.type == "single" || srcSide.type == "row") {
					vector<Room*> targetRooms;
					if (srcSide.type == "single") {
						auto levelIt = singleRoomBySlot.find(level);
						if (levelIt == singleRoomBySlot.end()) continue;
						auto slotIt = levelIt->second.find(srcSide.idx);
						if (slotIt == levelIt->second.end()) continue;
						targetRooms.push_back(slotIt->second);
					} else {
						auto levelIt = rowRoomBySlot.find(level);
						if (levelIt == rowRoomBySlot.end()) continue;
						auto slotIt = levelIt->second.find(srcSide.idx);
						if (slotIt == levelIt->second.end()) continue;
						targetRooms = slotIt->second;
					}
					for (Room* room : targetRooms) {
						float t = ProjectOntoLine(room->GetPosX(), room->GetPosY(), line.beginX, line.beginY, line.endX, line.endY);
						float px = line.beginX + t * (line.endX - line.beginX);
						float py = line.beginY + t * (line.endY - line.beginY);
						Node* projected = makeNode(px, py);
						newNodes.push_back(projected);
						line.anchors.emplace_back(t, projected);
						newConnections.push_back(new Connection(*projected, *room->GetNavigationNode()));
					}
					continue;
				} else {
					continue;
				}

				float t = ProjectOntoLine(nx, ny, line.beginX, line.beginY, line.endX, line.endY);
				float px = line.beginX + t * (line.endX - line.beginX);
				float py = line.beginY + t * (line.endY - line.beginY);
				Node* projected = makeNode(px, py);
				newNodes.push_back(projected);
				line.anchors.emplace_back(t, projected);
				newConnections.push_back(new Connection(*projected, *srcNode));
				continue;
			}

			// outside端：这次不照抄老工程"连building边界最近两个角"的近似，只在这里实例化
			// 出这个outside端点自己的世界坐标节点，收集进outsideNodes——由Map::
			// InitBuildings()负责用building朝向的边界Road+AddRoadAccessNode真正接上
			// 道路网(需要访问Map自己的数据，Building这一层做不到)，见building.md。
			if (connTemplate.begin.type == "outside" || connTemplate.end.type == "outside") {
				const NavigationEndpointTemplate& otherSide =
					connTemplate.begin.type == "outside" ? connTemplate.end : connTemplate.begin;
				vector<Node*> otherNodes = resolveEndpoint(otherSide);
				if (otherNodes.size() != 1 || !otherNodes[0]) continue;
				outsideNodes.push_back(otherNodes[0]);
				continue;
			}

			// upstair/downstair端：先登记，留待所有楼层处理完后按相邻层贪心匹配。
			if (connTemplate.begin.type == "upstair" || connTemplate.begin.type == "downstair" ||
				connTemplate.end.type == "upstair" || connTemplate.end.type == "downstair") {
				bool beginIsStair = connTemplate.begin.type == "upstair" || connTemplate.begin.type == "downstair";
				const NavigationEndpointTemplate& stairSide = beginIsStair ? connTemplate.begin : connTemplate.end;
				const NavigationEndpointTemplate& otherSide = beginIsStair ? connTemplate.end : connTemplate.begin;
				vector<Node*> otherNodes = resolveEndpoint(otherSide);
				if (otherNodes.size() != 1 || !otherNodes[0]) continue;
				stairEndpoints.emplace_back(level, otherNodes[0], stairSide.type == "upstair");
				continue;
			}

			vector<Node*> beginNodes = resolveEndpoint(connTemplate.begin);
			vector<Node*> endNodes = resolveEndpoint(connTemplate.end);
			for (Node* beginNode : beginNodes) {
				for (Node* endNode : endNodes) {
					if (!beginNode || !endNode) continue;
					newConnections.push_back(new Connection(*beginNode, *endNode));
				}
			}
		}

		for (auto& line : lineRuntimes) {
			if (line.anchors.size() < 2) continue;
			sort(line.anchors.begin(), line.anchors.end(),
				[](const pair<float, Node*>& a, const pair<float, Node*>& b) { return a.first < b.first; });
			for (size_t i = 0; i + 1 < line.anchors.size(); i++) {
				newConnections.push_back(new Connection(*line.anchors[i].second, *line.anchors[i + 1].second));
			}
		}
	}

	// 相邻楼层之间按世界坐标贪心最近匹配upstair/downstair。
	unordered_map<int, vector<pair<Node*, int>>> upstairsByLevel;
	unordered_map<int, vector<pair<Node*, int>>> downstairsByLevel;
	for (int i = 0; i < static_cast<int>(stairEndpoints.size()); i++) {
		auto& [level, node, isUp] = stairEndpoints[i];
		if (isUp) upstairsByLevel[level].emplace_back(node, i);
		else downstairsByLevel[level].emplace_back(node, i);
	}
	for (auto& [level, upList] : upstairsByLevel) {
		auto downIt = downstairsByLevel.find(level + 1);
		if (downIt == downstairsByLevel.end()) continue;
		auto& downList = downIt->second;

		vector<tuple<float, int, int>> candidates;
		for (int i = 0; i < static_cast<int>(upList.size()); i++) {
			for (int j = 0; j < static_cast<int>(downList.size()); j++) {
				float dx = upList[i].first->GetX() - downList[j].first->GetX();
				float dy = upList[i].first->GetY() - downList[j].first->GetY();
				candidates.emplace_back(dx * dx + dy * dy, i, j);
			}
		}
		sort(candidates.begin(), candidates.end(),
			[](const tuple<float, int, int>& a, const tuple<float, int, int>& b) { return get<0>(a) < get<0>(b); });

		vector<bool> upUsed(upList.size(), false), downUsed(downList.size(), false);
		for (auto& [_, i, j] : candidates) {
			if (upUsed[i] || downUsed[j]) continue;
			upUsed[i] = true;
			downUsed[j] = true;
			newConnections.push_back(new Connection(*upList[i].first, *downList[j].first));
		}
	}

	// Room自己中心点的导航节点由Room持有(Layout()里创建)，不在上面floorFixedNodes/line
	// 锚点这条链路里，但仍然要登记进navOut.nodes，否则Map::navAnchorNodes永远拿不到它们——
	// 既没办法在导航图可视化里按坐标画出来，也没有任何地方会delete它们(内存泄漏)。照抄
	// 老工程BuildNavigation结尾"for (auto room : rooms) newNodes.push_back(room->
	// GetNavigationNode())"这一步。**只有registerRoomNodes=true(行人那次调用)才做**——
	// 车行调用如果也做一遍，同一个Room的Node*会被登记进navAnchorNodes两次，~Map()清理时
	// double free，见BuildVehicleNavigation声明处的注释。
	if (registerRoomNodes) {
		for (Room* room : rooms) {
			if (room && room->GetNavigationNode()) {
				newNodes.push_back(room->GetNavigationNode());
			}
		}
	}
}
