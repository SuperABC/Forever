#pragma once

#include "organization.h"
#include "map.h"
#include "populace.h"

#include <windows.h>


class Society {
public:
	Society();
	~Society();

	// 读取Mods
	void InitOrganizations();

	// 初始化全部组织
	void Init(std::shared_ptr<Map> map, std::shared_ptr<Populace>populace);

	// 读取配置文件
	void ReadConfigs(std::string path) const;

	// 释放空间
	void Destroy();

	// 时钟前进
	void Tick();

	// 输出当前地图
	void Print();

	// 应用变更
	void ApplyChange(std::shared_ptr<Change> change, std::unique_ptr<Script>& script);

	// 保存/加载地图
	void Load(std::string path);
	void Save(std::string path);

private:
	// Mod管理
	std::vector<HMODULE> modHandles;
	std::unique_ptr<OrganizationFactory> organizationFactory;

	std::vector<std::shared_ptr<Organization>> organizations;
};

