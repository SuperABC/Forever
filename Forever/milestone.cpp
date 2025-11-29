#include "milestone.h"


using namespace std;

Milestone::Milestone(string name, shared_ptr<Event> trigger, bool visible, string description, string goal,
	vector<Dialog> dialogs, vector<shared_ptr<Change>> changes) :
	name(name), trigger(trigger), visible(visible), description(description), goal(goal), dialogs(dialogs), changes(changes) {

}

Milestone::~Milestone() {

}

vector<Dialog> Milestone::GetDialogs() {
	return dialogs;
}

vector<shared_ptr<Change>> Milestone::GetChanges() {
	return changes;
}

shared_ptr<Event> Milestone::GetTrigger() {
	return trigger;
}

bool Milestone::MatchTrigger(shared_ptr<Event> e) {
	if (!trigger)return false;
	if (!e)return false;
	if (trigger->GetName() != e->GetName())return false;

	return trigger->operator==(e);
}

string Milestone::GetName() {
	return name;
}

bool Milestone::IsVisible() {
	return visible;
}

string Milestone::GetDescription() {
	return description;
}

string Milestone::GetGoal() {
	return goal;
}

MilestoneNode::MilestoneNode(Milestone milestone) : content(milestone) {

}

