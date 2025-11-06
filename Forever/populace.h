#pragma once

#include "job.h"

#include <windows.h>


class Populace {
public:
	Populace();
	~Populace();

	void InitJobs(std::shared_ptr<JobFactory> factory);

private:
	std::vector<HMODULE> modHandles;
};

