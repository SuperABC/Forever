#include "job.h"


using namespace std;

std::vector<std::string> TestJob::GetScripts() const {
	return { "../Resources/scripts/jobs/test.json" };
}
