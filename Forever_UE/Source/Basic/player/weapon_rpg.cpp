#include "weapon_rpg.h"

#include "common/handle.h"
#include "common/json.h"

using namespace std;

int RpgWeapon::count = 0;

RpgWeapon::RpgWeapon() : id(count++) {
}

const char* RpgWeapon::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void RpgWeapon::SetProperty() {
	// 火箭筒：单发(装填一发、换弹3秒)，射线只用来给脚本提供瞄准点(weapon_fire的hit)，射线本身不造成任何伤害；
	// 开火后脚本(Resource/Story/weapon_rpg.script)用spawn_actor在枪口放出火箭弹Actor，爆炸伤害由火箭弹撞击后的projectile_hit里程碑结算。
	fireMode = FireMode::Hitscan;
	milestoneNames = { "weapon_rpg" };
	fireRate = 1.f;
	fullAuto = false;
	maxRange = 50000.f;    // 500米，瞄准点射线的最远距离
	baseSpread = 0.f;
	magazineCapacity = 1;
	reloadDuration = 3.f;

	// 发射器是空的(火箭弹是开火时才出现的独立Actor)。模型原点在发射器尾端、枪口朝+X，见Tools/split_rpg7.py。
	firstPersonMeshPath = "/Game/3rdParty/RPG7/StaticMeshes/SM_RPG7_Launcher.SM_RPG7_Launcher";

	// 3D变换：缩放和枪口位置按实测模型尺寸估(见weapon_rpg.md)，挂载偏移先沿用步枪，后面再调。
	modelScale = 1.f;
	attachOffsetX = -24.f; // 瞄准时比步枪靠后：模型长95.7cm的1/4
	attachOffsetY = 7.5f;  // 瞄准时比步枪靠右：模型宽(瞄准时侧向约15cm)的一半
	attachOffsetZ = 15.f;  // 瞄准时比步枪高15cm
	// 背枪：模型长95.7cm、背着时侧向宽约27cm。相对步枪的值(-5, 4, 0)：降低长度的1/3=32，向后背中心(Y=0)平移宽度的一半=13.6。
	backOffsetX = -5.f;
	backOffsetY = 4.f - 13.6f;
	backOffsetZ = -32.f;
	muzzleOffsetX = 96.f;
	muzzleOffsetZ = 0.f;

	// 单发后坐力很大。
	recoilPitchMin = 5.f;
	recoilPitchMax = 7.f;
	recoilYawMin = -1.f;
	recoilYawMax = 1.f;

	ammoType = "asset_ammo_rpg";
}
