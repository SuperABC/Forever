#include "cabin.h"

#include "map/building.h"
#include "story/script.h"
#include "story/script_factory.h"
#include "common/config.h"

#include <algorithm>

using namespace std;

Cabin::Cabin(Building* building, int specIndex, const string& interactName, int shaftIndex,
	int minLevel, int maxLevel, int initialLevel, float initialZ,
	const string& scriptModName, const vector<string>& milestoneNames, ScriptFactory* scriptFactory) :
	building(building), specIndex(specIndex), interactName(interactName), shaftIndex(shaftIndex),
	minLevel(minLevel), maxLevel(maxLevel), currentLevel(initialLevel), currentZ(initialZ), targetLevel(initialLevel) {

	// 和Door同一个模式：指定了里程碑文件才创建Script，脚本变量供$$self.xxx引用。
	if (!milestoneNames.empty() && scriptFactory) {
		script = new Script(scriptFactory, scriptModName.empty() ? string("empty") : scriptModName);
		for (const string& milestoneName : milestoneNames) {
			script->ReadMilestones(Config::GetScriptPath(milestoneName));
		}
		script->SetValue("name", ValueType(interactName));
		script->SetValue("building", ValueType(building ? building->GetName() : string()));
		script->SetValue("min", ValueType(minLevel));
		script->SetValue("max", ValueType(maxLevel));
	}
}

Cabin::~Cabin() {
	delete script;
}

void Cabin::Launch(int level) {
	level = clamp(level, minLevel, maxLevel);
	if (!moving && level == currentLevel) return;
	fromZ = currentZ;
	targetLevel = level;
	elapsed = 0.f;
	moving = true;
}

void Cabin::Arrive(float targetZ) {
	currentLevel = targetLevel;
	currentZ = targetZ;
	velocity = 0.f;
	moving = false;
	elapsed = 0.f;
}

void Cabin::AddOption(const string& option) {
	options.push_back(option);
}

void Cabin::RemoveOption(const string& option) {
	options.erase(remove(options.begin(), options.end(), option), options.end());
}
