#include "story/milestone.h"

#include "story/event.h"
#include "story/dialog.h"
#include "story/change.h"


using namespace std;

Milestone::Milestone(string name, vector<Event*> triggers, bool visible, Expression drop, string description,
	string goal, vector<Change*> changes, vector<Dialog*> dialogs, vector<string> subsequences) :
	name(move(name)), triggers(move(triggers)), visible(visible), drop(move(drop)), description(move(description)),
	goal(move(goal)), changes(move(changes)), dialogs(move(dialogs)), subsequences(move(subsequences)) {

}

Milestone::~Milestone() {
	for (auto& trigger : triggers) {
		delete trigger;
	}
	triggers.clear();
	for (auto& change : changes) {
		delete change;
	}
	changes.clear();
	for (auto& dialog : dialogs) {
		delete dialog;
	}
	dialogs.clear();
}

vector<Change*> Milestone::GetChanges() const {
	return changes;
}

vector<Dialog*> Milestone::GetDialogs() const {
	return dialogs;
}

vector<Event*> Milestone::GetTriggers() const {
	return triggers;
}

bool Milestone::MatchTrigger(Event* e, const ScriptContext& context) const {
	if (triggers.size() <= 0) return false;
	if (!e) return false;

	for (auto trigger : triggers) {
		if (!trigger) continue;
		if (!trigger->GetCondition().EvaluateBool(context)) continue;
		if (trigger->GetType() != e->GetType()) continue;
		if (trigger->Match(e, context)) {
			return true;
		}
	}

	return false;
}

string Milestone::GetName() const {
	return name;
}

bool Milestone::IsVisible() const {
	return visible;
}

Expression Milestone::DropCondition() const {
	return drop;
}

bool Milestone::DropSelf(const ScriptContext& context) const {
	return drop.EvaluateBool(context);
}

string Milestone::GetDescription() const {
	return description;
}

string Milestone::GetGoal() const {
	return goal;
}

vector<string> Milestone::GetSubsequences() const {
	return subsequences;
}

MilestoneNode::MilestoneNode() :
	content(nullptr),
	premise(0) {

}

MilestoneNode::MilestoneNode(Milestone* milestone) :
	content(milestone),
	premise(0) {

}
