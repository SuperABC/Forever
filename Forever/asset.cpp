#include "asset.h"


using namespace std;

string ZoneAsset::GetId() {
	return "zoneAsset";
}

string ZoneAsset::GetType() const {
	return "zoneAsset";
}

string ZoneAsset::GetName() const {
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

string BuildingAsset::GetId() {
	return "buildingAsset";
}

string BuildingAsset::GetType() const {
	return "buildingAsset";
}

string BuildingAsset::GetName() const {
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

string RoomAsset::GetId() {
	return "roomAsset";
}

string RoomAsset::GetType() const {
	return "roomAsset";
}

string RoomAsset::GetName() const {
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
