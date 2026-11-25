#include "society/immigration.h"

#include "common/random.h"
#include "common/utility.h"

#include "society/society.h"
#include "society/settlement.h"
#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "populace/populace.h"
#include "populace/citizen.h"

#include <algorithm>
#include <cmath>


using namespace std;

namespace {
	constexpr float kMaxRate = 0.005f; // r的上限
	constexpr int kRateSteps = 10000;
}

vector<Citizen*> Immigration::Daily(Society& society, Populace& populace, Map& map, const Time& now) {
	CORE_RANDOM_SCOPE("core.society");
	vector<Citizen*> arrived;
	int x = populace.GetInitialPopulation();
	if (x <= 0) return arrived;

	int y = 0;
	for (const Citizen* citizen : populace.GetCitizens()) {
		if (citizen && citizen->IsActive()) y++;
	}

	double rate = kMaxRate * static_cast<double>(GetRandom(kRateSteps + 1)) / kRateSteps;
	// 目标人口 = 开局人口 × 政策"移民目标比例"(默认1.5，由政治域调整)。
	double expected = (society.GetPolicy().immigrationTargetRatio * x - y) * rate;
	if (expected <= 0.0) return arrived;
	int count = static_cast<int>(expected);
	if (static_cast<double>(GetRandom(kRateSteps)) / kRateSteps < expected - count) count++;
	if (count <= 0) return arrived;

	// 没人住的住宅房间。
	vector<Room*> vacancies;
	for (const auto& [name, building] : map.GetBuildings()) {
		if (!building) continue;
		for (Room* room : building->GetRooms()) {
			if (room && room->IsResidence() && room->GetHouseholds().empty()) vacancies.push_back(room);
		}
	}

	for (int i = 0; i < count && !vacancies.empty(); i++) {
		int index = GetRandom(static_cast<int>(vacancies.size()));
		Room* home = vacancies[index];
		vacancies[index] = vacancies.back();
		vacancies.pop_back();

		Citizen* citizen = populace.SpawnImmigrant(now.GetYear());
		if (!citizen) break;
		citizen->SetInEconomy(true);
		Settlement::InitFunds(society, citizen, -1, false);
		map.SettleCitizen(citizen, home);
		society.RecruitCitizen(citizen, now);
		arrived.push_back(citizen);
	}
	return arrived;
}
