#pragma once

#include "class.h"

#include "society/job_mod.h"
#include "society/job_factory.h"

#include "common/utility.h"

#include <string>
#include <unordered_map>
#include <vector>


// Job：一份具体工作岗位，独占持有一个JobMod实例+一份自己的Script，见job.md。
// Job由Organization持有所有权，Citizen只存不持有所有权的Job*。
class Job {
	// 存档：SocietySaveCodec直接读写私有字段(society_save.md)。
	friend class SocietySaveCodec;
public:
	Job(JobFactory* factory, ScriptFactory* scriptFactory, const std::string& id, Room* position);
	~Job(); // factory->DestroyJob(mod)；delete script

	std::string GetType() const;
	Room* GetPosition() const; // 工位room，不持有
	Script* GetScript() const;

	Citizen* GetOccupant() const; // 当前持有这份工作的市民，不持有
	void SetOccupant(Citizen* citizen);

	// 岗位是否开放：扩招/重开时开放，裁员/倒闭时关闭。Job对象只增不删（存档下标和脚本键依赖jobs的顺序），
	// 关闭的岗位不算空缺、不招人、不发工资。关闭时占用者必须已经被清掉。
	bool IsOpen() const;
	void SetClosed(bool value);

	// 生成今天的调度表：转调mod->DailyPlan，结果就是mod->plans。@post透传给mod。
	void DailyPlan(const Time& currentTime, PostHandle* post);
	const std::unordered_map<std::string, Time>& GetPlans() const;

	// 执行一个到期节点：先同步occupantName给mod再转调mod->ExecNode；返回的Change*所有权
	// 留在mod，调用方不delete，见job.md"跨DLL数据传递约束"。
	std::vector<Change*> ExecNode(const std::string& node, PostHandle* post);

private:
	JobMod* mod;
	JobFactory* factory;
	Script* script;
	Room* position;
	Citizen* occupant = nullptr;
	bool closed = false;
};
