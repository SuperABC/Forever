#include "organization_base.h"


using namespace std;

void OrganizationFactory::RegisterOrganization(const string& id, function<unique_ptr<Organization>()> creator) {
    registries[id] = creator;
    configs[id] = false;
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

void OrganizationFactory::SetConfig(std::string name, bool config) {
    if (configs.find(name) != configs.end()) {
        configs[name] = config;
    }
}


