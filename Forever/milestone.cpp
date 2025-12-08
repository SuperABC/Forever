#include "milestone.h"


using namespace std;

Milestone::Milestone(string name, vector<shared_ptr<Event>> triggers, bool visible, Condition drop,
	string description, string goal, vector<Dialog> dialogs, vector<shared_ptr<Change>> changes) :
	name(name), triggers(triggers), visible(visible), drop(drop),
	description(description), goal(goal), dialogs(dialogs), changes(changes) {

}

Milestone::~Milestone() {

}

vector<Dialog> Milestone::GetDialogs() {
	return dialogs;
}

vector<shared_ptr<Change>> Milestone::GetChanges() {
	return changes;
}

vector<shared_ptr<Event>> Milestone::GetTriggers() {
	return triggers;
}

bool Milestone::MatchTrigger(shared_ptr<Event> e) {
	if (triggers.size() <= 0)return false;
	if (!e)return false;

	for(auto trigger : triggers) {
		if (trigger->GetType() != e->GetType())continue;
		if (trigger->operator==(e)) {
			return true;
		}
	}

	return false;
}

string Milestone::GetName() {
	return name;
}

bool Milestone::IsVisible() {
	return visible;
}

Condition Milestone::DropCondition() {
	return drop;
}

bool Milestone::DropSelf(function<ValueType(const string&)> getValue) {
	return drop.EvaluateBool(getValue);
}

string Milestone::GetDescription() {
	return description;
}

string Milestone::GetGoal() {
	return goal;
}

MilestoneNode::MilestoneNode(Milestone milestone) : content(milestone) {

}

