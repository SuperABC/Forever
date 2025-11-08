#include "job_base.h"


using namespace std;

void JobFactory::RegisterJob(const string& id, function<unique_ptr<Job>()> creator) {
    registries[id] = creator;
    configs[id] = false;
}

unique_ptr<Job> JobFactory::CreateJob(const string& id) {
    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool JobFactory::CheckRegistered(const string& id) {
    return registries.find(id) != registries.end();
}

void JobFactory::SetConfig(std::string name, bool config) {
    if (configs.find(name) != configs.end()) {
        configs[name] = config;
    }
}

