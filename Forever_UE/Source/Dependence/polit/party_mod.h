#pragma once

#include "common/mod_test.h"
#include "common/mod_state.h"
#include "common/utility.h"
#include "common/handle.h"

#include <string>
#include <vector>


// Core -> mod 的只读输入/mod -> Core 的输出结构。全部只含字符串和数值，Dependence看不到Citizen/Room。
// 约定：Core传给mod的容器mod只读；mod的输出写进mod自己的成员，Core读走(复制)，Core绝不修改mod的容器
// (跨DLL分配器，见memory cross_dll_allocator_crash)，所以输出容器由mod在自己的方法开头自己clear。

// 一项政策的当前值(Core给mod看的快照)。
struct PolicyValue {
	std::string id;
	double value = 0.0;
};

// 党派对一项政策的目标值(绝对值，不是比例)。Core把它换算成相对当前值的比例并夹到[0.9, 1.1]。
struct PolicyTarget {
	std::string policy;
	double target = 0.0;
};

// 权力词汇里的一项(Core动态提供)。policy非空=这项权力关联某个可调政策。
struct PowerInfo {
	std::string id;
	std::string name;
	std::string policy;
};

// 一个党员担任一项权力。
struct PowerAssignment {
	std::string member; // 党员姓名
	std::string power;  // PowerInfo::id
};

// 党代会日程：day是每月几号(0=本月不开)，start/end只用时分秒(同一天内，end>=start)。
struct PartyCongress {
	int day = 0;
	Time start;
	Time end;
};

// 赢得选举时Core交给mod做权力指派的上下文(全是只读快照)。
struct PartyContext {
	std::string leader;                // 党首姓名，可以为空(党首目前不参与指派)
	std::vector<std::string> members;  // 全体党员姓名
	std::vector<PowerInfo> powers;     // 全部已注册的权力
};

// 设计见party_mod.md。
class PartyMod {
public:
	PartyMod() = default;
	virtual ~PartyMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 单元测试：print输出关键信息，返回是否通过。没有深度测试的mod用FOREVER_MOD_GENERIC_UNIT_TEST(见common/mod_test.h)。
	virtual bool UnitTest(TestPrintFn print) = 0;

	// 子类在这里填memberCount等属性(不在构造函数里)，PartyFactory::CreateParty创建实例后立刻调一次。
	virtual void SetProperty() {}

	// 党员人数：新开局时Core从成年市民里随机抽这么多人(池不够就抽到没人为止)。
	int memberCount = 0;

	// 每次选举前Core调用：current是当前全部政策值(只读快照)，mod先clear自己的targets再填目标值。
	// 没写进targets的政策=这个党对它没有主张(比例按1)。
	virtual void Propose(const std::vector<PolicyValue>& current) { (void)current; targets.clear(); }
	std::vector<PolicyTarget> targets;

	// 这个月的党代会日程(day==0=不开)。Core每个月初调一次。
	virtual PartyCongress GetCongress(int year, int month) { (void)year; (void)month; return PartyCongress(); }

	// 创建城市时Core调用：mod通过post查询(如{"post":"random room","type":"room_meeting"})挑一个会议室，
	// 把房间地址写进meetingAddress(空=不指定)。Core会检查地址有效且没被别的党占用，重叠就重调，最多4次。
	// 读档不会再调用，直接用存档里的地址。
	virtual void ChooseMeetingRoom(PostHandle* post) { (void)post; meetingAddress.clear(); }
	std::string meetingAddress;

	// 赢得选举(成为执政党)时Core调用：mod先clear自己的assignments，再把权力分配给党员(一个党员可以担任0/1/多项)。
	// 只能引用ctx里出现过的党员姓名和权力id，其余会被Core忽略。
	virtual void AssignPowers(const PartyContext& ctx) { (void)ctx; assignments.clear(); }
	std::vector<PowerAssignment> assignments;

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
