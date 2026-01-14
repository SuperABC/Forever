#include "organization.h"


using namespace std;

string HotelOrganization::GetId() {
	return "hotel";
}

string HotelOrganization::GetType() const {
	return "hotel";
}

string HotelOrganization::GetName() const {
	return "酒店组织";
}

float HotelOrganization::GetPower() {
    return 1.0f;
}

vector<pair<string, pair<int, int>>> HotelOrganization::ComponentRequirements() const {
    vector<pair<string, pair<int, int>>> requirements;
    requirements.emplace_back("hotel", make_pair(1, 1));

    return requirements;
}

vector<pair<string, vector<vector<string>>>> HotelOrganization::ArrageVacancies(
    vector<pair<string, int>> components) const {
    vector<pair<string, vector<vector<string>>>> jobs;
    for (auto& comp : components) {
        jobs.emplace_back(comp.first, vector<vector<string>>(comp.second));
        for (int i = 0; i < comp.second; i++) {
            jobs.back().second[i] = vector<string>(10, "hotel_cleaner");
        }
    }

    return jobs;
}

void HotelOrganization::SetCalendar(CalendarFactory* factory) {
    for (auto &component : jobs) {
        for (auto &job : component.second) {
            shared_ptr<Calendar> calendar = factory->CreateCalendar("standard");
            job.first->SetCalendar(calendar);
        }
    }
}

void HotelOrganization::ArrangeRooms() {
    for (auto &component : jobs) {
        for (auto &job : component.second) {
            job.first->SetPosition(component.first->GetRooms()[0]);
        }
    }
}

