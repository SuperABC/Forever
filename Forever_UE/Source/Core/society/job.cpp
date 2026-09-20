#include "society/job.h"

#include "story/script.h"
#include "story/script_factory.h"
#include "common/config.h"
#include "populace/citizen.h"

#include <filesystem>

using namespace std;

namespace {
	// 和Story::Init同一个"empty"壳mod的用法——Job的Script这次只需要一个能创建出来的
	// ScriptMod外壳，具体内容(如果有milestone)靠GetMilestoneFiles()指定的文件加载，
	// 和挂载的具体ScriptMod类型无关，见story.cpp的kEmptyScriptId同款注释。
	constexpr const char* kEmptyScriptId = "empty";

	// Resource/Story/<name>.json：和Story::GetTestScriptPath()同一个相对路径约定，相对
	// config.json所在目录(Resource/Config/)。
	string GetMilestoneFilePath(const string& name) {
		filesystem::path configDir = Config::GetConfigDir();
		return (configDir / ".." / "Story" / (name + ".json")).string();
	}
}

Job::Job(JobFactory* factory, ScriptFactory* scriptFactory, const string& id, Room* position) :
	factory(factory), position(position) {
	mod = factory->CreateJob(id);
	script = new Script(scriptFactory, kEmptyScriptId);
	if (mod) {
		for (const string& file : mod->GetMilestoneFiles()) {
			script->ReadMilestones(GetMilestoneFilePath(file));
		}
	}
}

Job::~Job() {
	delete script;
	if (mod) factory->DestroyJob(mod);
}

string Job::GetType() const {
	return mod ? mod->GetType() : string();
}

Room* Job::GetPosition() const { return position; }
Script* Job::GetScript() const { return script; }

Citizen* Job::GetOccupant() const { return occupant; }
void Job::SetOccupant(Citizen* citizen) { occupant = citizen; }

void Job::DailyPlan(const Time& currentTime) {
	if (mod) mod->DailyPlan(currentTime);
}

const unordered_map<string, Time>& Job::GetPlans() const {
	static const unordered_map<string, Time> empty;
	return mod ? mod->plans : empty;
}

vector<Change*> Job::ExecNode(const string& node) {
	if (!mod) return {};
	mod->occupantName = occupant ? occupant->GetName() : string();
	mod->ExecNode(node);
	return mod->changes; // 只复制指针值，所有权留在mod自己身上，见job.h的说明
}
