#include "organization_base.h"


using namespace std;

void OrganizationFactory::RegisterOrganization(const string& id, function<unique_ptr<Organization>()> creator) {
    registry[id] = creator;
}

unique_ptr<Organization> OrganizationFactory::CreateOrganization(const string& id) {
    auto it = registry.find(id);
    if (it != registry.end()) {
        return it->second();
    }
    return nullptr;
}

bool OrganizationFactory::CheckRegistered(const string& id) {
    return registry.find(id) != registry.end();
}


