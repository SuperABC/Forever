#include "scheduler.h"


using namespace std;

WorkOnlyScheduler::WorkOnlyScheduler() {

}

WorkOnlyScheduler::~WorkOnlyScheduler() {

}

std::string WorkOnlyScheduler::GetId() {
	return "work_only";
}

std::string WorkOnlyScheduler::GetType() const {
	return "work_only";
}

std::string WorkOnlyScheduler::GetName() const {
	return "纯工作调度";
}
