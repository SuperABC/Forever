#include "job_base.h"


using namespace std;

void JobFactory::RegisterJob(const string& id, function<unique_ptr<Job>()> creator) {
    registry[id] = creator;
}

unique_ptr<Job> JobFactory::CreateJob(const string& id) {
    auto it = registry.find(id);
    if (it != registry.end()) {
        return it->second();
    }
    return nullptr;
}

bool JobFactory::CheckRegistered(const string& id) {
    return registry.find(id) != registry.end();
}

