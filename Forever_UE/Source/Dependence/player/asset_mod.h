#pragma once

#include "common/mod_state.h"

#include <string>
#include <vector>
// 设计见asset_mod.md。
// 四分类(不动产/载具/独立物体/容器内容物)是同一个mobility字段;"是否为容器"用volume>0表达,与mobility正交。
enum class AssetMobility { Estate, Vehicle, Object, Containee };

class AssetMod {
public:
	AssetMod() = default;
	virtual ~AssetMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 子类在这里填下面的字段(不在构造函数里),Core创建实例后立刻调一次。
	virtual void SetProperty() {}

	AssetMobility mobility = AssetMobility::Object;
	float weight = 0.f;
	float size = 0.f;
	float volume = 0.f;    // >0 即为容器，可以AddContent
	bool backpack = false; // 能否放进"后背"槽(容器类通常为true)
	bool weapon = false;   // IsWeapon()——能否放进左肩/右肩，也是"左右手拒绝"的判据
	std::string weaponType; // weapon为true时：在肩膀上激活这件资产时装备的WeaponMod id(比如"weapon_pistol")
	bool usable = false;   // IsUsable()——能否被"使用"(比如汉堡)
	// 使用时给使用者增加的饱食度(0~1,超出截断);0=不影响。
	float satiety = 0.f;
	// 对应的Industry Product类型(如"product_burger"),商店据此查零售价;空=不能在商店买。与Asset自己的id无代码关联。
	std::string productType;
	// IsThrowable():投掷物(手雷/烟雾弹),与weapon正交。
	bool throwable = false;
	// 世界里(掉落物/投掷预览)显示的静态网格软路径,空=Forever层默认Cube占位;meshScale为整体缩放。
	std::string meshPath;
	float meshScale = 1.f;

	// 资产的Script,两者都空=没有脚本也不挂选项触发盒,见asset_mod.md。
	std::string scriptModName;
	std::vector<std::string> milestoneNames;

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
