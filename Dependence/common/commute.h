#pragma once

#include <vector>

#include "../map/roadnet_base.h"
#include "utility.h"


class Commute {
public:
	Commute();
	~Commute();

	void SetNull();
	bool IsNull();

	void SetTarget(std::shared_ptr<Room> target);
	std::shared_ptr<Room> GetTarget();

	void SetPaths(std::vector<std::pair<Connection, Node>> paths);
	void StartCommute(Time time);

	bool AtConnection(Connection connection, Time time);
	bool FinishCommute(Time time);


private:
	std::vector<std::pair<Connection, Node>> currentPaths;
	Time startTime;

	std::shared_ptr<Room> targetRoom;
};