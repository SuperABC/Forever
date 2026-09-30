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
		if (!populace) {
			result["result"] = "fail";
			result["msg"] = "no citizen available.";
			return;
		}

		// 只从"已经有家"(GetRoom()非空)的citizen里挑——Map::Checkin()按住宅room名额分配
		// 住处，如果某一局地图生成的住宅room数量不够覆盖所有成年citizen("一个家庭消费一个
		// room名额"的池子提前耗尽)，会有一部分citizen始终没有Room/CurrentRoom。这种citizen
		// 被这里挑中当"交给玩家操控"的目标时，UForeverPopulaceFrameworkComponent::
		// ComputeLogicalPosition算不出世界坐标(citizen没有HasPosition()也没有CurrentRoom)，
		// SpawnCitizen/FindOrSpawnCitizenByName返回nullptr，ApplyControlChange只打一条
		// Warning日志就直接return，玩家会一直留在开局的ADefaultPawn(飞行相机)上，没有任何
		// 报错弹窗——这就是"有时候开局停在飞行相机"这个bug的根因，出现与否取决于这次随机
		// 挑到的citizen恰不恰好是没分到房子的那一个。
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
