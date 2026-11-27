#include "populace/demography.h"

#include "common/random.h"
#include "common/utility.h"

#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "map/zone.h"
#include "player/estate_asset.h"
#include "player/vehicle_asset.h"
#include "populace/citizen.h"
#include "populace/household.h"
#include "populace/kinship.h"
#include "populace/populace.h"
#include "society/emigration.h"
#include "society/job.h"
#include "society/ledger.h"
#include "society/society.h"

#include <algorithm>
#include <unordered_set>


using namespace std;

namespace {
	constexpr int kChanceSteps = 1 << 30;

	// 按概率p抽一次(p<=0不抽直接false)。
	bool Chance(double p) {
		if (p <= 0.0) return false;
		return static_cast<double>(GetRandom(kChanceSteps)) / kChanceSteps < p;
	}

	Time DateOf(const Time& now) { return Time(now.GetYear(), now.GetMonth(), now.GetDay()); }

	bool IsBirthday(const Citizen* citizen, const Time& now) {
		return citizen->GetBirthMonth() == now.GetMonth() && citizen->GetBirthDay() == now.GetDay();
	}

	// 家里除了自己还有没有活着的人。
	bool LivesWithOthers(const Citizen* citizen) {
		const Room* home = citizen->GetRoom();
		if (!home) return false;
		for (const Household* household : home->GetHouseholds()) {
			for (const Citizen* member : household->GetMembers()) {
				if (member && member != citizen && member->IsActive()) return true;
			}
		}
		return false;
	}

	// 把资产从死者的列表挪到继承人的列表(heir为空=只从死者那里摘掉)。
	void MoveAsset(Citizen* from, Citizen* heir, Asset* asset) {
		from->RemoveAsset(asset);
		if (heir) heir->AddAsset(asset);
	}
}

vector<Citizen*> Demography::Kill(Map& map, Society& society, Citizen* citizen) {
	vector<Citizen*> heirs;
	if (!citizen || !citizen->IsActive()) return heirs;

	// 继承人：在世配偶；没有就在世的子女(按出生顺序)。
	if (citizen->HasLivingSpouse()) heirs.push_back(citizen->GetSpouse());
	else {
		for (Citizen* child : citizen->GetChildren()) {
			if (child && child->IsActive()) heirs.push_back(child);
		}
	}
	Citizen* firstHeir = heirs.empty() ? nullptr : heirs.front();

	// 遗产：先清算成现金(和离城同一套)，正余额分给继承人(除不尽的零头给第一个)，没有继承人归政府；欠款由城市外核销。
	Ledger& ledger = society.GetLedger();
	Emigration::Liquidate(society, citizen);
	int64_t balance = ledger.Balance(Holder::Of(citizen));
	if (balance < 0) ledger.Transfer(Holder::Outside(), Holder::Of(citizen), -balance, Flow::DEBT_WRITE_OFF);
	else if (balance > 0) {
		if (heirs.empty()) ledger.Transfer(Holder::Of(citizen), Holder::Government(), balance, Flow::INHERIT);
		else {
			const int64_t share = balance / static_cast<int64_t>(heirs.size());
			const int64_t remainder = balance - share * static_cast<int64_t>(heirs.size());
			for (size_t i = 0; i < heirs.size(); i++) {
				ledger.Transfer(Holder::Of(citizen), Holder::Of(heirs[i]), share + (i == 0 ? remainder : 0), Flow::INHERIT);
			}
		}
	}

	// 名下的不动产和载具：给第一个继承人；没有继承人不动产清空所有者，载具留着不动(没人会开它)。
	const vector<Asset*> assets = citizen->GetAssets();
	for (Asset* asset : assets) {
		if (EstateAsset* estate = dynamic_cast<EstateAsset*>(asset)) {
			switch (estate->GetLevel()) {
			case EstateAsset::Level::Zone: if (estate->GetZone()) estate->GetZone()->SetOwner(firstHeir); break;
			case EstateAsset::Level::Building: if (estate->GetBuilding()) estate->GetBuilding()->SetOwner(firstHeir); break;
			case EstateAsset::Level::Room: if (estate->GetRoom()) estate->GetRoom()->SetOwner(firstHeir); break;
			}
			estate->SetOwner(firstHeir);
			MoveAsset(citizen, firstHeir, estate);
		}
		else if (VehicleAsset* vehicle = dynamic_cast<VehicleAsset*>(asset)) {
			if (!firstHeir) continue;
			vehicle->SetOwnerCitizen(firstHeir);
			MoveAsset(citizen, firstHeir, vehicle);
		}
	}
	// 没挂在资产列表上的归属(保险起见，同离城扫一遍)。
	for (const auto& [name, building] : map.GetBuildings()) {
		if (!building) continue;
		if (building->GetOwner() == citizen) building->SetOwner(firstHeir);
		for (Room* room : building->GetRooms()) {
			if (room && room->GetOwner() == citizen) room->SetOwner(firstHeir);
		}
	}
	for (const auto& [name, zone] : map.GetZones()) {
		if (zone && zone->GetOwner() == citizen) zone->SetOwner(firstHeir);
	}

	if (Job* job = citizen->GetJob()) {
		job->SetOccupant(nullptr);
		citizen->SetJob(nullptr);
	}
	citizen->ClearPregnancy();
	citizen->SetPendingMoveOut(false);
	citizen->Die();
	// 移出住户：独居的人死了房子就空出来。家(GetRoom)留着当记录。
	if (Room* home = citizen->GetRoom()) home->RemoveResident(citizen);
	return heirs;
}

DemographyReport Demography::Daily(Populace& populace, Map& map, Society& society, const Time& now,
	const Citizen* controlled, const DemographyConfig& config) {
	CORE_RANDOM_SCOPE("core.populace");
	DemographyReport report;
	const Time today = DateOf(now);
	// 出生会往citizens末尾追加，按开始时的人数遍历(新生儿当天不参与后面的步骤)。
	const vector<Citizen*>& citizens = populace.GetCitizens();
	const size_t count = citizens.size();

	// 1. 自然死亡：每天概率(年龄-60)×0.00001。
	for (size_t i = 0; i < count; i++) {
		Citizen* citizen = citizens[i];
		if (!citizen || !citizen->IsActive() || citizen == controlled) continue;
		const int age = citizen->GetAge(now);
		if (age < config.deathStartAge) continue;
		if (!Chance((age - config.deathStartAge) * config.deathRatePerYear)) continue;
		DemographyReport::Death death;
		death.citizen = citizen;
		death.age = age;
		death.heirs = Kill(map, society, citizen);
		report.deaths.push_back(death);
	}

	// 2. 出生：怀孕到预产期的母亲生下孩子。孩子住父母共同的家，父母不住一起(开局遗留)就住母亲家。
	for (size_t i = 0; i < count; i++) {
		Citizen* mother = citizens[i];
		if (!mother || !mother->IsActive() || !mother->IsPregnant()) continue;
		if (today < DateOf(mother->GetPregnancyDue())) continue;
		Citizen* father = mother->GetSpouse();
		mother->ClearPregnancy();
		mother->SetLastBirth(today);
		Citizen* child = populace.SpawnNewborn(father ? father : mother, today);
		if (!child) continue; // 取名一直撞车(理论上不会)：这次生育作废
		child->SetInEconomy(true); // 账户第一次有进账时才建，开局余额就是0
		Room* home = mother->GetRoom();
		if (father && father->IsActive() && father->GetRoom() && father->GetRoom() == mother->GetRoom()) home = father->GetRoom();
		if (home) map.SettleCitizen(child, home);

		// 亲属：父母各自记子女，和父母、已有的兄弟姐妹互相写亲属关系。
		vector<Citizen*> siblings;
		for (Citizen* sibling : mother->GetChildren()) if (sibling) siblings.push_back(sibling);
		if (father) {
			for (Citizen* sibling : father->GetChildren()) {
				if (sibling && find(siblings.begin(), siblings.end(), sibling) == siblings.end()) siblings.push_back(sibling);
			}
		}
		const int year = today.GetYear();
		mother->AddChild(child);
		LinkKin(mother, child, RELATIVE_CHILD, RELATIVE_PARENT, year);
		if (father) {
			father->AddChild(child);
			LinkKin(father, child, RELATIVE_CHILD, RELATIVE_PARENT, year);
		}
		for (Citizen* sibling : siblings) LinkKin(sibling, child, RELATIVE_SIBLING, RELATIVE_SIBLING, year);

		DemographyReport::Birth birth;
		birth.child = child;
		birth.mother = mother;
		birth.father = father;
		report.births.push_back(birth);
	}

	// 3. 怀孕：育龄、配偶在世、距上次生育满一年的女性。
	for (size_t i = 0; i < count; i++) {
		Citizen* woman = citizens[i];
		if (!woman || !woman->IsActive() || woman->GetGender() != GENDER_FEMALE || woman->IsPregnant()) continue;
		if (!woman->HasLivingSpouse()) continue;
		const int age = woman->GetAge(now);
		if (age < config.fertileMinAge || age > config.fertileMaxAge) continue;
		if (woman->GetLastBirth().IsValid()) {
			Time ready = DateOf(woman->GetLastBirth());
			ready.AddDays(config.birthSpacingDays);
			if (today < ready) continue;
		}
		if (!Chance(config.pregnancyChancePerDay)) continue;
		Time due = today;
		due.AddDays(config.pregnancyDays);
		woman->SetPregnancyDue(due);
	}

	// 4. 配对：成年单身(没有在世配偶)的人按年龄概率找对象，配上就结婚，随机一方搬到另一方家里。
	vector<Citizen*> singles;
	for (size_t i = 0; i < count; i++) {
		Citizen* citizen = citizens[i];
		if (!citizen || !citizen->IsActive() || citizen == controlled) continue;
		if (citizen->HasLivingSpouse() || citizen->GetAge(now) < config.adultAge) continue;
		singles.push_back(citizen);
	}
	unordered_set<const Citizen*> pairedToday;
	// 搬家：把mover(和他同住的未成年子女)搬到target，记进报告。
	auto relocate = [&](Citizen* mover, Room* target, DemographyReport::MoveReason reason) {
		Room* from = mover->GetRoom();
		if (!target || from == target) return;
		vector<Citizen*> minors;
		if (from) {
			for (Citizen* child : mover->GetChildren()) {
				if (child && child->IsActive() && child->GetRoom() == from && child->GetAge(now) < config.adultAge &&
					child != controlled) minors.push_back(child);
			}
		}
		map.MoveCitizen(mover, target);
		mover->SetPendingMoveOut(false);
		report.moves.push_back({ mover, from, target, reason });
		for (Citizen* child : minors) {
			map.MoveCitizen(child, target);
			report.moves.push_back({ child, from, target, reason });
		}
	};
	for (Citizen* citizen : singles) {
		if (pairedToday.count(citizen)) continue;
		if (!Chance(DatingHazardForAge(citizen->GetAge(now)) * config.datingScale)) continue;
		Citizen* partner = nullptr;
		for (int attempt = 0; attempt < config.pairingAttempts && !singles.empty(); attempt++) {
			Citizen* candidate = singles[GetRandom(static_cast<int>(singles.size()))];
			if (candidate == citizen || pairedToday.count(candidate)) continue;
			if (candidate->GetGender() == citizen->GetGender()) continue;
			if (IsCloseRelative(citizen, candidate)) continue;
			partner = candidate;
			break;
		}
		if (!partner) continue;
		pairedToday.insert(citizen);
		pairedToday.insert(partner);
		citizen->SetSpouse(partner);
		partner->SetSpouse(citizen);
		LinkKin(citizen, partner, RELATIVE_SPOUSE, RELATIVE_SPOUSE, today.GetYear());
		report.marriages.push_back({ citizen, partner });

		Room* homeA = citizen->GetRoom();
		Room* homeB = partner->GetRoom();
		if (homeA && homeB) {
			if (homeA == homeB) continue;
			if (GetRandom(2) == 0) relocate(citizen, homeB, DemographyReport::MoveReason::Marriage);
			else relocate(partner, homeA, DemographyReport::MoveReason::Marriage);
		}
		else if (homeA) relocate(partner, homeA, DemographyReport::MoveReason::Marriage);
		else if (homeB) relocate(citizen, homeB, DemographyReport::MoveReason::Marriage);
		else if (Room* vacant = map.PickVacantResidence()) {
			relocate(citizen, vacant, DemographyReport::MoveReason::Marriage);
			relocate(partner, vacant, DemographyReport::MoveReason::Marriage);
		}
	}

	// 5. 成年搬出：满18岁那天(或之前没找到空房、标了pendingMoveOut的)家里还有别人，就搬到随机一间空房。
	for (size_t i = 0; i < count; i++) {
		Citizen* citizen = citizens[i];
		if (!citizen || !citizen->IsActive() || citizen == controlled) continue;
		const bool turnsAdult = citizen->GetAge(now) == config.adultAge && IsBirthday(citizen, now);
		if (!turnsAdult && !citizen->IsPendingMoveOut()) continue;
		// 已经结婚(和配偶住)、或者本来就一个人住：不用搬。
		if (citizen->HasLivingSpouse() || !LivesWithOthers(citizen)) {
			citizen->SetPendingMoveOut(false);
			continue;
		}
		Room* vacant = map.PickVacantResidence();
		if (!vacant) {
			citizen->SetPendingMoveOut(true);
			continue;
		}
		Room* from = citizen->GetRoom();
		map.MoveCitizen(citizen, vacant);
		citizen->SetPendingMoveOut(false);
		report.moves.push_back({ citizen, from, vacant, DemographyReport::MoveReason::MoveOut });
	}
	return report;
}
