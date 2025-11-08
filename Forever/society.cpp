#include "util.h"
#include "error.h"
#include "society.h"
#include "json.h"

#include <fstream>
#include <filesystem>


using namespace std;

Society::Society() {
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

void Society::InitOrganizations() {
    organizationFactory->RegisterOrganization(TestOrganization::GetId(), []() { return std::make_unique<TestOrganization>(); });

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

void Society::ReadConfigs(std::string path) const {
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
