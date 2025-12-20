#include "calendar_base.h"


using namespace std;

void CalendarFactory::RegisterCalendar(const string& id, function<unique_ptr<Calendar>()> creator) {
    registries[id] = creator;
}

unique_ptr<Calendar> CalendarFactory::CreateCalendar(const string& id) {
    if(configs.find(id) == configs.end() || !configs.find(id)->second)return nullptr;
    
    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool CalendarFactory::CheckRegistered(const string& id) {
    return registries.find(id) != registries.end();
}

void CalendarFactory::SetConfig(string name, bool config) {
    configs[name] = config;
}


