#include "room.h"

#include "common/error.h"
#include "common/random.h"

#include "map/building.h"
#include "map/door.h"
#include "player/asset.h"
#include "populace/citizen.h"
#include "populace/household.h"
#include "story/script.h"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>


using namespace std;

Room::Room(RoomFactory* factory, RoomMod* mod, Building* parentBuilding, Component* parentComponent, int layer) :
	Quad(),
	mod(mod),
	factory(factory),
	type(),
	name(),
	parentBuilding(parentBuilding),
	parentComponent(parentComponent),
	layer(layer) {
	if (!mod) {
		THROW_EXCEPTION(DataException, "Room mod is null.\n");
	}

	MOD_CALL(mod, mod->SetProperty());
	type = mod->GetType();
	name = mod->GetName();
	properties.isResidence = mod->isResidence;
	properties.residenceCapacity = mod->residenceCapacity;
	properties.isWorkspace = mod->isWorkspace;
	properties.workspaceCapacity = mod->workspaceCapacity;
	properties.isStorage = mod->isStorage;
	properties.storageType = mod->storageType;
	properties.rent = mod->rent;
	properties.isManufacture = mod->isManufacture;
	properties.manufactureTypes = mod->manufactureTypes;
	scriptModName = mod->scriptModName;
	scriptMilestones = mod->milestoneNames;
}

Room::~Room() {
	for (Household* household : households) delete household;
	for (ParkingSlot* slot : parkingSlots) delete slot;
	factory->DestroyRoom(mod);
	for (auto& [name, asset] : assets) delete asset; // 没人捡走的地上物品，房间销毁时清理
	for (Door* door : doorEntities) delete door;
	delete script;
}

string Room::GetType() const { return type; }
string Room::GetName() const { return name; }
RoomMod* Room::GetMod() const { return mod; }
int Room::GetLayer() const { return layer; }
int Room::GetDirection() const { return direction; }
void Room::SetDirection(int dir) { direction = dir; }

// 朝向无效按朝北；W/E时u、v两个轴和建筑局部X/Y对调。
static int EffectiveFace(int direction) {
	return (direction >= 0 && direction < 4) ? direction : FACE_NORTH;
}

float Room::AxisSizeU() const {
	int face = EffectiveFace(direction);
	return (face == FACE_WEST || face == FACE_EAST) ? GetSizeY() : GetSizeX();
}

float Room::AxisSizeV() const {
	int face = EffectiveFace(direction);
	return (face == FACE_WEST || face == FACE_EAST) ? GetSizeX() : GetSizeY();
}

void Room::LocalToBuildingLocal(float u, float v, float& offsetX, float& offsetY) const {
	float halfX = GetSizeX() * 0.5f, halfY = GetSizeY() * 0.5f;
	switch (EffectiveFace(direction)) {
	case FACE_WEST:  offsetX = v - halfX; offsetY = halfY - u; break;
	case FACE_EAST:  offsetX = halfX - v; offsetY = u - halfY; break;
	case FACE_SOUTH: offsetX = halfX - u; offsetY = halfY - v; break;
	default:         offsetX = u - halfX; offsetY = v - halfY; break; // FACE_NORTH
	}
}

float Room::FacingYawDegrees() const {
	static const float kFacingYawDegrees[4] = { 0.f, 180.f, 90.f, 270.f }; // 下标=FACE_WEST/EAST/NORTH/SOUTH；必须和LocalToBuildingLocal的v轴方向一致(WEST时v->+X、EAST时v->-X)，CoreTest save套件校验
	return kFacingYawDegrees[EffectiveFace(direction)];
}

pair<float, float> Room::LocalToWorld(float u, float v) const {
	float offsetX = 0.f, offsetY = 0.f;
	LocalToBuildingLocal(u, v, offsetX, offsetY);
	return parentBuilding->LocalToWorld(GetPosX() + offsetX, GetPosY() + offsetY);
}

float Room::LocalYawToWorld(float yawDegrees) const {
	const float degToRad = 3.14159265358979323846f / 180.f;
	return parentBuilding->GetRotation() + (FacingYawDegrees() + yawDegrees) * degToRad;
}

// ---- 家具 ----

namespace {
	// 地图单位1=10米。
	constexpr float kMetersPerUnit = 10.f;
}

// 建筑局部相对房间中心的偏移 -> 房间局部(u,v)地图单位，是LocalToBuildingLocal的逆。
static void BuildingOffsetToLocal(int face, float halfX, float halfY, float ox, float oy, float& u, float& v) {
	switch (face) {
	case FACE_WEST:  v = ox + halfX; u = halfY - oy; break;
	case FACE_EAST:  v = halfX - ox; u = oy + halfY; break;
	case FACE_SOUTH: u = halfX - ox; v = halfY - oy; break;
	default:         u = ox + halfX; v = oy + halfY; break; // FACE_NORTH
	}
}

RoomLayoutInfo Room::BuildLayoutInfo() const {
	RoomLayoutInfo info;
	float widthUnits = AxisSizeU(), depthUnits = AxisSizeV();
	info.width = widthUnits * kMetersPerUnit;
	info.depth = depthUnits * kMetersPerUnit;

	const vector<float>& heights = parentBuilding->GetFloorHeights();
	int idx = parentBuilding->GetBasementCount() + layer;
	float floorHeight = (idx >= 0 && idx < static_cast<int>(heights.size())) ? heights[idx] : 0.4f;
	info.height = floorHeight * kMetersPerUnit;

	int face = EffectiveFace(direction);
	float halfX = GetSizeX() * 0.5f, halfY = GetSizeY() * 0.5f;
	// 建筑局部墙(按FACE)上沿墙量的s(地图单位，0=沿墙最小端)转相对房间中心的偏移；公式同building.cpp的ComputeDoorFaces。
	auto wallPoint = [&](int wallFace, float s, float& ox, float& oy) {
		switch (wallFace) {
		case FACE_WEST:  ox = -halfX; oy = -halfY + s; break;
		case FACE_EAST:  ox = halfX;  oy = -halfY + s; break;
		case FACE_NORTH: ox = -halfX + s; oy = -halfY; break;
		default:         ox = -halfX + s; oy = halfY;  break; // FACE_SOUTH
		}
	};
	auto convert = [&](const WallHole& holes, const DoorTags* tags, vector<RoomOpening>& out) {
		for (const auto& [wallFace, positions] : holes) {
			if (wallFace < 0 || wallFace >= 4) continue;
			float span = (wallFace == FACE_WEST || wallFace == FACE_EAST) ? GetSizeY() : GetSizeX();
			auto tagIt = tags ? tags->find(wallFace) : DoorTags::const_iterator();
			for (size_t i = 0; i < positions.size(); i++) {
				const RectParams& p = positions[i];
				float s1 = p[0] * span + p[1], s2 = p[4] * span + p[5];
				float ox1, oy1, ox2, oy2, u1, v1, u2, v2;
				wallPoint(wallFace, s1, ox1, oy1);
				wallPoint(wallFace, s2, ox2, oy2);
				BuildingOffsetToLocal(face, halfX, halfY, ox1, oy1, u1, v1);
				BuildingOffsetToLocal(face, halfX, halfY, ox2, oy2, u2, v2);

				RoomOpening opening;
				bool alongU = fabsf(v1 - v2) < fabsf(u1 - u2); // 洞沿u方向延伸 = 在正面/背面墙上
				if (alongU) {
					opening.wall = (v1 + v2) * 0.5f > depthUnits * 0.5f ? ROOM_WALL_FRONT : ROOM_WALL_BACK;
					opening.a0 = min(u1, u2); opening.a1 = max(u1, u2);
				} else {
					opening.wall = (u1 + u2) * 0.5f > widthUnits * 0.5f ? ROOM_WALL_U_MAX : ROOM_WALL_U_MIN;
					opening.a0 = min(v1, v2); opening.a1 = max(v1, v2);
				}
				opening.a0 *= kMetersPerUnit; opening.a1 *= kMetersPerUnit;
				// y1/y2是"从天花板往下量"的距离(和BuildDoorsForElement一致)。
				float y1 = p[2] * floorHeight + p[3], y2 = p[6] * floorHeight + p[7];
				opening.z0 = (floorHeight - max(y1, y2)) * kMetersPerUnit;
				opening.z1 = (floorHeight - min(y1, y2)) * kMetersPerUnit;
				if (tags && tagIt != tags->end() && i < tagIt->second.size()) opening.tag = tagIt->second[i];
				out.push_back(opening);
			}
		}
	};
	convert(doors, &doorTags, info.doors);
	convert(windows, nullptr, info.windows);
	return info;
}

void Room::LayoutFurniture() {
	furniture.clear();
	if (!mod) return;
	{
		// 房间mod在自己的随机流里摆(GetRandom可复现，--seed按mod id生效)。
		RandomScope scope(RandomRegistry::GetInstanceStream(mod));
		MOD_CALL(mod, mod->Layout(BuildLayoutInfo()));
	}
	float baseZ = parentBuilding->GetFloorWorldZ(layer);
	for (const FurnitureSpec& spec : mod->furniture) { // 只读，不改mod的容器(跨DLL分配器)
		DecorationInstance instance;
		instance.meshPath = spec.meshPath;
		pair<float, float> world = LocalToWorld(spec.u / kMetersPerUnit, spec.v / kMetersPerUnit);
		instance.x = world.first;
		instance.y = world.second;
		instance.z = baseZ + spec.z / kMetersPerUnit;
		instance.yaw = LocalYawToWorld(spec.yaw);
		instance.scaleX = spec.scaleX; instance.scaleY = spec.scaleY; instance.scaleZ = spec.scaleZ;
		furniture.push_back(std::move(instance));
	}
}

const vector<DecorationInstance>& Room::GetFurniture() const { return furniture; }

Building* Room::GetParentBuilding() const { return parentBuilding; }
Component* Room::GetParentComponent() const { return parentComponent; }
const WallHole& Room::GetDoors() const { return doors; }
const DoorTags& Room::GetDoorTags() const { return doorTags; }
const WallHole& Room::GetWindows() const { return windows; }
void Room::SetDoors(WallHole d, DoorTags tags) { doors = std::move(d); doorTags = std::move(tags); }
void Room::SetWindows(WallHole w) { windows = std::move(w); }
const WallStyle& Room::GetWallStyle(int dir) const { return wallStyles[dir < 0 || dir >= 4 ? 0 : dir]; }
void Room::SetWallStyle(int dir, const WallStyle& style) { if (dir >= 0 && dir < 4) wallStyles[dir] = style; }

void Room::AddDoorEntity(Door* door) {
	if (door) doorEntities.push_back(door);
}

const vector<Door*>& Room::GetDoorEntities() const { return doorEntities; }
const string& Room::GetNumber() const { return number; }

void Room::SetNumber(int level, int seq) {
	ostringstream oss;
	if (level < 0) {
		oss << 'b' << -level;
	} else {
		oss << level;
	}
	oss << setw(4) << setfill('0') << seq;
	number = oss.str();
}

Script* Room::GetScript() const { return script; }

Node* Room::GetNavigationNode() const { return navigationNode; }
void Room::SetNavigationNode(Node* node) { navigationNode = node; }

bool Room::IsResidence() const { return properties.isResidence; }
int Room::ResidenceCapacity() const { return properties.residenceCapacity; }
bool Room::IsWorkspace() const { return properties.isWorkspace; }
int Room::WorkspaceCapacity() const { return properties.workspaceCapacity; }
bool Room::IsStorage() const { return properties.isStorage; }
string Room::StorageType() const { return properties.storageType; }
int64_t Room::Rent() const { return properties.rent; }
bool Room::IsManufacture() const { return properties.isManufacture; }
vector<string> Room::ManufactureTypes() const { return properties.manufactureTypes; }

bool ParkingSlot::Accepts(const string& category) const {
	return std::find(categories.begin(), categories.end(), category) != categories.end();
}

float ParkingSlot::FacingYawDegrees() const {
	// 建筑局部坐标里北=-Y、南=+Y(见building.cpp ComputeDoorFaces)，和Room::FacingYawDegrees(房间网格朝向约定)不同，不能共用。
	static const float kFacingYawDegrees[4] = { 180.f, 0.f, 270.f, 90.f }; // 下标=FACE_WEST/EAST/NORTH/SOUTH
	return kFacingYawDegrees[EffectiveFace(direction)];
}

float ParkingSlot::VehicleYawDegrees() const {
	float yaw = FacingYawDegrees() + 180.f;
	return yaw >= 360.f ? yaw - 360.f : yaw;
}

bool ParkingSlot::Contains(float x, float y) const {
	// 开口朝向W/E时深度沿X、宽度沿Y，N/S反之。
	int face = EffectiveFace(direction);
	bool depthAlongX = (face == FACE_WEST || face == FACE_EAST);
	float hx = depthAlongX ? halfDepth : halfWidth;
	float hy = depthAlongX ? halfWidth : halfDepth;
	return fabsf(x - centerX) <= hx && fabsf(y - centerY) <= hy;
}

const vector<ParkingSlot*>& Room::GetParkingSlots() const { return parkingSlots; }

ParkingSlot* Room::AddParkingSlot(const ParkingSlot& slot) {
	ParkingSlot* created = new ParkingSlot(slot);
	parkingSlots.push_back(created);
	return created;
}

int Room::FindFreeSlot(const string& category) const {
	for (size_t i = 0; i < parkingSlots.size(); i++) {
		if (parkingSlots[i]->IsFree() && parkingSlots[i]->Accepts(category)) return static_cast<int>(i);
	}
	return -1;
}

bool Room::HasSlotFor(const string& category) const {
	for (const ParkingSlot* slot : parkingSlots) {
		if (slot->Accepts(category)) return true;
	}
	return false;
}

int Room::FindSlotContaining(float x, float y, float yawDegrees) const {
	const float degToRad = 3.14159265358979323846f / 180.f;
	for (size_t i = 0; i < parkingSlots.size(); i++) {
		const ParkingSlot* slot = parkingSlots[i];
		if (slot->parkedVehicle || !slot->Contains(x, y)) continue;
		// 车头朝向与车位开口朝向相反：点积<-0.8。
		float dot = cosf((yawDegrees - slot->FacingYawDegrees()) * degToRad);
		if (dot < -0.8f) return static_cast<int>(i);
	}
	return -1;
}

string Room::GetAddress() const {
	if (!parentBuilding) return number;
	return parentBuilding->GetAddress() + " " + number;
}

Citizen* Room::GetOwner() const { return owner; }
void Room::SetOwner(Citizen* value) { owner = value; }
bool Room::GetStated() const { return stated; }
void Room::SetStated(bool value) { stated = value; }

const vector<Household*>& Room::GetHouseholds() const { return households; }

Household* Room::AddHousehold() {
	Household* household = new Household(this);
	households.push_back(household);
	return household;
}

Household* Room::AddResident(Citizen* citizen) {
	if (!citizen) return nullptr;

	// 找出与新住户有亲属关系的全部户：只有一户就并入，有多户说明新住户把它们连起来了，合并成一户。
	Household* target = nullptr;
	for (size_t i = 0; i < households.size();) {
		Household* household = households[i];
		if (household->IsRelatedTo(citizen)) {
			if (!target) {
				target = household;
				i++;
			}
			else {
				target->Absorb(*household);
				for (Citizen* member : target->GetMembers()) member->SetHousehold(target);
				delete household;
				households.erase(households.begin() + i);
			}
		}
		else {
			i++;
		}
	}
	if (!target) target = AddHousehold();
	target->AddMember(citizen);
	citizen->SetHousehold(target);
	return target;
}

const vector<Citizen*>& Room::GetOccupants() const { return occupants; }

void Room::AddOccupant(Citizen* citizen) {
	if (citizen) occupants.push_back(citizen);
}

void Room::RemoveOccupant(Citizen* citizen) {
	occupants.erase(remove(occupants.begin(), occupants.end(), citizen), occupants.end());
}

const unordered_map<string, Asset*>& Room::GetAssets() const { return assets; }

void Room::AddAsset(Asset* asset) {
	if (asset) assets.insert_or_assign(asset->GetName(), asset);
}

Asset* Room::RemoveAsset(const string& name) {
	auto it = assets.find(name);
	if (it == assets.end()) return nullptr;
	Asset* removed = it->second;
	assets.erase(it);
	return removed;
}
