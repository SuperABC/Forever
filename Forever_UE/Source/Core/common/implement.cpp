#include "common/implement.h"

#include "common/utility.h"

#include "populace/populace.h"
#include "populace/citizen.h"
#include "player/player.h"
#include "society/job.h"
#include "map/room.h"


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

	// "citizen home address"/"citizen workplace address"：按姓名查一个citizen，返回它
	// 家/工位房间的具体地址(Room::GetAddress()，"<building地址> <门牌号>"格式)——
	// NPCNavigateChange::destination这次改成必须是这种能被Map::LocateRoom(address)直接
	// 解析回Room*的具体地址字符串，不能再写"home"/"workplace"这种描述性文本(JobMod所在
	// 的Dependence层看不到Citizen/Room这些Core类型，只能通过Post查询拿到字符串结果)，
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

	result["result"] = "fail";
	result["msg"] = "post not found.";
}

const JsonValue& PostImplement::GetResult() const {
	return result;
}
