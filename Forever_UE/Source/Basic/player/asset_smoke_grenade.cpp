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
	// 引擎自带球体(直径100cm)缩到10cm的小球；烟雾效果以后另做，目前没有脚本。
	meshPath = "/Engine/BasicShapes/Sphere.Sphere";
	meshScale = 0.1f;
}
