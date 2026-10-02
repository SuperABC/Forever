#include "traffic.h"

#include "common/registry.h"
#include "common/utility.h"
#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "story/change.h"

#include <algorithm>

using namespace std;

Traffic::Traffic() :
	vehicleFactory(Registry::Get().GetVehicleFactory()),
	scriptFactory(Registry::Get().GetScriptFactory()) {
}

Traffic::~Traffic() {
	for (auto& [name, vehicle] : vehicles) {
		delete vehicle;
	}
}

void Traffic::Init(Map* map) {
	if (!map) return;

	int parkingRoomCount = 0;

	for (auto& [buildingName, building] : map->GetBuildings()) {
		if (!building) continue;
		for (Room* room : building->GetRooms()) {
			if (!room || !room->IsParking()) continue;
			parkingRoomCount++;

			for (const ParkingSpot& spot : room->GetParkingSpots()) {
				vector<string> ids = vehicleFactory.GetRegisteredIds();
				ids.erase(remove(ids.begin(), ids.end(), "empty"), ids.end());
				if (ids.empty()) continue; // 没有真正可用的车型，跳过这个车位

				string modId = ids[GetRandom(static_cast<int>(ids.size()))];
				string name = "Vehicle" + to_string(vehicleCounter++);

				Vehicle* vehicle = CreateVehicle(modId, name);
				if (!vehicle) continue;

				// room->GetPosX()/GetPosY()已经是房间自己的中心点(Quad的约定，不是角点)，
				// 这里的ratio要按"相对房间中心的偏移"解释——ratio=0.5意味着偏移0(正中心)，
				// ratio=0/1分别是房间两侧边缘(-/+半边长)，不能直接当成"相对角点"的0~1。
				// 之前漏了减0.5这一步，ratio=0.5算出来的偏移是半个房间宽/高，车直接出现在
				// 房间角落上，见GenerateVehicles()最终换算成世界坐标的ComputeWorldPosition。
				float localX = (spot.position[0] - 0.5f) * room->GetSizeX() + spot.position[1];
				float localY = (spot.position[2] - 0.5f) * room->GetSizeY() + spot.position[3];
				vehicle->SetParking(room, localX, localY, spot.rotationDegrees);

				// 停车场这次都在Shop/Factory地下室里，生成是随机的(Shop要地块够大+50%概率，
				// Factory要地块够大)，玩家靠自己在地图上找会很难定位——打一条日志说清楚"这辆车
				// 在哪栋楼"，见Room::GetAddress()("<building地址> <门牌号>"格式)。
				debugf("Vehicle %s parked at %s.\n", name.c_str(), room->GetAddress().c_str());
			}
		}
	}

	debugf("Traffic::Init: found %d parking room(s), generated %d vehicle(s).\n",
		parkingRoomCount, static_cast<int>(vehicles.size()));
}

Vehicle* Traffic::CreateVehicle(const string& modId, const string& name) {
	auto it = vehicles.find(name);
	if (it != vehicles.end()) {
		delete it->second;
		vehicles.erase(it);
	}

	Vehicle* vehicle = new Vehicle(&vehicleFactory, &scriptFactory, modId, name);
	if (!vehicle->IsValid()) {
		delete vehicle;
		return nullptr;
	}

	vehicles.insert_or_assign(name, vehicle);
	return vehicle;
}

void Traffic::DestroyVehicle(const string& name) {
	auto it = vehicles.find(name);
	if (it == vehicles.end()) return;
	delete it->second;
	vehicles.erase(it);
}

Vehicle* Traffic::FindVehicleByName(const string& name) const {
	auto it = vehicles.find(name);
	return it != vehicles.end() ? it->second : nullptr;
}

const unordered_map<string, Vehicle*>& Traffic::GetVehicles() const { return vehicles; }

void Traffic::Tick(const Time& currentTime, bool crossedDay, PostHandle* post) {
	// 占位，等Traffic域真的有需要每帧处理的逻辑时再补，见traffic.h声明处注释。
}

void Traffic::ApplyChange(const Change* change, const ScriptContext& context) {
	if (auto* addOption = dynamic_cast<const AddOptionChange*>(change)) {
		Vehicle* target = FindVehicleByName(ToString(EvaluateExpression(addOption->GetName(), context)));
		if (target) target->AddOption(ToString(EvaluateExpression(addOption->GetOption(), context)));
		return;
	}
}
