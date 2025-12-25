#include "job_base.h"


using namespace std;

void Job::SetCalendar(shared_ptr<Calendar> calendar) {
    this->calendar = calendar;
}

shared_ptr<Calendar> Job::GetCalendar() const {
    return calendar;
}

void Job::SetPosition(shared_ptr<Room> room) {
    position = room;
}

shared_ptr<Room> Job::GetPosition() const {
    return position;
}

void JobFactory::RegisterJob(const string& id, function<unique_ptr<Job>()> creator) {
    registries[id] = creator;
}

unique_ptr<Job> JobFactory::CreateJob(const string& id) {
    if(configs.find(id) == configs.end() || !configs.find(id)->second)return nullptr;
    
    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool JobFactory::CheckRegistered(const string& id) {
    return registries.find(id) != registries.end();
}

void JobFactory::SetConfig(string name, bool config) {
    configs[name] = config;
}

