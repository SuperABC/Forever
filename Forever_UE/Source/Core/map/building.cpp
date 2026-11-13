#include "building.h"

#include "common/error.h"
#include "common/json.h"
#include "common/utility.h"

#include "map/room.h"
#include "map/component.h"
#include "map/door.h"
#include "map/map.h"
#include "map/zone.h"
#include "story/script.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <cstdint>
#include <fstream>
#include <sstream>


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

	constexpr float kPi = 3.14159265358979323846f;

	// 墙面信息：horizBase/horizSpan/alongY/acrossFixed公式必须和Forever/Element/BuildingElement.cpp
	// ::BuildWallsForElement的FFaceInfo表完全一致，否则门扇和渲染层挖的门洞对不齐(见door.md)。
	// normalYaw是墙外法线朝向(弧度，building旋转=0时的局部朝向，BuildDoors再叠加GetRotation())：
	// EAST=0，SOUTH=π/2，WEST=π，NORTH=-π/2，与LocalToWorld的cos/sin旋转约定一致。
	struct DoorFaceInfo { bool alongY; float horizBase; float horizSpan; float acrossFixed; float normalYaw; };

	array<DoorFaceInfo, 4> ComputeDoorFaces(float centerX, float centerY, float sizeX, float sizeY) {
		float halfX = sizeX * 0.5f, halfY = sizeY * 0.5f;
		array<DoorFaceInfo, 4> faces{};
		faces[FACE_WEST] = { true, centerY - halfY, sizeY, centerX - halfX, kPi };
		faces[FACE_EAST] = { true, centerY - halfY, sizeY, centerX + halfX, 0.f };
		faces[FACE_NORTH] = { false, centerX - halfX, sizeX, centerY - halfY, -kPi * 0.5f };
		faces[FACE_SOUTH] = { false, centerX - halfX, sizeX, centerY + halfY, kPi * 0.5f };
		return faces;
	}

	// 把WallHole+DoorTags里的每个门洞resolve+CreateDoor。centerX/Y/sizeX/Y是元素(corridor或room)
	// 在楼体局部坐标系里的中心和尺寸，floorHeight/floorBaseZ是所在层的高度和底部Z(地图单位)。
	void BuildDoorsForElement(Map& map, const WallHole& holes, const DoorTags& tags,
		const unordered_map<string, DoorSpec>& doorSpecs, float centerX, float centerY,
		float sizeX, float sizeY, float floorHeight, float floorBaseZ, int level,
		DOOR_KIND_TYPE kind, Zone* zone, Building* building, Room* room, vector<Door*>& out) {
		array<DoorFaceInfo, 4> faces = ComputeDoorFaces(centerX, centerY, sizeX, sizeY);

		for (const auto& [dir, positions] : holes) {
			if (dir < 0 || dir >= 4) continue;
			const DoorFaceInfo& face = faces[dir];
			auto tagIt = tags.find(dir);

			for (size_t i = 0; i < positions.size(); i++) {
				bool hasTag = (tagIt != tags.end() && i < tagIt->second.size());
				string tag = hasTag ? tagIt->second[i] : string();
				const DoorSpec* spec = ResolveDoorSpec(doorSpecs, tag);
				if (!spec) continue;

				const RectParams& p = positions[i];
				float x1 = p[0] * face.horizSpan + p[1];
				float x2 = p[4] * face.horizSpan + p[5];
				float y1 = p[2] * floorHeight + p[3];
				float y2 = p[6] * floorHeight + p[7];

				float alongWallCenter = face.horizBase + (x1 + x2) * 0.5f;
				float localX = face.alongY ? face.acrossFixed : alongWallCenter;
				float localY = face.alongY ? alongWallCenter : face.acrossFixed;

				pair<float, float> worldPos = building->LocalToWorld(localX, localY);
				float width = fabsf(x2 - x1);
				float height = fabsf(y2 - y1);
					// y1/y2是"从天花板往下量"的距离(和BuildWallsForElement挖墙洞的约定一致)，所以门底边离地板
					// =floorHeight-max(y1,y2)，不是min(y1,y2)；写反会让门悬在半空。
				float z = floorBaseZ + (floorHeight - max(y1, y2));
				float yaw = face.normalYaw + building->GetRotation();

				Door* door = map.CreateDoor(kind, zone, building, room, tag, *spec,
					worldPos.first, worldPos.second, z, width, height, yaw, level, false);
				if (door) out.push_back(door);
			}
		}
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
const WallStyle& Stair::GetWallStyle(int dir) const {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Stair wall direction out of range [0,3].\n");
	return wallStyles[dir];
}
WallStyle& Stair::MutableWallStyle(int dir) {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Stair wall direction out of range [0,3].\n");
	return wallStyles[dir];
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
const WallStyle& Elevator::GetWallStyle(int dir) const {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Elevator wall direction out of range [0,3].\n");
	return wallStyles[dir];
}
WallStyle& Elevator::MutableWallStyle(int dir) {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Elevator wall direction out of range [0,3].\n");
	return wallStyles[dir];
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
const WallStyle& Ramp::GetWallStyle(int dir) const {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Ramp wall direction out of range [0,3].\n");
	return wallStyles[dir];
}
WallStyle& Ramp::MutableWallStyle(int dir) {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Ramp wall direction out of range [0,3].\n");
	return wallStyles[dir];
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
const DoorTags& Corridor::GetDoorTags() const { return doorTags; }
void Corridor::AddDoor(int dir, vector<RectParams> positions, string tag) {
	for (auto& p : positions) {
		doors[dir].push_back(p);
		doorTags[dir].push_back(tag);
	}
}
const WallHole& Corridor::GetWindows() const { return windows; }
void Corridor::AddWindow(int dir, vector<RectParams> positions) {
	for (auto& p : positions) windows[dir].push_back(p);
}
const WallStyle& Corridor::GetWallStyle(int dir) const {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Corridor wall direction out of range [0,3].\n");
	return wallStyles[dir];
}
WallStyle& Corridor::MutableWallStyle(int dir) {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Corridor wall direction out of range [0,3].\n");
	return wallStyles[dir];
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
const DoorTags& Single::GetDoorTags() const { return doorTags; }
void Single::AddDoor(int dir, vector<RectParams> positions, string tag) {
	for (auto& p : positions) {
		doors[dir].push_back(p);
		doorTags[dir].push_back(tag);
	}
}
const WallHole& Single::GetWindows() const { return windows; }
void Single::AddWindow(int dir, vector<RectParams> positions) {
	for (auto& p : positions) windows[dir].push_back(p);
}
const WallStyle& Single::GetWallStyle(int dir) const {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Single wall direction out of range [0,3].\n");
	return wallStyles[dir];
}
WallStyle& Single::MutableWallStyle(int dir) {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Single wall direction out of range [0,3].\n");
	return wallStyles[dir];
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
const DoorTags& Row::GetDoorTags() const { return doorTags; }
void Row::AddDoor(int dir, vector<RectParams> positions, string tag) {
	for (auto& p : positions) {
		doors[dir].push_back(p);
		doorTags[dir].push_back(tag);
	}
}
const WallHole& Row::GetWindows() const { return windows; }
void Row::AddWindow(int dir, vector<RectParams> positions) {
	for (auto& p : positions) windows[dir].push_back(p);
}
const WallStyle& Row::GetWallStyle(int dir) const {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Row wall direction out of range [0,3].\n");
	return wallStyles[dir];
}
WallStyle& Row::MutableWallStyle(int dir) {
	if (dir < 0 || dir >= 4) THROW_EXCEPTION(InvalidArgumentException, "Row wall direction out of range [0,3].\n");
	return wallStyles[dir];
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

float BuildingLayoutLibrary::FaceYawDegrees(int face) {
	switch (face) {
	case FACE_WEST: return 270.f;
	case FACE_EAST: return 90.f;
	case FACE_SOUTH: return 180.f;
	default: return 0.f;
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
	// 某些旋转会翻转沿墙方向：face0翻转水平墙(NORTH/SOUTH)，face1翻转竖直墙(WEST/EAST)，face2不翻转，
	// face3全部翻转；翻转时起终点对调并做1-ratio镜像。
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

	// 解析元素上可选的"wallTags"：{"<墙朝向0-3>":{"east":"tag","south":..,"west":..,"north":..}}。
	// face是模板旋转版本，外层墙朝向和内层侧面方向都经InverseDirection换算(和walls/doors一致)；
	// 结果按旋转后的墙朝向写进elem的WallStyle，tags下标同样是旋转后的侧面方向(FACE_DIRECTION)。
	template <class T>
	void ReadWallTags(const JsonValue& node, int face, T& elem) {
		if (!node.ValidMember("wallTags")) return;
		const JsonValue& wallTags = node["wallTags"];
		static const char* const sideNames[4] = { "west", "east", "north", "south" }; // 下标=FACE_DIRECTION
		for (int wall = 0; wall < 4; wall++) {
			string key = std::to_string(wall);
			if (!wallTags.ValidMember(key)) continue;
			const JsonValue& sides = wallTags[key];
			WallStyle& style = elem.MutableWallStyle(BuildingLayoutLibrary::InverseDirection(wall, face));
			for (int side = 0; side < 4; side++) {
				if (!sides.ValidMember(sideNames[side])) continue;
				style.tags[BuildingLayoutLibrary::InverseDirection(side, face)] = sides[sideNames[side]].AsString();
			}
		}
	}

	// 解析一个navigation/pedestrianNavigation/vehicleNavigation块，产出4个朝向各一份的
	// NavigationTemplate——照抄老工程Building::ReadTemplates对应段落。navRoot为空(节点
	// 不存在，比如vehicleNavigation这次多数模板都没画)时4份都是默认构造的空NavigationTemplate。
	void ParseNavigationBlock(const JsonValue& navRoot, array<NavigationTemplate, 4>& out) {
		vector<PointParams> rawNodes;
		for (auto& n : navRoot["nodes"]) {
			rawNodes.push_back(ReadPointParams(n["position"]));
		}

		// 可选的通行方向字段（车行导航用）：缺省 forward=true/backward=false，保持旧layout行为。
		auto readDirection = [](const JsonValue& e, bool& forward, bool& backward) {
			if (e.ValidMember("forward")) forward = e["forward"].AsBool();
			if (e.ValidMember("backward")) backward = e["backward"].AsBool();
		};

		struct RawLine { PointParams begin; PointParams end; bool forward = true; bool backward = false; };
		vector<RawLine> rawLines;
		for (auto& l : navRoot["lines"]) {
			RawLine rawLine{ ReadPointParams(l["begin"]), ReadPointParams(l["end"]) };
			readDirection(l, rawLine.forward, rawLine.backward);
			rawLines.push_back(rawLine);
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
			readDirection(c, connTemplate.forward, connTemplate.backward);
			rawConnections.push_back(connTemplate);
		}

		for (int i = 0; i < 4; i++) {
			NavigationTemplate navTemplate;
			for (auto& position : rawNodes) {
				NavigationNodeTemplate nodeTemplate;
				nodeTemplate.position = BuildingLayoutLibrary::InversePoint(position, i);
				navTemplate.nodes.push_back(nodeTemplate);
			}
			for (const RawLine& rawLine : rawLines) {
				NavigationLineTemplate lineTemplate;
				lineTemplate.begin = BuildingLayoutLibrary::InversePoint(rawLine.begin, i);
				lineTemplate.end = BuildingLayoutLibrary::InversePoint(rawLine.end, i);
				lineTemplate.forward = rawLine.forward;
				lineTemplate.backward = rawLine.backward;
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
				ReadWallTags(s, i, stair);
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
				ReadWallTags(e, i, elevator);
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
				ReadWallTags(r, i, ramp);
				ramps4[i].push_back(ramp);
			}
		}
		rampTemplates[basename] = std::move(ramps4);

		array<vector<Ceiling>, 4> ceilings4;
		for (auto& c : root["ceilings"]) {
			// 条目是纯8数组(无标签)或{"rect":[...], "tag":"..."}。
			RectParams rect = ReadRectParams(c.IsObject() ? c["rect"] : c);
			string tag = c.IsObject() && !c["tag"].IsNull() ? c["tag"].AsString() : "";
			for (int i = 0; i < 4; i++) {
				ceilings4[i].emplace_back(InverseParams(rect, i));
				ceilings4[i].back().SetTag(tag);
			}
		}
		ceilingTemplates[basename] = std::move(ceilings4);

		array<vector<Ground>, 4> grounds4;
		for (auto& g : root["grounds"]) {
			RectParams rect = ReadRectParams(g.IsObject() ? g["rect"] : g);
			string tag = g.IsObject() && !g["tag"].IsNull() ? g["tag"].AsString() : "";
			for (int i = 0; i < 4; i++) {
				grounds4[i].emplace_back(InverseParams(rect, i));
				grounds4[i].back().SetTag(tag);
			}
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
				ReadWallTags(c, i, corridor);
				for (auto& door : c["doors"]) {
					int origDir = door["direction"].AsInt();
					string tag = door["tag"].IsNull() ? "" : door["tag"].AsString();
					vector<RectParams> positions;
					for (auto& p : door["positions"]) positions.push_back(InverseWall(ReadRectParams(p), origDir, i));
					corridor.AddDoor(InverseDirection(origDir, i), positions, tag);
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
				ReadWallTags(s, i, single);
				for (auto& door : s["doors"]) {
					int doorDir = door["direction"].AsInt();
					string tag = door["tag"].IsNull() ? "" : door["tag"].AsString();
					vector<RectParams> positions;
					for (auto& p : door["positions"]) positions.push_back(InverseWall(ReadRectParams(p), doorDir, i));
					single.AddDoor(InverseDirection(doorDir, i), positions, tag);
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
				ReadWallTags(r, i, row);
				for (auto& door : r["doors"]) {
					int doorDir = door["direction"].AsInt();
					string tag = door["tag"].IsNull() ? "" : door["tag"].AsString();
					vector<RectParams> positions;
					for (auto& p : door["positions"]) positions.push_back(InverseWall(ReadRectParams(p), doorDir, i));
					row.AddDoor(InverseDirection(doorDir, i), positions, tag);
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
			// 车行导航模板，由Building::BuildVehicleNavigation使用。
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

bool BuildingLayoutLibrary::HasTemplate(const string& name) const {
	// 每个.layout文件解析时每类元素都会登记一项(哪怕是空数组)，用stairTemplates代表"这个模板存在"。
	return stairTemplates.count(name) > 0;
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
	for (Door* door : doorEntities) delete door;
	for (Cabin* cabin : cabinEntities) delete cabin;
	delete script;
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
	// 允许0层(basements也为0时整栋没有任何Floor，GetFloor(level)恒返回nullptr)——ParkBuilding
	// 这类只有植被、不放楼体的"建筑"用，UE层会直接跳过这种building不渲染，见building.md。
	layers = std::max(0, mod->layers);
	int expected = basements + layers;
	floorHeights = mod->floorHeights;
	if (static_cast<int>(floorHeights.size()) != expected) {
		floorHeights.assign(expected, kDefaultFloorHeight);
	}
	lodMaterialPath = mod->lodMaterial;
	gapMaterialPath = mod->gapMaterial;
	floorSpecs.clear();
	for (auto& [level, spec] : mod->floors) floorSpecs[level] = spec;
	cabins = mod->cabins;
	stationMod = mod->stationMod;
	scriptModName = mod->scriptModName;
	scriptMilestones = mod->milestoneNames;

		// 自定义模型：位置/yaw按face=2(模板原样)定义，按最终朝向用InversePoint换算(direction==-1按原样)，
		// 再经LocalToWorld叠加建筑的世界旋转；yaw的朝向偏移见FaceYawDegrees。
	decorations.clear();
	{
		const float degToRad = 3.14159265358979323846f / 180.f;
		int face = (direction >= 0 && direction < 4) ? direction : FACE_NORTH;
		float faceYawDegrees = BuildingLayoutLibrary::FaceYawDegrees(face);
		for (const DecorationSpec& spec : mod->decorations) {
			PointParams p = BuildingLayoutLibrary::InversePoint(spec.position, face);
			auto [worldX, worldY] = LocalToWorld(p[0] * bodySizeX + p[1], p[2] * bodySizeY + p[3]);
			DecorationInstance decoration;
			decoration.meshPath = spec.meshPath;
			decoration.x = worldX;
			decoration.y = worldY;
			decoration.z = spec.z;
			decoration.yaw = GetRotation() + (faceYawDegrees + spec.yaw) * degToRad;
			decoration.pitch = spec.pitch;
			decoration.roll = spec.roll;
			decoration.scaleX = spec.scaleX;
			decoration.scaleY = spec.scaleY;
			decoration.scaleZ = spec.scaleZ;
			decorations.push_back(decoration);
		}
	}

	// 楼层：先按human-friendly层号(-basements..layers-1)全部占位建出空Floor，
	// 再用mod->floors记录的模板一个个填充——这样即使mod漏配了某一层，GetFloor(level)也
	// 返回一个"没有任何元素"的空Floor而不是nullptr，渲染层不用特殊处理这种情况。
	floors.clear();
	floors.reserve(expected);
	for (int idx = 0; idx < expected; idx++) {
		floors.emplace_back(idx - basements, bodySizeX, bodySizeY);
	}
	for (auto& [level, spec] : mod->floors) {
		// 模板名找不到和读档时"missing layout"一样算错误：静默当空模板会让这一层没有任何房间，
		// 开局能进、存档却读不回来。templateName为空表示这层故意不用模板。
		if (!spec.templateName.empty() && !library.HasTemplate(spec.templateName)) {
			THROW_EXCEPTION(InvalidArgumentException, "Building " + name + " floor " + to_string(level) +
				" uses missing layout template: " + spec.templateName + ".\n");
		}
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
	// ArrangeRowEach：切出的每个Room各自独立一个Component。
	for (auto& [level, slot, roomType, acreage, componentType] : mod->eachRows) {
		ArrangeRow(level, slot, roomType, acreage, nullptr, roomFactory, componentType, &componentFactory);
	}

	// 每个Room自己中心点的导航锚点，供BuildPedestrianNavigation()里"single"/"row"类型的
	// 导航端点引用。不做ConfigRoom/PlacePivots——那些依赖furniture/pivots，还没有迁移。
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

	// 带标签的地板/天花板按mod登记的标签->材质表抄上材质路径，没有标签或没登记就留空(渲染层用白色)。
	auto surfaceMaterial = [&](const string& tag) {
		if (tag.empty() || !mod) return string();
		auto found = mod->surfaceMaterials.find(tag);
		return found != mod->surfaceMaterials.end() ? found->second : string();
		};
	// 元素每面墙每个侧面的标签同样查表得到材质(同一张表，标签命名空间和地板/天花板共用)。
	auto resolveWalls = [&](auto& element) {
		for (int dir = 0; dir < 4; dir++) {
			WallStyle& style = element.MutableWallStyle(dir);
			for (int side = 0; side < 4; side++) style.materials[side] = surfaceMaterial(style.tags[side]);
		}
		};
	for (Stair stair : library.GetStairs(templateName, face)) {
		stair.InstanciateQuad(width, height);
		resolveWalls(stair);
		floor.AddStair(stair);
	}
	for (Elevator elevator : library.GetElevators(templateName, face)) {
		elevator.InstanciateQuad(width, height);
		resolveWalls(elevator);
		floor.AddElevator(elevator);
	}
	for (Ramp ramp : library.GetRamps(templateName, face)) {
		ramp.InstanciateQuad(width, height);
		resolveWalls(ramp);
		floor.AddRamp(ramp);
	}
	for (Ceiling ceiling : library.GetCeilings(templateName, face)) {
		ceiling.InstanciateQuad(width, height);
		ceiling.SetMaterial(surfaceMaterial(ceiling.GetTag()));
		floor.AddCeiling(ceiling);
	}
	for (Ground ground : library.GetGrounds(templateName, face)) {
		ground.InstanciateQuad(width, height);
		ground.SetMaterial(surfaceMaterial(ground.GetTag()));
		floor.AddGround(ground);
	}
	for (Corridor corridor : library.GetCorridors(templateName, face)) {
		corridor.InstanciateQuad(width, height);
		resolveWalls(corridor);
		floor.AddCorridor(corridor);
	}
	for (Single single : library.GetSingles(templateName, face)) {
		single.InstanciateQuad(width, height);
		resolveWalls(single);
		floor.AddSingle(single);
	}
	for (Row row : library.GetRows(templateName, face)) {
		row.InstanciateQuad(width, height);
		resolveWalls(row);
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
	room->SetDoors(single.GetDoors(), single.GetDoorTags());
	room->SetWindows(single.GetWindows());
	for (int dir = 0; dir < 4; dir++) room->SetWallStyle(dir, single.GetWallStyle(dir));
	room->SetNumber(level, floors[idx].AssignNumber());
	component->AddRoom(room);

	rooms.push_back(room);
	singleRoomBySlot[level][slot] = room;
}

void Building::ArrangeRow(int level, int slot, const string& roomType, float acreage, Component* component,
	RoomFactory& roomFactory, const string& componentType, ComponentFactory* componentFactory) {
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
		Component* owner = component;
		if (!owner) {
			if (!componentFactory) return;
			ComponentMod* componentMod = componentFactory->CreateComponent(componentType);
			if (!componentMod) continue;
			owner = new Component(componentFactory, componentMod, this);
			components.push_back(owner);
		}
		RoomMod* roomMod = roomFactory.CreateRoom(roomType);
		if (!roomMod) continue;
		Room* room = new Room(&roomFactory, roomMod, this, owner, level);

		if (alongY) {
			room->SetVertices(row.GetLeft(), row.GetBottom() + div * i, row.GetRight(), row.GetBottom() + div * (i + 1));
		} else {
			room->SetVertices(row.GetLeft() + div * i, row.GetBottom(), row.GetLeft() + div * (i + 1), row.GetTop());
		}
		room->SetDirection(row.GetDirection());
		room->SetDoors(row.GetDoors(), row.GetDoorTags());
		room->SetWindows(row.GetWindows());
		// 只继承落在联排外边界上的墙；房间之间的隔墙不带标签。
		for (int dir = 0; dir < 4; dir++) {
			bool outer;
			if (alongY) outer = (dir == FACE_WEST || dir == FACE_EAST) || (dir == FACE_NORTH && i == 0) || (dir == FACE_SOUTH && i == count - 1);
			else outer = (dir == FACE_NORTH || dir == FACE_SOUTH) || (dir == FACE_WEST && i == 0) || (dir == FACE_EAST && i == count - 1);
			if (outer) room->SetWallStyle(dir, row.GetWallStyle(dir));
		}
		room->SetNumber(level, floors[idx].AssignNumber());
		owner->AddRoom(room);

		rooms.push_back(room);
		rowRoomBySlot[level][slot].push_back(room);
	}
}

void Building::BuildDoors(Map& map) {
	for (Floor& floor : floors) {
		int level = floor.GetLevel();
		int idx = basements + level;
		if (idx < 0 || idx >= static_cast<int>(floorHeights.size())) continue;
		float floorHeight = floorHeights[idx];
		float floorBaseZ = GetFloorBaseZ(level);

		for (const Corridor& corridor : floor.GetCorridors()) {
			BuildDoorsForElement(map, corridor.GetDoors(), corridor.GetDoorTags(), mod->doorSpecs,
				corridor.GetPosX(), corridor.GetPosY(), corridor.GetSizeX(), corridor.GetSizeY(),
				floorHeight, floorBaseZ, level, DOOR_KIND_BUILDING, nullptr, this, nullptr, doorEntities);
		}
	}

	for (Room* room : rooms) {
		int level = room->GetLayer();
		int idx = basements + level;
		if (idx < 0 || idx >= static_cast<int>(floorHeights.size())) continue;
		float floorHeight = floorHeights[idx];
		float floorBaseZ = GetFloorBaseZ(level);

		vector<Door*> roomDoors;
		BuildDoorsForElement(map, room->GetDoors(), room->GetDoorTags(), room->GetMod()->doorSpecs,
			room->GetPosX(), room->GetPosY(), room->GetSizeX(), room->GetSizeY(),
			floorHeight, floorBaseZ, level, DOOR_KIND_ROOM, nullptr, this, room, roomDoors);
		for (Door* door : roomDoors) room->AddDoorEntity(door);
	}
}

const vector<Door*>& Building::GetDoorEntities() const { return doorEntities; }

void Building::BuildCabins(Map& map) {
	int floorCount = static_cast<int>(floorHeights.size());
	for (size_t i = 0; i < cabins.size(); i++) {
		const ElevatorCabinSpec& spec = cabins[i];
		int minIdx = basements + spec.minFloor, maxIdx = basements + spec.maxFloor;
		if (spec.minFloor > spec.maxFloor || minIdx < 0 || maxIdx >= floorCount) {
			debugf("Warning: elevator spec %d of building %s has invalid floor range [%d,%d], skipped.\n",
				static_cast<int>(i), name.c_str(), spec.minFloor, spec.maxFloor);
			continue;
		}
		// 同一口井里楼层范围重叠的两台电梯会撞在一起，后一条跳过。
		bool overlapped = false;
		for (const Cabin* other : cabinEntities) {
			if (other->GetShaftIndex() == spec.shaftIndex &&
				spec.minFloor <= other->GetMaxLevel() && other->GetMinLevel() <= spec.maxFloor) {
				overlapped = true;
				break;
			}
		}
		if (overlapped) {
			debugf("Warning: elevator spec %d of building %s overlaps another elevator in shaft %d, skipped.\n",
				static_cast<int>(i), name.c_str(), spec.shaftIndex);
			continue;
		}
		Cabin* cabin = map.CreateCabin(this, static_cast<int>(i), spec);
		if (cabin) cabinEntities.push_back(cabin);
	}
}

const vector<Cabin*>& Building::GetCabinEntities() const { return cabinEntities; }

float Building::GetBodyOffsetX() const { return bodyOffsetX; }
float Building::GetBodyOffsetY() const { return bodyOffsetY; }
float Building::GetBodySizeX() const { return bodySizeX; }
float Building::GetBodySizeY() const { return bodySizeY; }
int Building::GetBasementCount() const { return basements; }
int Building::GetLayerCount() const { return layers; }
const vector<float>& Building::GetFloorHeights() const { return floorHeights; }
const string& Building::GetLodMaterialPath() const { return lodMaterialPath; }
const string& Building::GetGapMaterialPath() const { return gapMaterialPath; }
const vector<DecorationInstance>& Building::GetDecorations() const { return decorations; }

const FloorLayoutSpec* Building::GetFloorSpec(int level) const {
	auto it = floorSpecs.find(level);
	return it != floorSpecs.end() ? &it->second : nullptr;
}
const unordered_map<int, FloorLayoutSpec>& Building::GetFloorSpecs() const { return floorSpecs; }
const vector<ElevatorCabinSpec>& Building::GetCabins() const { return cabins; }
const string& Building::GetStationMod() const { return stationMod; }
const vector<Node*>& Building::GetOutsideNodes() const { return outsideNodes; }
void Building::SetOutsideNodes(const vector<Node*>& nodes) { outsideNodes = nodes; }
Script* Building::GetScript() const { return script; }

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

pair<float, float> Building::QuadNorthPointToLocal(const PointParams& p) const {
	int face = (direction >= 0 && direction < 4) ? direction : FACE_NORTH;
	PointParams rotated = BuildingLayoutLibrary::InversePoint(p, face);
	return { rotated[0] * GetSizeX() + rotated[1] - GetSizeX() * 0.5f,
		rotated[2] * GetSizeY() + rotated[3] - GetSizeY() * 0.5f };
}

pair<float, float> Building::QuadLocalToWorld(float localX, float localY) const {
	float rot = GetRotation();
	float c = cosf(rot), s = sinf(rot);
	return { GetPosX() + localX * c - localY * s, GetPosY() + localX * s + localY * c };
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
		bool forward = true;   // 车行导航通行方向，见NavigationLineTemplate::forward/backward
		bool backward = false;
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
	// 车行导航不再支持"outside"端点（用entrance/exit），这份清单永远为空，只为凑BuildNavigationGraph的签名。
	vector<Node*> ignoredOutsideNodes;
	BuildNavigationGraph(library, inputDirection, /*isVehicle=*/true,
		navOut.vehicleNodes, navOut.vehicleConnections, ignoredOutsideNodes, /*registerRoomNodes=*/false,
		&navOut.vehicleEntranceNodes, &navOut.vehicleExitNodes);
}

void Building::BuildNavigationGraph(const BuildingLayoutLibrary& library, int inputDirection, bool isVehicle,
	vector<Node*>& newNodes, vector<Connection*>& newConnections, vector<Node*>& outsideNodes,
	bool registerRoomNodes, vector<Node*>* entranceNodes, vector<Node*>* exitNodes) {
	// 楼层间楼梯端点登记，留待所有楼层处理完后按相邻层贪心匹配。
	vector<tuple<int, Node*, bool>> stairEndpoints;

	for (int idx = 0; idx < static_cast<int>(floors.size()); idx++) {
		int level = idx - basements;
		const Floor& floor = floors[idx];

		auto specIt = floorSpecs.find(level);
		if (specIt == floorSpecs.end()) continue;
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
			lineRuntimes[i].forward = lineTemplate.forward;
			lineRuntimes[i].backward = lineTemplate.backward;
		}

		// 加一条from->to的连接，按通行方向展开：行人导航恒定一条（Map合并时自己做成双向），
		// 车行导航forward生成from->to、backward生成to->from，都开就是双向。
		auto addConnection = [&](Node* from, Node* to, bool forward, bool backward) {
			if (!isVehicle) {
				newConnections.push_back(new Connection(*from, *to));
				return;
			}
			if (forward) newConnections.push_back(new Connection(*from, *to));
			if (backward) newConnections.push_back(new Connection(*to, *from));
		};

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
						// 方向按模板的begin/end：投影端是begin时 projected->room，否则 room->projected。
						if (beginIsProjection) addConnection(projected, room->GetNavigationNode(), connTemplate.forward, connTemplate.backward);
						else addConnection(room->GetNavigationNode(), projected, connTemplate.forward, connTemplate.backward);
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
				if (beginIsProjection) addConnection(projected, srcNode, connTemplate.forward, connTemplate.backward);
				else addConnection(srcNode, projected, connTemplate.forward, connTemplate.backward);
				continue;
			}

			// outside端只实例化出自己的世界坐标节点收进outsideNodes，真正接路网由Map::InitBuildings做
			// (Building访问不到Map数据)，见building.md"行人导航"。
			// entrance/exit端(仅车行导航)：不生成Connection，只把另一端node记进对应清单，方向由类型决定，
			// 同一个node可以两个都标；行人导航里出现这两个类型没有意义，忽略。
			if (connTemplate.begin.type == "entrance" || connTemplate.end.type == "entrance" ||
				connTemplate.begin.type == "exit" || connTemplate.end.type == "exit") {
				bool beginIsMarker = connTemplate.begin.type == "entrance" || connTemplate.begin.type == "exit";
				const NavigationEndpointTemplate& markerSide = beginIsMarker ? connTemplate.begin : connTemplate.end;
				const NavigationEndpointTemplate& otherSide = beginIsMarker ? connTemplate.end : connTemplate.begin;
				if (!isVehicle || !entranceNodes || !exitNodes) {
					debugf("Warning: \"%s\" endpoint is only valid in vehicleNavigation, ignored.\n", markerSide.type.c_str());
					continue;
				}
				vector<Node*> otherNodes = resolveEndpoint(otherSide);
				if (otherNodes.size() != 1 || !otherNodes[0]) continue;
				(markerSide.type == "entrance" ? entranceNodes : exitNodes)->push_back(otherNodes[0]);
				continue;
			}

			// outside端：行人导航专用，车行导航改用entrance/exit。
			if (connTemplate.begin.type == "outside" || connTemplate.end.type == "outside") {
				if (isVehicle) {
					debugf("Warning: \"outside\" endpoint is not supported in vehicleNavigation, use \"entrance\"/\"exit\". Ignored.\n");
					continue;
				}
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
					addConnection(beginNode, endNode, connTemplate.forward, connTemplate.backward);
				}
			}
		}

		for (auto& line : lineRuntimes) {
			if (line.anchors.size() < 2) continue;
			sort(line.anchors.begin(), line.anchors.end(),
				[](const pair<float, Node*>& a, const pair<float, Node*>& b) { return a.first < b.first; });
			for (size_t i = 0; i + 1 < line.anchors.size(); i++) {
				addConnection(line.anchors[i].second, line.anchors[i + 1].second, line.forward, line.backward);
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
			// 上下楼的连线车行导航恒定双向（上楼下楼都能走），不受layout方向选项影响。
			if (isVehicle) newConnections.push_back(new Connection(*downList[j].first, *upList[i].first));
		}
	}

	// Room自己中心点的导航节点由Room持有(Layout()里创建)，不在floorFixedNodes/line锚点链路里，
	// 但必须登记进newNodes，否则Map::navAnchorNodes拿不到它们(可视化画不出来、也没人delete)。
	// 只有行人那次调用登记：车行再登记一遍会让同一个Node*进navAnchorNodes两次，~Map()时double free。
	if (registerRoomNodes) {
		for (Room* room : rooms) {
			if (room && room->GetNavigationNode()) {
				newNodes.push_back(room->GetNavigationNode());
			}
		}
	}
}
