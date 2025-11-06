#include "util.h"
#include "populace.h"


using namespace std;

Populace::Populace() {

}

Populace::~Populace() {
    for (auto& mod : modHandles) {
        if (mod) {
            FreeLibrary(mod);
        }
    }
    modHandles.clear();
}

void Populace::InitJobs(std::shared_ptr<JobFactory> factory) {
    factory->RegisterJob("test", []() { return std::make_unique<TestJob>(); });

    HMODULE modHandle = LoadLibraryA("Mod.dll");
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModJobsFunc registerFunc = (RegisterModJobsFunc)GetProcAddress(modHandle, "RegisterModJobs");
        if (registerFunc) {
            registerFunc(factory.get());
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
        if (factory->CheckRegistered(jobId)) {
            auto job = factory->CreateJob(jobId);
            debugf(("Created job: " + job->GetName() + " (ID: " + jobId + ")\n").data());
        }
        else {
            debugf("Job not registered: %s\n", jobId);
        }
    }
#endif // MOD_TEST

}