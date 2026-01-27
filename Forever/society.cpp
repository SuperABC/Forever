#include "utility.h"
#include "error.h"
#include "society.h"
#include "json.h"

#include <fstream>
#include <filesystem>


using namespace std;

Society::Society() {
    calendarFactory.reset(new CalendarFactory());
    organizationFactory.reset(new OrganizationFactory());
}

Society::~Society() {
    for (auto& mod : modHandles) {
        if (mod) {
            FreeLibrary(mod);
        }
    }
    modHandles.clear();
}

void Society::InitCalendars() {
    calendarFactory->RegisterCalendar(StandardCalendar::GetId(), []() { return make_unique<StandardCalendar>(); });
    calendarFactory->RegisterCalendar(FullCalendar::GetId(), []() { return make_unique<FullCalendar>(); });

    HMODULE modHandle = LoadLibraryA(REPLACE_PATH("Mod.dll"));
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModCalendarsFunc registerFunc = (RegisterModCalendarsFunc)GetProcAddress(modHandle, "RegisterModCalendars");
        if (registerFunc) {
            registerFunc(calendarFactory.get());
        }
        else {
            debugf("Incorrect dll content.");
        }
    }
    else {
        debugf("Failed to load mod.dll.");
    }

#ifdef MOD_TEST
    auto calendarList = { "mod" };
    for (const auto& calendarId : calendarList) {
        if (calendarFactory->CheckRegistered(calendarId)) {
            auto calendar = calendarFactory->CreateCalendar(calendarId);
            debugf(("Created calendar: " + calendar->GetName() + " (ID: " + calendarId + ")\n").data());
        }
        else {
            debugf("Calendar not registered: %s\n", calendarId);
        }
    }
#endif // MOD_TEST
}

void Society::InitOrganizations() {
    organizationFactory->RegisterOrganization(DefaultOrganization::GetId(),
        []() { return make_unique<DefaultOrganization>(); }, DefaultOrganization::GetPower());

    HMODULE modHandle = LoadLibraryA(REPLACE_PATH("Mod.dll"));
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully\n");

        RegisterModOrganizationsFunc registerFunc = (RegisterModOrganizationsFunc)GetProcAddress(modHandle, "RegisterModOrganizations");
        if (registerFunc) {
            registerFunc(organizationFactory.get());
        }
        else {
            debugf("Incorrect dll content\n");
        }
    }
    else {
        debugf("Failed to load mod.dll\n");
    }

#ifdef MOD_TEST
    auto organizationList = { "mod" };
    for (const auto& organizationId : organizationList) {
        if (organizationFactory->CheckRegistered(organizationId)) {
            auto organization = organizationFactory->CreateOrganization(organizationId);
            debugf(("Created organization: " + organization->GetName() + " (ID: " + organizationId + ")\n").data());
        }
        else {
            debugf("Organization not registered: %s\n", organizationId);
        }
    }
#endif // MOD_TEST

}

void Society::ReadConfigs(string path) const {
    if (!filesystem::exists(path)) {
        THROW_EXCEPTION(IOException, "Path does not exist: " + path + ".\n");
    }

    JsonReader reader;
    JsonValue root;

    ifstream fin(path);
    if (!fin.is_open()) {
        THROW_EXCEPTION(IOException, "Failed to open file: " + path + ".\n");
    }
    if (reader.Parse(fin, root)) {
        for (auto calendar : root["mods"]["calendar"]) {
            calendarFactory->SetConfig(calendar.AsString(), true);
        }
        for (auto organization : root["mods"]["organization"]) {
            organizationFactory->SetConfig(organization.AsString(), true);
        }
    }
    else {
        fin.close();
        THROW_EXCEPTION(JsonFormatException, "Json syntax error: " + reader.GetErrorMessages() + ".\n");
    }
    fin.close();
}

void Society::Init(unique_ptr<Map>& map, unique_ptr<Populace>& populace) {
    // 计算职缺
	auto components = map->GetComponents();

	unordered_map<string, vector<shared_ptr<Component>>> componentMap;
    for (auto component : components) {
        if(componentMap.find(component->GetType()) == componentMap.end())
			componentMap[component->GetType()] = vector<shared_ptr<Component>>();
		componentMap[component->GetType()].push_back(component);
    }

    auto powers = organizationFactory->GetPowers();
    vector<pair<string, float>> cdfs;
    float sum = 0.f;
    for (auto power : powers) {
        sum += power.second;
        cdfs.emplace_back(power.first, sum);
    }
    if (sum == 0.f) {
        THROW_EXCEPTION(InvalidArgumentException, "No valid organization for generation.\n");
    }
    for (auto& cdf : cdfs) {
        cdf.second /= sum;
    }

	int attempt = 0;
    while(attempt < 16) {
        float r = GetRandom(1000) / 1000.f;
        string selectedOrganization;
        for (auto& cdf : cdfs) {
            if (r <= cdf.second) {
                selectedOrganization = cdf.first;
                break;
            }
        }
        shared_ptr<Organization> organization = organizationFactory->CreateOrganization(selectedOrganization);
        if (!organization) {
            attempt++;
            continue;
        }

        auto requirements = organization->ComponentRequirements();
        bool valid = true;
        for (auto& req : requirements) {
            auto it = componentMap.find(req.first);
            if (it == componentMap.end()) {
                valid = false;
                break;
            }
            if(it->second.size() < req.second.first) {
                valid = false;
                break;
			}
        }
        if (!valid) {
            attempt++;
            continue;
        }

        organizations.push_back(organization);
		vector<pair<string, int>> usedComponents;
        for (auto& req : requirements) {
            auto it = componentMap.find(req.first);
            if (it->second.size() < req.second.second) {
                usedComponents.emplace_back(req.first, (int)it->second.size());
            }
            else {
                usedComponents.emplace_back(req.first, req.second.first + GetRandom(req.second.second - req.second.first + 1));
            }
        }

		auto jobArrangements = organization->ArrageVacancies(usedComponents);
        for (int i = 0; i < usedComponents.size(); i++) {
            auto& type = usedComponents[i].first;
            auto& count = usedComponents[i].second;
            auto& availableComponents = componentMap[type];
            for (int j = 0; j < count; j++) {
                auto component = availableComponents.back();
                availableComponents.pop_back();
                vector<shared_ptr<Job>> jobs;
                for (auto& componentName : jobArrangements[i].second) {
                    for (auto& jobName : componentName) {
                        shared_ptr<Job> job = populace->GetJobFactory()->CreateJob(jobName);
                        if (job) {
                            jobs.push_back(job);
                        }
                    }
                }
                organization->AddVacancy(component, jobs);
            }
        }

		organization->SetCalendar(calendarFactory.get());
        organization->ArrangeRooms();
	}

    // 分配工作
    vector<shared_ptr<Organization>> temps;
    for (auto organization : organizations) {
        temps.push_back(organization);
    }
    auto adults = vector<shared_ptr<Person>>();
    for (auto citizen : populace->GetCitizens()) {
        if (citizen->GetAge(populace->GetTime()) < 18)continue;
        if (citizen->GetAge(populace->GetTime()) >= 60)continue;
        adults.push_back(citizen);
    }
    for (int i = 0; i < adults.size(); i++) {
        vector<int> ids;
        ids.push_back(adults[i]->GetId());
        int r = GetRandom((int)temps.size());
        auto jobs = temps[r]->EnrollEmployee(ids);
        if (jobs.size() > 0) {
            adults[i]->AddJob(jobs[0]);
        }
        else {
            temps[r] = temps.back();
            temps.pop_back();
            if (temps.size() <= 0)break;
            i--;
        }
    }
}

void Society::Destroy() {
    organizations.clear();
}

void Society::Tick(int day, int hour, int min, int sec) {

}

void Society::Print() const {

}

void Society::ApplyChange(shared_ptr<Change> change, unique_ptr<Story>& story) {

}

void Society::Load(string path) {

}

void Society::Save(string path) const {

}



