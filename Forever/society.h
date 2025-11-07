#pragma once

#include "organization.h"

#include <windows.h>


class Society {
public:
	Society();
	~Society();

	void InitOrganizations();

private:
	std::vector<HMODULE> modHandles;
	std::shared_ptr<OrganizationFactory> organizationFactory;
};

