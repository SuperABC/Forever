#include "weapon_camera.h"

#include "common/handle.h"
#include "common/json.h"

using namespace std;

int CameraWeapon::count = 0;

CameraWeapon::CameraWeapon() : id(count++) {
}

const char* CameraWeapon::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void CameraWeapon::SetProperty() {
	// 相机：不用弹药(magazineCapacity=0)、没有伤害/散射/后坐力，开火走Custom：OnFire里向Core发"screenshot"请求截屏。
	fireMode = FireMode::Custom;
	damage = 0.f;
	fireRate = 1.f;      // 两次拍照最小间隔
	fullAuto = false;
	maxRange = 0.f;
	baseSpread = 0.f;
	magazineCapacity = 0;
	reloadDuration = 0.f;
	recoilPitchMin = 0.f;
	recoilPitchMax = 0.f;
	recoilYawMin = 0.f;
	recoilYawMax = 0.f;

	// 非"_C"结尾=单个StaticMesh；镜头已在模型里朝+X。目录名里是en dash(U+2013)，不是普通连字符。
	firstPersonMeshPath = "/Game/3rdParty/Digital_Camera_3D_Model_–_Low_Poly_DSLR/StaticMeshes/15_camera.15_camera";

	// 3D变换先给估值，后面按模型实际大小在PIE里调。
	modelScale = 0.2f;
	attachOffsetX = 0.f;
	attachOffsetZ = 0.f;
	backOffsetX = -5.f;
	backOffsetY = -6.f;
	backOffsetZ = 0.f;
}

void CameraWeapon::OnFire(PostHandle* post) {
	if (!post) return;

	// 截屏由Core转给UE层执行(Core看不到UE)，文件名前缀交给UE层拼时间戳存放。
	JsonValue request(DATA_OBJECT);
	request["post"] = "screenshot";
	request["name"] = "photo";
	post->Post(request);
}
