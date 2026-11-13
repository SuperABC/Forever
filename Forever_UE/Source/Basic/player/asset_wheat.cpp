#include "asset_wheat.h"

using namespace std;

int WheatAsset::count = 0;

WheatAsset::WheatAsset() : id(count++) {
}

const char* WheatAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void WheatAsset::SetProperty() {
	mobility = AssetMobility::Containee;
	weight = 0.1f;
	size = 0.1f;
}
