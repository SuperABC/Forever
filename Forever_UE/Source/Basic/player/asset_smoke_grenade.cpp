#include "asset_smoke_grenade.h"

using namespace std;

int SmokeGrenadeAsset::count = 0;

SmokeGrenadeAsset::SmokeGrenadeAsset() : id(count++) {
}

const char* SmokeGrenadeAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void SmokeGrenadeAsset::SetProperty() {
	mobility = AssetMobility::Object;
	weight = 0.4f;
	size = 0.3f;
	throwable = true;
	meshPath = "/Game/3rdParty/Pineapple_grenade/StaticMeshes/pineapple_grenade.pineapple_grenade";
	meshScale = 1.f;
}
