#include "change.h"


using namespace std;

Change::Change(CHANGE_TYPE type) {

}

Change::~Change() {

}

CHANGE_TYPE Change::GetType() {
	return type;
}

void Change::SetCondition(Condition condition) {
	this->condition = condition;
}
Condition& Change::GetCondition() {
	return condition;
}


