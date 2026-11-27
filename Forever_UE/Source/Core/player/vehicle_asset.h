#pragma once

#include "class.h"

#include "player/asset.h"


// VehicleAsset：载具资产(AssetMobility::Vehicle)，Core内置的Asset子类，没有AssetMod——载具之间的差异
// 已经由VehicleMod承担，不需要每个载具mod再配一个资产mod。指向一辆Vehicle，不拥有它(Vehicle归Traffic)。
// 资产本身由Traffic持有，挂在所有者(Citizen或Organization)的资产列表上。详见vehicle_asset.md。
class VehicleAsset : public Asset {
public:
	VehicleAsset(Vehicle* vehicle, Citizen* ownerCitizen, Organization* ownerOrganization);

	Vehicle* GetVehicle() const { return vehicle; }
	Citizen* GetOwnerCitizen() const { return ownerCitizen; }
	// 继承(自然死亡，见populace/demography.md)：只改字段，调用方负责把资产从旧主人的资产列表挪到新主人的。
	void SetOwnerCitizen(Citizen* value) { ownerCitizen = value; }
	Organization* GetOwnerOrganization() const { return ownerOrganization; }

private:
	Vehicle* vehicle;
	Citizen* ownerCitizen;
	Organization* ownerOrganization; // 两者都空=政府(目前没有)
};
