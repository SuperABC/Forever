#include "util.h"
#include "society.h"


using namespace std;

Society::Society() {
    organizationFactory = make_shared<OrganizationFactory>();
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
    organizationFactory->RegisterOrganization("test", []() { return std::make_unique<TestOrganization>(); });

    HMODULE modHandle = LoadLibraryA("Mod.dll");
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