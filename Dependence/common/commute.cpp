#include "commute.h"


using namespace std;

Commute::Commute() {

}

Commute::~Commute() {

}

void Commute::SetNull() {
	currentPaths.clear();
}

bool Commute::IsNull() {
	return currentPaths.size() == 0;
}

void Commute::SetTarget(shared_ptr<Room> target) {
	targetRoom = target;
}

shared_ptr<Room> Commute::GetTarget() {
	return targetRoom;
}

void Commute::SetPaths(std::vector<std::pair<Connection, Node>> paths) {
	currentPaths = paths;
}

void Commute::StartCommute(Time time) {
	startTime = time;
}

bool Commute::AtConnection(Connection connection, Time time) {
	return false;
}

bool Commute::FinishCommute(Time time) {
	return false;
}
