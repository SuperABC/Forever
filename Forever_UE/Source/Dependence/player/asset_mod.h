#pragma once

#include "common/mod_state.h"

#include <string>
// AssetMod：物品/资产的类型定义(目录性质，比照ProductMod/WeaponMod，不迁移老工程
// DefineEstate()/DefineVehicle()/DefineObject()/DefineContainer()/DefineContainee()那套
// 工具方法)——具体子类必须在SetProperty()里赋值公开字段，不要在构造函数里赋值，见
// asset_basic.h。
//
// 四分类(不动产/载具/独立物体/容器内容物)是同一个mobility枚举字段，不是四个子类，照抄
// 老工程"是否为容器"用volume>0表达、和mobility正交的设计，见Source/Core/player/asset.md。
enum class AssetMobility { Estate, Vehicle, Object, Containee };

class AssetMod {
public:
	AssetMod() = default;
	virtual ~AssetMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 具体子类必须在这里填下面这几个字段，不要在构造函数里赋值——构造函数只负责id/count
	// 这类登记，和StorageMod::SetProperty()/RoomMod::SetProperty()同一套"两段式"约定。
	// Core创建完mod实例后会立刻调一次这个方法，再读这几个字段。
	virtual void SetProperty() {}

	AssetMobility mobility = AssetMobility::Object;
	float weight = 0.f;
	float size = 0.f;
	float volume = 0.f;    // >0 即为容器，可以AddContent
	bool backpack = false; // 能否放进"后背"槽(容器类通常为true)
	bool weapon = false;   // IsWeapon()——能否放进左肩/右肩，也是"左右手拒绝"的判据
	std::string weaponType; // weapon为true时：在肩膀上激活这件资产时装备的WeaponMod id(比如"weapon_pistol")
	bool usable = false;   // IsUsable()——能否被"使用"(比如汉堡)
	// IsThrowable()——投掷物(手雷/烟雾弹)：按3键激活、瞄准后扔出去，和weapon正交(投掷物可以拿在
	// 手上/放进容器，不能挂肩膀)。
	bool throwable = false;
	// 这件资产在世界里(掉落物/投掷预览)显示的静态网格路径(UE软路径，比如
	// "/Game/Asset/Meshes/Sphere.Sphere")，空=沿用Forever层默认的Cube占位。meshScale是整体
	// 缩放系数(网格自身尺寸不一定合适，比如100cm直径的球体当手雷要缩到0.1)。
	std::string meshPath;
	float meshScale = 1.f;

	// ---- 存档(见Source/Dependence/common/mod_state.md) ----
	// mod私有数据的存档/读档钩子，默认不存任何东西。基类里的公共声明字段由Core直接快照，
	// 这里只放mod自己的私有数据。
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	// SaveState写出数据的格式版本：改了自己SaveState的格式就加一，读档时从
	// StateReader::GetSavedVersion()拿到当时的版本自己迁移。默认0。
	virtual int GetStateVersion() const { return 0; }
	// 这个mod类用来生成唯一实例名的静态计数器——存档保存、读档后恢复，避免读档后新建的实例和
	// 恢复出来的名字撞车。没有计数器的类保持默认(返回-1、Set不做事)。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
