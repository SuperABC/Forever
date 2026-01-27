#include "organization.h"


using namespace std;

string DefaultOrganization::GetId() {
	return "default";
}

string DefaultOrganization::GetType() const {
	return "default";
}

string DefaultOrganization::GetName() const {
	return "默认组织";
}

float DefaultOrganization::GetPower() {
    return 1.0f;
}

vector<pair<string, pair<int, int>>> DefaultOrganization::ComponentRequirements() const {
    vector<pair<string, pair<int, int>>> requirements;
    requirements.emplace_back("default_working", make_pair(1, 1));

    return requirements;
}

vector<pair<string, vector<vector<string>>>> DefaultOrganization::ArrageVacancies(
    vector<pair<string, int>> components) const {
    vector<pair<string, vector<vector<string>>>> jobs;
    for (auto& comp : components) {
        jobs.emplace_back(comp.first, vector<vector<string>>(comp.second));
        for (int i = 0; i < comp.second; i++) {
            jobs.back().second[i] = vector<string>(10, "default");
        }
    }

    return jobs;
}

void DefaultOrganization::SetCalendar(CalendarFactory* factory) {
    for (auto &component : jobs) {
        for (auto &job : component.second) {
            shared_ptr<Calendar> calendar = factory->CreateCalendar("full");
            job.first->SetCalendar(calendar);
        }
    }
}

void DefaultOrganization::ArrangeRooms() {
    for (auto &component : jobs) {
        for (auto &job : component.second) {
            job.first->SetPosition(component.first->GetRooms()[0]);
        }
    }
}

