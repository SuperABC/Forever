#include "asset.h"


using namespace std;

TestAsset::TestAsset() {

}

TestAsset::~TestAsset() {

}

ZoneAsset::ZoneAsset(shared_ptr<Zone> zone) : zone(zone) {

}

ZoneAsset::~ZoneAsset() {

}

shared_ptr<Zone> ZoneAsset::GetZone() {
	return zone;
}

void ZoneAsset::SetZone(shared_ptr<Zone> zone) {
	this->zone = zone;
}

BuildingAsset::BuildingAsset(shared_ptr<Building> building) : building(building) {

}

BuildingAsset::~BuildingAsset() {

}

shared_ptr<Building> BuildingAsset::GetBuilding() {
	return building;
}

void BuildingAsset::SetBuilding(shared_ptr<Building> building) {
	this->building = building;
}

RoomAsset::RoomAsset(shared_ptr<Room> room) : room(room) {

}

RoomAsset::~RoomAsset() {

}

shared_ptr<Room> RoomAsset::GetRoom() {
	return room;
}

void RoomAsset::SetRoom(shared_ptr<Room> room) {
	this->room = room;
}
