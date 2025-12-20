#include "organization_base.h"


using namespace std;

vector<shared_ptr<Job>> Organization::EnrollEmployee(vector<int> ids) {
    vector<shared_ptr<Job>> positions;

    int i = 0;
    for (auto& component : jobs) {
        for (auto& vacancy : component.second) {
            if (vacancy.second < 0) {
                positions.push_back(vacancy.first);
                vacancy.second = ids[i++];
                if (i >= ids.size())return positions;
            }
        }
    }

    return positions;
}

vector<pair<shared_ptr<Component>, vector<pair<shared_ptr<Job>, int>>>> Organization::GetJobs() const {
    return jobs;
}

void Organization::AddVacancy(shared_ptr<Component> component, vector<shared_ptr<Job>> vacancies) {
    vector< pair<shared_ptr<Job>, int>> positions;
    for (auto vacancy : vacancies) {
        positions.push_back(make_pair(vacancy, -1));
    }
    jobs.push_back(make_pair(component, positions));
}

void OrganizationFactory::RegisterOrganization(const string& id, function<unique_ptr<Organization>()> creator, float power) {
    registries[id] = creator;
    powers[id] = power;
}

unique_ptr<Organization> OrganizationFactory::CreateOrganization(const string& id) {
    if(configs.find(id) == configs.end() || !configs.find(id)->second)return nullptr;
    
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
    configs[name] = config;
}

const unordered_map<string, float>& OrganizationFactory::GetPowers() const {
    return powers;
}


