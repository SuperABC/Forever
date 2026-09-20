#pragma once

#include "common/utility.h"
#include "common/handle.h"

#include <string>
#include <unordered_map>
#include <vector>

class Change;

// JobMod：一份具体工作类型的行为定义(比如店员)。这次迁移不要Calendar——"哪几天上班/几点
// 上下班"直接写进具体JobMod子类的DailyPlan里，不再有独立的Calendar概念。
//
// DailyPlan/ExecNode这次调度机制完全是C++直接构造Change*，和milestone/JSON没有任何关系
// （不做"按节点名找milestone触发"这种合并——milestone只能通过它自己原本的Event广播+
// Script::MatchEvent触发器匹配流程被触发，两者运行时完全独立，见job.md）。plans/changes
// 是这个mod实例自己的成员字段：DailyPlan/ExecNode是具体子类重写的虚方法，直接写自己的
// 成员字段（分配/写入/析构全程都在这个mod实例所在的同一侧，Core侧只读，不delete这里面
// 的Change*），和BuildingMod::floors/singles/rows已经在用的跨DLL安全模式一致，见
// Source/Core/society/job.md"跨DLL数据传递约束"一节。
class JobMod {
public:
	JobMod() = default;
	virtual ~JobMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// Script配置——老工程用pair<ScriptModName, vector<MilestoneName>>方式在Mod侧声明
	// 意图，这次照抄：两个普通成员字段，和plans/changes同一个"mod自己的字段，Core只读"
	// 安全模式，不通过返回值传递(之前这里是一个返回vector<string>的方法，按值把mod
	// 分配的容器交给Core遍历，是被明确禁止的跨DLL模式，这次改掉)。scriptModName默认
	// "empty"(纯粹要个能创建出来的ScriptMod壳，具体内容和milestoneNames无关)，
	// milestoneNames默认为空(不需要milestone的job类型不用改)。具体子类应该在自己的
	// 构造函数里直接赋值。milestoneNames每一项是不含路径/扩展名的bare文件名，Core通过
	// Config::GetScriptPath()解析实际路径，mod不知道也不需要知道文件实际存放位置——这两
	// 个字段和下面的DailyPlan/ExecNode完全无关，纯粹是给这个Job独占的Script提供milestone
	// 内容，将来由外部广播一个Event、走Script::MatchEvent的正常触发器匹配流程触发。
	std::string scriptModName = "empty";
	std::vector<std::string> milestoneNames;

	// 生成今天的调度表，直接写进this->plans(node名->今天触发的具体时间)。
	// @post：向Core发起查询的句柄(见Dependence/common/handle.h)，这次新增的用途是查
	// occupant家/工位的具体房间地址(Room::GetAddress()格式，"post"分别是"citizen home
	// address"/"citizen workplace address"，Core端实现见Core/common/implement.cpp)——
	// NPCNavigateChange::destination不能再写"home"/"workplace"这种描述性文本，必须是
	// 能被Map::LocateRoom(address)直接解析回Room*的具体地址字符串，DailyPlan如果需要
	// 提前知道地址(比如往plans里塞和地址相关的信息)可以在这里就查；不需要这个能力可以
	// 忽略这个参数。
	virtual void DailyPlan(const Time& currentTime, PostHandle* post) = 0;

	// 执行一个到期节点：直接new Change*写进this->changes(所有权留在这个mod实例身上，
	// 不转移给调用方)。调用前调用方已经保证清空了上一次的残留（见Job::ExecNode）。不需要
	// 对某个节点做任何事的job类型不用重写。
	// @post：同DailyPlan——构造NPCNavigateChange时按需调用post->Post(...)查occupant
	// 家/工位的具体地址，见上。
	virtual void ExecNode(const std::string& node, PostHandle* post) {}

	// 当前持有这份工作的市民姓名——由Job在调用ExecNode前同步进来，供ExecNode里构造
	// NPCNavigateChange这类需要知道"是谁"的Change使用，不代表这个mod持有Citizen（
	// Dependence层看不到Citizen这个Core类型，只能传字符串）。
	std::string occupantName;

	std::unordered_map<std::string, Time> plans;
	std::vector<Change*> changes;
};
