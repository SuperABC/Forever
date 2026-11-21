#include "party_common.h"

#include "common/json.h"
#include "common/utility.h"


using namespace std;

string PickRandomMeetingRoom(PostHandle* post) {
	if (!post) return string();
	JsonValue request(DATA_OBJECT);
	request["post"] = "random room";
	request["type"] = "room_meeting";
	post->Post(request);
	const JsonValue& result = post->GetResult();
	if (result["result"].AsString() != "success") return string();
	return result["address"].AsString();
}

void AssignPolicyPowersRandomly(const PartyContext& context, vector<PowerAssignment>& out) {
	out.clear();
	if (context.members.empty()) return;
	for (const PowerInfo& power : context.powers) {
		if (power.policy.empty()) continue;
		PowerAssignment assignment;
		assignment.member = context.members[static_cast<size_t>(GetRandom(static_cast<int>(context.members.size())))];
		assignment.power = power.id;
		out.push_back(assignment);
	}
}
