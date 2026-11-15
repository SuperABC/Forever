#include "player/estate_asset.h"

#include "map/building.h"
#include "map/room.h"
#include "map/zone.h"

using namespace std;

EstateAsset::EstateAsset(Zone* zone, Citizen* owner) :
	Asset(AssetMobility::Estate, zone ? zone->GetAddress() : string(), "estate"),
	level(Level::Zone), zone(zone), owner(owner) {}

EstateAsset::EstateAsset(Building* building, Citizen* owner) :
	Asset(AssetMobility::Estate, building ? building->GetAddress() : string(), "estate"),
	level(Level::Building), building(building), owner(owner) {}

EstateAsset::EstateAsset(Room* room, Citizen* owner) :
	Asset(AssetMobility::Estate, room ? room->GetAddress() : string(), "estate"),
	level(Level::Room), room(room), owner(owner) {}
