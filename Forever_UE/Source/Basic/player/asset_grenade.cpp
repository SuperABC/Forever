#include "asset_grenade.h"

using namespace std;

int GrenadeAsset::count = 0;

GrenadeAsset::GrenadeAsset() : id(count++) {
}

const char* GrenadeAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void GrenadeAsset::SetProperty() {
	mobility = AssetMobility::Object;
	weight = 0.4f;
	size = 0.3f;
	throwable = true;
	meshPath = "/Game/3rdParty/Pineapple_grenade/StaticMeshes/pineapple_grenade.pineapple_grenade";
	meshScale = 1.f; // 先按模型原始尺寸，PIE里看大小再调
	// 脚本：扔出后(throw_asset)计时，到时生成爆炸特效并销毁自己，见Resource/Story/asset_grenade.script。
	milestoneNames = { "asset_grenade" };
}
