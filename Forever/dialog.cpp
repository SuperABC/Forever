#include "dialog.h"


using namespace std;

Dialog::Dialog() {

}

Dialog::~Dialog() {

}

void Dialog::AddDialog(string speaker, string content) {
	list.push_back(make_pair(speaker, content));
}

vector<pair<string, string>> Dialog::GetDialogs() {
	return list;
}

void Dialog::SetCondition(Condition condition) {
	this->condition = condition;
}
Condition& Dialog::GetCondition() {
	return condition;
}
