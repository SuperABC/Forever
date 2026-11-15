#pragma once

#include "class.h"

#include "player/asset.h"

#include <string>


// EstateAsset：不动产资产(AssetMobility::Estate)，Core内置的Asset子类，没有AssetMod——不动产没有mod
// 的概念，指向的是已经存在的Zone/Building/Room，本身不拥有它们。由Map持有，Map::BuildEstateAssets()
// 按归属(owner/stated)建立，挂到Citizen或政府(Society)的资产列表上。详见estate_asset.md。
class EstateAsset : public Asset {
public:
	enum class Level { Zone, Building, Room };

	EstateAsset(Zone* zone, Citizen* owner);
	EstateAsset(Building* building, Citizen* owner);
	EstateAsset(Room* room, Citizen* owner);

	Level GetLevel() const { return level; }
	Zone* GetZone() const { return zone; }
	Building* GetBuilding() const { return building; }
	Room* GetRoom() const { return room; }

	// 所有者：私有归属是对应市民，公有(stated)为nullptr(即政府)。
	Citizen* GetOwner() const { return owner; }

private:
	Level level;
	Zone* zone = nullptr;
	Building* building = nullptr;
	Room* room = nullptr;
	Citizen* owner = nullptr;
};
