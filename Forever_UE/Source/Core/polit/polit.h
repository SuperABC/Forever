#pragma once

#include "class.h"

#include "common/handle.h"
#include "common/utility.h"
#include "polit/party.h"
#include "polit/policy.h"
#include "polit/power.h"

#include <string>
#include <vector>


// 党代会开始/结束的通知：Core不发脚本事件，调用方(UE层)用ConsumeMeetingNotices取走后转发给党员的脚本。
struct MeetingNotice {
	std::string party;               // 党派注册id
	bool start = true;               // true=开始，false=结束
	std::string address;             // 会议室地址(空=没有会议室)
	std::vector<Citizen*> members;   // 发出通知那一刻的全体在城党员
};

// 最近一次选举的结果(测试/日志用)。
struct ElectionSummary {
	int monthKey = 0;                // 选举发生时的月份(year*12+month)
	int voters = 0;
	std::vector<std::string> partyIds;
	std::vector<int> votes;          // 和partyIds同序
	int winner = -1;                 // partyIds下标，-1=没有选出(没有选民/没有党派)
	bool tied = false;               // 平票随机裁决
};

// 政治域：党派、政策值、选举、党代会、权力。和Map/Populace/Society平级，设计见polit.md。
// 没有启用任何党派时只持有一份默认的GovernmentPolicy，什么都不做，经济结果和没有政治域时完全一致。
class Polit {
	// 存档：PolitSaveCodec直接读写私有字段(polit_save.md)。
	friend class PolitSaveCodec;
public:
	Polit(); // 党派工厂取自Registry
	~Polit();

	Polit(const Polit&) = delete;
	Polit& operator=(const Polit&) = delete;

	// 接上其它域并把自己持有的政策绑给Society(Society::BindPolicy)。Init和读档后都要调用；
	// Polit必须比Society活得久(析构时不碰Society)。
	void Bind(Map& map, Populace& populace, Society& society);

	// 只在新开局调用：每个启用的党派建一个实例，抽党员，选会议室。post给mod挑房间用。
	void Init(PostHandle& post, const Time& now);

	// 每个WorldStep调一次(不受crossedDay限制)：月份变化时选举，推进党代会，清理离城/死亡党员。
	void Step(const Time& now);

	// 取走并清空待发的党代会通知(同Traffic::ConsumeStopEvents)。
	std::vector<MeetingNotice> ConsumeMeetingNotices();

	// 月末选举：全体在城成年人投票，胜者成为执政党并调整政策基础值。Step在月份变化时调用，测试也可直接调。
	void HoldElection(const Time& now);

	GovernmentPolicy& GetPolicy() { return policy; }
	const GovernmentPolicy& GetPolicy() const { return policy; }
	const PowerCatalog& GetPowers() const { return powers; }
	PowerCatalog& GetPowers() { return powers; }

	const std::vector<Party*>& GetParties() const { return parties; }
	Party* FindParty(const std::string& id) const;
	Party* GetRulingParty() const;
	const ElectionSummary& GetLastElection() const { return lastElection; }
	int GetLastMonthKey() const { return lastMonthKey; }

	// 查询：这个市民当前担任的权力id / 某项权力的担任者(只有执政党的权力)。
	std::vector<std::string> PowersOf(const Citizen* citizen) const;
	std::vector<Citizen*> HoldersOf(const std::string& powerId) const;

	static int MonthKeyOf(const Time& now) { return now.GetYear() * 12 + now.GetMonth(); }

private:
	// polit.cpp
	void RemoveGoneMembers(Party& party, const Time& now);
	void AssignPowers(Party& party, const Time& now);

	// election.cpp：对每项政策把党派提议换算成相对当前值的比例(夹到[0.9,1.1])。
	std::vector<double> ComputeRatios(Party& party, const Time& now);
	void ApplyRatios(const std::vector<double>& ratios, const Time& now);

	// congress.cpp
	void UpdateCongress(Party& party, const Time& now);
	void ElectLeader(Party& party, const Time& now);
	void PushNotice(const Party& party, bool start);

	PartyFactory& factory;
	Map* map = nullptr;
	Populace* populace = nullptr;
	Society* society = nullptr;

	GovernmentPolicy policy;
	PowerCatalog powers;
	std::vector<Party*> parties;
	int lastMonthKey = 0;            // 0=还没记录过(第一次Step只记录不选举)
	std::vector<MeetingNotice> notices;
	ElectionSummary lastElection;
};
