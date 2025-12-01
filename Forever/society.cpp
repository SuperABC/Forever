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
    calendarFactory->RegisterCalendar(TestCalendar::GetId(), []() { return make_unique<TestCalendar>(); });

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
    auto calendarList = { "test", "mod" };
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
    organizationFactory->RegisterOrganization(TestOrganization::GetId(),
        []() { return make_unique<TestOrganization>(); }, TestOrganization::GetPower());

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
    auto organizationList = { "test", "mod" };
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

void Society::Init(std::unique_ptr<Map>& map, std::unique_ptr<Populace>& populace) {
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
		std::vector<std::pair<std::string, int>> usedComponents;
        for (auto& req : requirements) {
            auto it = componentMap.find(req.first);
            if (it->second.size() < req.second.second) {
                usedComponents.emplace_back(req.first, (int)it->second.size());
            }
            else {
                usedComponents.emplace_back(req.first, req.second.first + GetRandom(req.second.second - req.second.first));
            }
        }

		auto jobArrangements = organization->ArrageJobs(usedComponents);
        for (int i = 0; i < usedComponents.size(); i++) {
            auto& type = usedComponents[i].first;
            auto& count = usedComponents[i].second;
            auto& availableComponents = componentMap[type];
            for (int j = 0; j < count; j++) {
                auto component = availableComponents.back();
                availableComponents.pop_back();
                vector<shared_ptr<Job>> jobs;
                for (auto& jobName : jobArrangements[i].second) {
                    shared_ptr<Job> job = populace->GetJobFactory()->CreateJob(jobName);
                    if (job) {
                        jobs.push_back(job);
                    }
                }
                organization->AddMapping(component, jobs);
            }
        }

		organization->SetCalendar(calendarFactory.get());
	}
}

void Society::ReadConfigs(string path) const {
    if (!filesystem::exists(path)) {
        THROW_EXCEPTION(IOException, "Path does not exist: " + path + ".\n");
    }

    Json::Reader reader;
    Json::Value root;

    ifstream fin(path);
    if (!fin.is_open()) {
        THROW_EXCEPTION(IOException, "Failed to open file: " + path + ".\n");
    }
    if (reader.parse(fin, root)) {

    }
    else {
        fin.close();
        THROW_EXCEPTION(JsonFormatException, "Json syntax error: " + reader.getFormattedErrorMessages() + ".\n");
    }
    fin.close();
}

void Society::Destroy() {
    organizations.clear();
}

void Society::Tick() {

}

void Society::Print() {

}

void Society::ApplyChange(shared_ptr<Change> change, unique_ptr<Script>& script) {

}

void Society::Load(string path) {

}

void Society::Save(string path) {

}



