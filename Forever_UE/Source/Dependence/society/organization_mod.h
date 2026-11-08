#pragma once

#include "common/mod_state.h"
#include "common/utility.h"
#include "common/handle.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class Change;

// 组织对外提供的一项服务：服务名只给组织自己后面实际提供服务时识别，类型/单价/质量给系统撮合。
struct ServiceOffer {
	std::string name;
	std::string type;
	int64_t unitPrice = 0; // 分
	float quality = 0.f;
	int capacity = 0;      // 当日最多能提供的份数（商店是该货物的库存），系统按它加权分配需求、防止超卖
};

// 岗位计划里的一条：房间名(roomName，同一个组织里唯一)+岗位类型。组织的岗位计划就是一串这样的条目，
// 同一个(房间, 类型)出现几次就是计划几个岗位。
struct Vacancy {
	std::string room;
	std::string type;
};

// 一个工位房间的容量(roomName+workspaceCapacity)，mod据此决定每个房间的计划人数。
struct RoomState {
	const char* room = "";
	int capacity = 0;
};

// 月度结算时Core传给mod的现状，mod只读。组织级的数字已经扣掉了工资/房租，未扣企业税和分红。
struct OrganizationMonthlyState {
	int64_t balance = 0;       // 月结时账户余额（分）
	int64_t profit = 0;        // 当月利润 = 当前余额 − 上次月结完成后的余额，不含增发募资
	int64_t initialFunds = 0;
	int64_t totalShares = 0, treasuryShares = 0, sharePrice = 0;
	int employees = 0;         // 全组织在岗人数
	int positions = 0;         // 全组织开放岗位数（含空缺）
	const RoomState* rooms = nullptr; // 该组织全部工位房间的容量（Core持有的数组，调用期间有效）
	int roomCount = 0;
};

// 设计见organization_mod.md。Dependence层看不到Job/Component/Room/Citizen,接口只用字符串/int/Time/Change*。
class OrganizationMod {
public:
	OrganizationMod() = default;
	virtual ~OrganizationMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 子类还必须实现static GetPower()和static GetRequirements(),通过RegisterOrganization注册,见organization_mod.md。

	// 每个workspace房间调一次,把这个房间要配的岗位以{roomName, job类型}追加进this->vacancies(岗位计划)。
	// reset为true时先vacancies.clear()再追加(倒闭重启时Core对第一个房间传true,清掉旧计划);为false只追加。
	// clear必须由mod自己做:string在mod堆上分配,Core侧释放会跨DLL堆崩溃。
	virtual void DesignJobsForRoom(const std::string& componentType, const std::string& componentName,
		const std::string& roomType, const std::string& roomName, int workspaceCapacity, bool reset) = 0;

	// 每月结算一次(倒闭中的组织不调)。mod就地修改this->vacancies来扩招(多push几条)或裁员(erase几条),
	// 也可以改dividendPayoutRatio、设issueShares(本月增发的股数);不改就是岗位/分红率保持不变、不增发。
	// Core每月在调用前把issueShares置0,调用后据vacancies对账岗位、按dividendPayoutRatio分红、按issueShares增发。
	virtual void MonthlySettle(const OrganizationMonthlyState& state) {}

	// 存档:把岗位计划写成/读成一个字符串数组[room,type,room,type,...]。必须是虚函数,实现在头文件里:
	// 虚调用落到mod DLL里编译的那份,vacancies的string始终在mod堆上分配释放,Core不碰它的内存。
	virtual void SaveVacancies(StateWriter& writer) const {
		writer.BeginArray("vacancies");
		for (const Vacancy& v : vacancies) {
			writer.SetString("", v.room.c_str());
			writer.SetString("", v.type.c_str());
		}
		writer.EndArray();
	}
	virtual void LoadVacancies(StateReader& reader) {
		vacancies.clear();
		if (!reader.EnterArray("vacancies")) return;
		int n = reader.GetArraySize();
		for (int i = 0; i + 1 < n; i += 2) {
			Vacancy v;
			v.room = reader.GetStringAt(i, "");
			v.type = reader.GetStringAt(i + 1, "");
			vacancies.push_back(v);
		}
		reader.ExitArray();
	}

	// 每日服务收集:系统先把状态写进script(商店是self.stock.<货物类型>),stockTypes列出写入的类型,
	// mod在services里填本日能提供的服务(先clear再push)。post可查商品零售价等(post "product info")。
	virtual void CollectServices(Container* script, const std::vector<std::string>& stockTypes,
		PostHandle* post) {}

	// 市民用掉了count份name服务:mod改script变量,系统随后回写Storage。
	virtual void ApplyServiceUsage(const std::string& name, int count, Container* script) {}

	virtual void DailyPlan(const Time& currentTime, PostHandle* post) {}
	virtual void ExecNode(const std::string& node, PostHandle* post) {}

	std::vector<Vacancy> vacancies;     // 岗位计划,mod持有并就地修改,Core只读
	int64_t issueShares = 0;            // MonthlySettle的输出,见上
	std::vector<ServiceOffer> services; // CollectServices的输出

	// 经济参数(分),语义见organization_mod.md。
	int64_t initialFunds = 0;
	std::vector<float> shareholders;
	int64_t wage = 0;
	float dividendPayoutRatio = 1.f; // 分红率[0,1]：月度可分配利润(税后)里发给股东的比例，其余留在企业账户
	std::unordered_map<std::string, Time> plans;
	std::vector<Change*> changes;

	// Script配置,同JobMod。
	std::string scriptModName; // 空=无自定义ScriptMod；和milestoneNames都空则实例没有Script(GetScript()==nullptr)，见MakeScript
	std::vector<std::string> milestoneNames;

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
