#pragma once

#include "class.h"
#include "common/utility.h"

#include <vector>


// 运行时人口推进的参数。默认值就是游戏里用的数(先写死在这里，不进Config)；测试把概率拉到1来确定性地覆盖每个分支。
struct DemographyConfig {
	int deathStartAge = 60;
	double deathRatePerYear = 0.00001;  // 每天自然死亡概率 = (年龄-deathStartAge) × 它
	int fertileMinAge = 20;
	int fertileMaxAge = 45;
	double pregnancyChancePerDay = 0.0004;
	int pregnancyDays = 280;
	int birthSpacingDays = 365;          // 距上次生育至少这么多天才会再怀孕
	int adultAge = 18;
	double datingScale = 1.0 / 365.0;    // 每天配对概率 = DatingHazardForAge(年龄) × 它(年概率摊到每天)
	int pairingAttempts = 10;
};

// 一天里发生的人口变化，WorldStep据此调UE层的回调(onDied/onBorn/onRelocated)并写日志。
struct DemographyReport {
	struct Death {
		Citizen* citizen = nullptr;
		int age = 0;
		std::vector<Citizen*> heirs; // 遗产给了谁(空=归政府)
	};
	struct Birth {
		Citizen* child = nullptr;
		Citizen* mother = nullptr;
		Citizen* father = nullptr; // 可能已经不在世
	};
	enum class MoveReason { MoveOut, Marriage };
	struct Move {
		Citizen* citizen = nullptr;
		Room* from = nullptr;
		Room* to = nullptr;
		MoveReason reason = MoveReason::MoveOut;
	};
	std::vector<Death> deaths;
	std::vector<Birth> births;
	std::vector<std::pair<Citizen*, Citizen*>> marriages;
	std::vector<Move> moves;
};

// 每日跨天时的人口推进：自然死亡、出生、怀孕、配对结婚同居、成年搬出。纯Core计算，不发脚本事件，见demography.md。
class Demography {
public:
	// controlled=玩家正在操控的市民(没有传nullptr)：他不会自然死亡、不配对、不搬家。
	static DemographyReport Daily(Populace& populace, Map& map, Society& society, const Time& now,
		const Citizen* controlled, const DemographyConfig& config = DemographyConfig());

	// 自然死亡的善后：释放工作、遗产(清算成现金后给在世配偶，没有就平分给在世子女，都没有归政府；
	// 欠款由城市外核销)、名下房产和载具给第一个继承人(没有继承人房产清空、载具不动)、Die()、移出住户。
	// 返回继承人。
	static std::vector<Citizen*> Kill(Map& map, Society& society, Citizen* citizen);
};
