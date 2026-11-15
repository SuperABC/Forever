#include "player/vehicle_asset.h"

#include "traffic/vehicle.h"

using namespace std;

VehicleAsset::VehicleAsset(Vehicle* vehicle, Citizen* ownerCitizen, Organization* ownerOrganization) :
	Asset(AssetMobility::Vehicle, vehicle ? vehicle->GetName() : string(), "vehicle"),
	vehicle(vehicle), ownerCitizen(ownerCitizen), ownerOrganization(ownerOrganization) {}
