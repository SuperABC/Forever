#pragma once


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
	bool usable = false;   // IsUsable()——能否被"使用"(比如汉堡)
};
