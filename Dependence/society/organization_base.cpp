#include "organization_base.h"


using namespace std;

vector<pair<shared_ptr<Component>, vector<shared_ptr<Job>>>> Organization::GetMappings() const {
    return mappings;
}

void Organization::AddMapping(shared_ptr<Component> component, vector<shared_ptr<Job>> jobs) {
    mappings.push_back(make_pair(component, jobs));
}

void OrganizationFactory::RegisterOrganization(const string& id, function<unique_ptr<Organization>()> creator, float power) {
    registries[id] = creator;
    configs[id] = false;
    powers[id] = power;
}

unique_ptr<Organization> OrganizationFactory::CreateOrganization(const string& id) {
    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool OrganizationFactory::CheckRegistered(const string& id) {
    return registries.find(id) != registries.end();
}

void OrganizationFactory::SetConfig(string name, bool config) {
    if (configs.find(name) != configs.end()) {
        configs[name] = config;
    }
}

const unordered_map<string, float>& OrganizationFactory::GetPowers() const {
    return powers;
}


