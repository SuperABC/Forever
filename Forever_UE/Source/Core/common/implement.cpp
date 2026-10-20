#include "common/implement.h"

#include "common/utility.h"

#include "populace/populace.h"
#include "populace/citizen.h"
#include "player/player.h"
#include "society/job.h"
#include "map/room.h"
#include "map/map.h"
#include "map/zone.h"
#include "map/building.h"


using namespace std;

PostImplement::PostImplement(Map* map, Populace* populace, Society* society, Story* story,
	Industry* industry, Traffic* traffic, Player* player) :
	map(map), populace(populace), society(society), story(story),
	industry(industry), traffic(traffic), player(player), result() {

}

void PostImplement::Post(const JsonValue& request) {
	result = JsonValue(DATA_OBJECT);

	if (request.IsObject() && request["post"].AsString() == "random citizen") {
		if (!populace) {
			result["result"] = "fail";
			result["msg"] = "no citizen available.";
			return;
		}

		// 只从"已经有家"(GetRoom()非空)的citizen里挑——地图生成的住宅room数量如果不够
		// 覆盖所有成年citizen("一个家庭消费一个room名额"的池子提前耗尽)，会有一部分citizen
		// 始终没有Room/CurrentRoom；这种citizen如果被挑中当"交给玩家操控"的目标，
		// UForeverPopulaceFrameworkComponent::ComputeLogicalPosition算不出世界坐标，
		// SpawnCitizen/FindOrSpawnCitizenByName返回nullptr，玩家会停留在开局的
		// ADefaultPawn(飞行相机)上且没有任何报错弹窗。
		vector<Citizen*> housed;
		for (Citizen* citizen : populace->GetCitizens()) {
			if (citizen && citizen->GetRoom()) housed.push_back(citizen);
		}
		if (housed.empty()) {
			result["result"] = "fail";
			result["msg"] = "no housed citizen available.";
			return;
		}

		Citizen* citizen = housed[GetRandom(static_cast<int>(housed.size()))];
		result["result"] = "success";
		result["name"] = citizen->GetName();
		return;
	}

	if (request.IsObject() && request["post"].AsString() == "game time") {
		if (!player || !player->GetTime()) {
			result["result"] = "fail";
			result["msg"] = "no game time available.";
			return;
		}

		result["result"] = "success";
		result["date"] = player->GetTime()->Format("YYYY-MM-DD");
		result["time"] = player->GetTime()->Format("HH:mm");
		return;
	}

	// "citizen home address"/"citizen workplace address"：按姓名查一个citizen，返回它
	// 家/工位房间的具体地址(Room::GetAddress()，"<building地址> <门牌号>"格式)——
	// NPCNavigateChange::destination必须是这种能被Map::LocateRoom(address)直接解析回
	// Room*的具体地址字符串，不能写"home"/"workplace"这种描述性文本(JobMod所在的
	// Dependence层看不到Citizen/Room这些Core类型，只能通过Post查询拿到字符串结果)，
	// 见Dependence/society/job_mod.h。两个post类型共用同一段按姓名查citizen的逻辑。
	if (request.IsObject() && (request["post"].AsString() == "citizen home address" ||
		request["post"].AsString() == "citizen workplace address")) {
		bool wantHome = request["post"].AsString() == "citizen home address";
		string name = request["name"].AsString();

		if (!populace) {
			result["result"] = "fail";
			result["msg"] = "no populace available.";
			return;
		}

		Citizen* citizen = nullptr;
		for (Citizen* candidate : populace->GetCitizens()) {
			if (candidate && candidate->GetName() == name) {
				citizen = candidate;
				break;
			}
		}
		if (!citizen) {
			result["result"] = "fail";
			result["msg"] = "citizen not found: " + name;
			return;
		}

		Room* room = wantHome ? citizen->GetRoom()
			: (citizen->GetJob() ? citizen->GetJob()->GetPosition() : nullptr);
		if (!room) {
			result["result"] = "fail";
			result["msg"] = wantHome ? "citizen has no home room." : "citizen has no job/workplace room.";
			return;
		}

		result["result"] = "success";
		result["address"] = room->GetAddress();
		return;
	}

	// "list zones"/"list buildings"/"list rooms"：request["type"]是要找的mod type(Zone/Building/
	// Room::GetType()，比如"yuanshen")，返回地图上所有该type的实例的地址(GetAddress())数组，成功
	// 返回{"result":"success","addresses":[...]}，没有匹配也是success+空数组；map为空返回fail。
	// 类型全图唯一的mod(比如只会放一栋的测试建筑)取addresses[0]即可。Map的zones/buildings是
	// 无序容器，返回顺序不保证稳定；rooms遍历每个building->GetRooms()。
	if (request.IsObject() && (request["post"].AsString() == "list zones" ||
		request["post"].AsString() == "list buildings" || request["post"].AsString() == "list rooms")) {
		string kind = request["post"].AsString();
		string type = request["type"].AsString();

		if (!map) {
			result["result"] = "fail";
			result["msg"] = "no map available.";
			return;
		}

		JsonValue addresses(DATA_ARRAY);
		if (kind == "list zones") {
			for (const auto& [name, zone] : map->GetZones()) {
				if (zone && zone->GetType() == type) addresses.append(JsonValue(zone->GetAddress()));
			}
		} else if (kind == "list buildings") {
			for (const auto& [name, building] : map->GetBuildings()) {
				if (building && building->GetType() == type) addresses.append(JsonValue(building->GetAddress()));
			}
		} else {
			for (const auto& [name, building] : map->GetBuildings()) {
				if (!building) continue;
				for (Room* room : building->GetRooms()) {
					if (room && room->GetType() == type) addresses.append(JsonValue(room->GetAddress()));
				}
			}
		}
		result["result"] = "success";
		result["addresses"] = addresses;
		return;
	}

	result["result"] = "fail";
	result["msg"] = "post not found.";
}

const JsonValue& PostImplement::GetResult() const {
	return result;
}
