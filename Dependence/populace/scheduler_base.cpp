#include "scheduler_base.h"


using namespace std;

void SchedulerFactory::RegisterScheduler(const string& id, function<unique_ptr<Scheduler>()> creator, float power) {
    registries[id] = creator;
    powers[id] = power;
}

unique_ptr<Scheduler> SchedulerFactory::CreateScheduler(const string& id) {
    if(configs.find(id) == configs.end() || !configs.find(id)->second)return nullptr;
    
    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool SchedulerFactory::CheckRegistered(const string& id) {
    return registries.find(id) != registries.end();
}

void SchedulerFactory::SetConfig(string name, bool config) {
    configs[name] = config;
}

const unordered_map<string, float>& SchedulerFactory::GetPowers() const {
    return powers;
}
