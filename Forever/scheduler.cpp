#include "scheduler.h"


using namespace std;

WorkOnlyScheduler::WorkOnlyScheduler() {

}

WorkOnlyScheduler::~WorkOnlyScheduler() {

}

string WorkOnlyScheduler::GetId() {
	return "work_only";
}

string WorkOnlyScheduler::GetType() const {
	return "work_only";
}

string WorkOnlyScheduler::GetName() const {
	return "纯工作调度";
}
