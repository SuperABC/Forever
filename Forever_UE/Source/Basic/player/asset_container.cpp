#include "asset_container.h"

using namespace std;

int ContainerAsset::count = 0;

ContainerAsset::ContainerAsset() : id(count++) {
}

const char* ContainerAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void ContainerAsset::SetProperty() {
	mobility = AssetMobility::Object;
	weight = 2.0f;
	size = 5.0f;
	volume = 50.0f;
	backpack = true;
}
