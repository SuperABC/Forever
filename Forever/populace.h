#pragma once

#include "job.h"

#include <windows.h>


class Populace {
public:
	Populace();
	~Populace();

	void InitJobs();

private:
	std::vector<HMODULE> modHandles;
	std::unique_ptr<JobFactory> jobFactory;
};

