#include "organization.h"


using namespace std;

std::string HotelOrganization::GetId() {
	return "hotel";
}

std::string HotelOrganization::GetType() const {
	return "hotel";
}

std::string HotelOrganization::GetName() const {
	return "酒店组织";
}

float HotelOrganization::GetPower() {
    return 1.0f;
}

std::vector<std::pair<std::string, std::pair<int, int>>> HotelOrganization::ComponentRequirements() const {
    std::vector<std::pair<std::string, std::pair<int, int>>> requirements;
    requirements.emplace_back("hotel", std::make_pair(1, 1));

    return requirements;
}

std::vector<std::pair<std::string, std::vector<std::vector<std::string>>>> HotelOrganization::ArrageVacancies(
    std::vector<std::pair<std::string, int>> components) const {
    std::vector<std::pair<std::string, std::vector<std::vector<std::string>>>> jobs;
    for (auto& comp : components) {
        jobs.emplace_back(comp.first, std::vector<std::vector<std::string>>(comp.second));
        for (int i = 0; i < comp.second; i++) {
            jobs.back().second[i] = std::vector<std::string>(10, "hotel_cleaner");
        }
    }

    return jobs;
}

void HotelOrganization::SetCalendar(CalendarFactory* factory) {
    for (auto component : jobs) {
        for (auto job : component.second) {
            std::shared_ptr<Calendar> calendar = factory->CreateCalendar("test");
            job.first->SetCalendar(calendar);
        }
    }
}

