#include "common/implement.h"

#include "common/utility.h"

#include "populace/populace.h"
#include "populace/citizen.h"
#include "player/player.h"


using namespace std;

PostImplement::PostImplement(Map* map, Populace* populace, Society* society, Story* story,
	Industry* industry, Traffic* traffic, Player* player) :
	map(map), populace(populace), society(society), story(story),
	industry(industry), traffic(traffic), player(player), result() {

}

void PostImplement::Post(const JsonValue& request) {
	result = JsonValue(DATA_OBJECT);

	if (request.IsObject() && request["post"].AsString() == "random citizen") {
		if (!populace || populace->GetCitizens().empty()) {
			result["result"] = "fail";
			result["msg"] = "no citizen available.";
			return;
		}

		const vector<Citizen*>& citizens = populace->GetCitizens();
		Citizen* citizen = citizens[GetRandom(static_cast<int>(citizens.size()))];
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

	result["result"] = "fail";
	result["msg"] = "post not found.";
}

const JsonValue& PostImplement::GetResult() const {
	return result;
}
