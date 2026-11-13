#include "asset_beef.h"

using namespace std;

int BeefAsset::count = 0;

BeefAsset::BeefAsset() : id(count++) {
}

const char* BeefAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void BeefAsset::SetProperty() {
	mobility = AssetMobility::Containee;
	weight = 0.1f;
	size = 0.1f;
}
