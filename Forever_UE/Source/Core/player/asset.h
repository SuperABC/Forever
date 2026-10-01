#pragma once

#include "player/asset_mod.h"
#include "player/asset_factory.h"

#include <string>
#include <unordered_map>


// Asset：物品/资产的实例——照抄Storage的"factory+mod+自己的可变状态"模式，不是照抄老工程
// "类型定义和某个背包里的一份库存混一起"的Asset。AssetMod是类型定义(照抄一份到自己的字段
// 里，不持有mod的可变状态)，count/contents这些"这一份具体是多少/装着什么"是Asset自己的。
//
// 堆叠：count代表"这一份的数量"，子弹120发是1个Asset实例(count=120)，不是120个实例，
// 见asset.md"堆叠"一节。
//
// 容器：volume>0即为容器，可以AddContent，一个容器本身还能被装进另一个容器(没有限制)。
class Asset {
public:
	// id需要在config.json的"asset_mods"里启用，否则mod为空、IsValid()返回false。
	Asset(AssetFactory* factory, const std::string& id, const std::string& name);
	~Asset(); // 递归delete contents

	bool IsValid() const;

	const std::string& GetName() const;
	const std::string& GetType() const; // = id，供WeaponFactory/Industry按同一个id反查

	AssetMobility GetMobility() const;
	float GetWeight() const;
	float GetSize() const;
	float GetVolume() const;
	bool GetBackpack() const;
	bool IsWeapon() const;
	bool IsUsable() const;
	bool IsContainer() const; // volume > 0

	int GetCount() const;
	void SetCount(int value);

	// 使用：count-1，count降到0时返回true(调用方负责从所在容器/槽位/房间移除+delete这个
	// Asset)。只有IsUsable()==true时生效，否则直接返回false、不改动count。
	bool Use();

	// 容器行为(仅IsContainer()==true时有意义，否则GetSpace()恒为0、AddContent恒失败)：
	float GetSpace() const; // volume - Σ(content->size * content->count)
	bool AddContent(Asset* content);                // 空间不够返回false，成功后独占持有
	Asset* RemoveContent(const std::string& name);   // 找不到返回nullptr，摘除不delete
	const std::unordered_map<std::string, Asset*>& GetContents() const;

	// 递归按type汇总/扣减数量(含自己直接持有的这一份+所有子容器)——供Player::CountByType/
	// ConsumeByType转发，也供UI显示汇总。
	int CountByType(const std::string& type) const;
	// 两阶段实现：先只读确认总量够不够，够了再真正扣减；不够返回false且不改动任何状态。
	bool ConsumeByType(const std::string& type, int amount);

private:
	// 递归消耗up to amount份type，返回实际消耗掉的数量(可能小于amount，调用方
	// (ConsumeByType)已经用CountByType提前确认过整体够不够，正常情况下总能凑够amount)。
	int ConsumeUpTo(const std::string& type, int amount);

	AssetFactory* factory;
	AssetMod* mod;
	std::string name;
	std::string type;

	AssetMobility mobility;
	float weight = 0.f;
	float size = 0.f;
	float volume = 0.f;
	bool backpack = false;
	bool weaponFlag = false;
	bool usableFlag = false;

	int count = 1;
	std::unordered_map<std::string, Asset*> contents; // 独占持有
};
