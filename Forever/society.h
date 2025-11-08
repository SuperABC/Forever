#pragma once

#include "organization.h"

#include <windows.h>


class Society {
public:
	Society();
	~Society();

	// 读取Mods
	void InitOrganizations();

	// 读取配置文件
	void ReadConfigs(std::string path) const;

private:
	// Mod管理
	std::vector<HMODULE> modHandles;
	std::unique_ptr<OrganizationFactory> organizationFactory;
};

