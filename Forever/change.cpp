#include "change.h"


using namespace std;

std::vector<std::shared_ptr<Change>> SetValueChange::ApplyChange() {
	return std::vector<std::shared_ptr<Change>>();
}

std::vector<std::shared_ptr<Change>> RemoveValueChange::ApplyChange() {
	return std::vector<std::shared_ptr<Change>>();
}
