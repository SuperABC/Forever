#include "change.h"


using namespace std;

vector<shared_ptr<Change>> SetValueChange::ApplyChange() {
	return vector<shared_ptr<Change>>();
}

vector<shared_ptr<Change>> RemoveValueChange::ApplyChange() {
	return vector<shared_ptr<Change>>();
}
