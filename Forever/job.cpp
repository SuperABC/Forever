#include "job.h"


using namespace std;

string DefaultJob::GetId() {
	return "default";
}

string DefaultJob::GetType() const {
	return "default";
}

string DefaultJob::GetName() const {
	return "默认工作";
}

vector<string> DefaultJob::GetScripts() const {
	return { "../Resources/scripts/jobs/default.json" };
}
