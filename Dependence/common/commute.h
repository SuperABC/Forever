#pragma once

#include <vector>

#include "../map/roadnet_base.h"
#include "utility.h"


class Commute {
public:
	Commute();
	~Commute();

	// 通勤有效性
	void SetNull();
	bool IsNull();

	// 通勤目标
	void SetTarget(std::shared_ptr<Room> target);
	std::shared_ptr<Room> GetTarget();

	// 通勤开始
	void SetPaths(std::vector<Connection> paths);
	void StartCommute(Time time);

	// 通勤结束
	bool AtConnection(Connection connection, Time time);
	bool FinishCommute(Time time);


private:
	std::vector<Connection> currentPaths;
	Time startTime;

	std::shared_ptr<Room> targetRoom;
};