#include "util.h"
#include "error.h"
#include "populace.h"
#include "json.h"

#include <fstream>
#include <filesystem>


using namespace std;

Populace::Populace() {
    jobFactory.reset(new JobFactory());
}

Populace::~Populace() {
    for (auto& mod : modHandles) {
        if (mod) {
            FreeLibrary(mod);
        }
    }
    modHandles.clear();
}

void Populace::InitJobs() {
    jobFactory->RegisterJob(TestJob::GetId(), []() { return std::make_unique<TestJob>(); });

    HMODULE modHandle = LoadLibraryA(REPLACE_PATH("Mod.dll"));
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModJobsFunc registerFunc = (RegisterModJobsFunc)GetProcAddress(modHandle, "RegisterModJobs");
        if (registerFunc) {
            registerFunc(jobFactory.get());
        }
        else {
            debugf("Incorrect dll content.");
        }
    }
    else {
        debugf("Failed to load mod.dll.");
    }

#ifdef MOD_TEST
    auto jobList = { "test", "mod" };
    for (const auto& jobId : jobList) {
        if (jobFactory->CheckRegistered(jobId)) {
            auto job = jobFactory->CreateJob(jobId);
            debugf(("Created job: " + job->GetName() + " (ID: " + jobId + ")\n").data());
        }
        else {
            debugf("Job not registered: %s\n", jobId);
        }
    }
#endif // MOD_TEST

}

void Populace::ReadConfigs(std::string path) const {
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
