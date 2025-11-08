#pragma once

#include "job.h"

#include <windows.h>


class Populace {
public:
	Populace();
	~Populace();

	// 读取Mods
	void InitJobs();

	// 读取配置文件
	void ReadConfigs(std::string path) const;

private:
	// Mod管理
	std::vector<HMODULE> modHandles;
	std::unique_ptr<JobFactory> jobFactory;
};

