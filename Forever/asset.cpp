#include "asset.h"


using namespace std;

std::string ZoneAsset::GetId() {
	return "zoneAsset";
}

std::string ZoneAsset::GetType() const {
	return "zoneAsset";
}

std::string ZoneAsset::GetName() const {
	return "园区资产";
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

std::string BuildingAsset::GetId() {
	return "buildingAsset";
}

std::string BuildingAsset::GetType() const {
	return "buildingAsset";
}

std::string BuildingAsset::GetName() const {
	return "建筑资产";
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

std::string RoomAsset::GetId() {
	return "roomAsset";
}

std::string RoomAsset::GetType() const {
	return "roomAsset";
}

std::string RoomAsset::GetName() const {
	return "房间资产";
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
