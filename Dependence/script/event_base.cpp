#include "event_base.h"


using namespace std;

void Event::SetCondition(Condition condition) {
    this->condition = condition;
}

Condition& Event::GetCondition() {
    return condition;
}

void EventFactory::RegisterEvent(const string& id, function<unique_ptr<Event>()> creator) {
    registries[id] = creator;
    configs[id] = false;
}

unique_ptr<Event> EventFactory::CreateEvent(const string& id) {
    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool EventFactory::CheckRegistered(const string& id) {
    return registries.find(id) != registries.end();
}

void EventFactory::SetConfig(string name, bool config) {
    if (configs.find(name) != configs.end()) {
        configs[name] = config;
    }
}


