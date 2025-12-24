#include "room.h"


using namespace std;

string ParkingRoom::GetId() {
	return "parking";
}

string ParkingRoom::GetType() const {
	return "parking";
}

string ParkingRoom::GetName() const {
	return "停车场房间";
}

bool ParkingRoom::IsResidential() const {
	return false;
}

bool ParkingRoom::IsWorkspace() const {
	return false;
}

int ParkingRoom::GetLivingCapacity() const {
	return 0;
}

int ParkingRoom::GetPersonnelCapacity() const {
	return 0;
}

string FlatRoom::GetId() {
	return "flat";
}

string FlatRoom::GetType() const {
	return "flat";
}

string FlatRoom::GetName() const {
	return "公寓房间";
}

bool FlatRoom::IsResidential() const {
	return true;
}

bool FlatRoom::IsWorkspace() const {
	return false;
}

int FlatRoom::GetLivingCapacity() const {
	return 2;
}

int FlatRoom::GetPersonnelCapacity() const {
	return 0;
}

string HotelRoom::GetId() {
	return "hotel";
}

string HotelRoom::GetType() const {
	return "hotel";
}

string HotelRoom::GetName() const {
	return "酒店房间";
}

bool HotelRoom::IsResidential() const {
	return false;
}

bool HotelRoom::IsWorkspace() const {
	return false;
}

int HotelRoom::GetLivingCapacity() const {
	return 0;
}

int HotelRoom::GetPersonnelCapacity() const {
	return 0;
}

string LobbyRoom::GetId() {
	return "lobby";
}

string LobbyRoom::GetType() const {
	return "lobby";
}

string LobbyRoom::GetName() const {
	return "大堂房间";
}

bool LobbyRoom::IsResidential() const {
	return false;
}

bool LobbyRoom::IsWorkspace() const {
	return true;
}

int LobbyRoom::GetLivingCapacity() const {
	return 0;
}

int LobbyRoom::GetPersonnelCapacity() const {
	return 100;
}
