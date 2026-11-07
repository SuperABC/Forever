#include "common/implement.h"

#include "common/utility.h"
#include "common/config.h"

#include "populace/populace.h"
#include "populace/citizen.h"
#include "player/player.h"
#include "society/job.h"
#include "map/room.h"
#include "map/map.h"
#include "map/zone.h"
#include "map/building.h"
#include "industry/industry.h"
#include "industry/product.h"


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

		// 只挑有家的：无Room的citizen会让UE层算不出世界坐标、玩家无报错地卡在飞行相机上，见implement.md
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

	if (request.IsObject() && request["post"].AsString() == "product info") {
		Product* product = industry ? industry->FindProduct(request["type"].AsString()) : nullptr;
		if (!product) {
			result["result"] = "fail";
			result["msg"] = "product not found: " + request["type"].AsString();
			return;
		}

		result["result"] = "success";
		result["retail price"] = static_cast<long long>(product->GetRetailPrice());
		result["import price"] = static_cast<long long>(product->GetImportPrice());
		result["export price"] = static_cast<long long>(product->GetExportPrice());
		result["service type"] = product->GetServiceType();
		result["service quality"] = static_cast<double>(product->GetServiceQuality());
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

	// 两个post类型共用按姓名查citizen的逻辑
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

	if (request.IsObject() && request["post"].AsString() == "resource path") {
		string path = Config::GetResourceFilePath(request["name"].AsString());
		if (path.empty()) {
			result["result"] = "fail";
			result["msg"] = "resource not found: " + request["name"].AsString();
			return;
		}

		result["result"] = "success";
		result["path"] = path;
		return;
	}

	result["result"] = "fail";
	result["msg"] = "post not found.";
}

const JsonValue& PostImplement::GetResult() const {
	return result;
}
