#pragma once

#include "class.h"

#include "player/asset.h"
#include "player/asset_factory.h"

#include <string>
#include <unordered_map>
#include <vector>


// 注意：这是老工程的"物件/道具/手机"domain（Asset/App/Puzzle三个Mod扩展点），不是UE的
// AForeverPlayerController，两者是完全不同的东西，见PHASE4_PLAN.md"关于player domain改名
// 的说明"一节。
//
// 这次迁移了老工程Player里的"全局时钟"（Time*+Init/Tick/GetTime/SetTime/CrossDay）和
// "资产系统"（leftHand/rightHand/backPack三槽位扩到五槽位+估产载具表+路径寻址，见下方
// "资产系统"一节和player.md）。手机(Phone)、存款等其余字段/方法还没有迁移，留到
// PHASE4_PLAN.md阶段4-7再点名做，见player.md。

class Player {
public:
	Player();
	~Player();

	// 初始化玩家时钟：创建Time，默认设为8点（真正的起始日期由脚本后续SetTime/change_time
	// 决定），照抄老工程Player::Init()里时钟相关的这一小段。
	void Init();

	// 时钟周期更新：按真实帧时长推进游戏时钟。time_flow_ratio默认2.0，可以被主线剧情
	// .script的global_settings.time_flow_ratio字段覆盖（见SetTimeFlowRatio），见
	// player.md"time_flow_ratio"一节。
	// @delta: 本帧时长（秒）
	void Tick(float delta);

	// 覆盖time_flow_ratio——由AForeverFrameworkActor::EnsurePlayerGenerated()读取主线
	// 剧情.script的global_settings.time_flow_ratio字段后调用，没有声明则保持默认值2.0。
	void SetTimeFlowRatio(double ratio);

	// 获取游戏时钟
	Time* GetTime() const;

	// 直接将游戏时钟设为绝对时间（脚本change_time瞬间跳变用）；会记录跳变前的天数，
	// 供CrossDay正确检测跨天。
	// @newTime: 目标绝对时间
	void SetTime(const Time& newTime);

	// 是否发生跨天（相对上一次Tick/SetTime时的天数）
	bool CrossDay();

	// 生命值——进入武器系统时新增。这次只做"扣血/回血/是否死亡"这个最小闭环，不做死亡后果
	// (复活/game over等)，见Source/Forever/Player/ForeverWeaponComponent.md。伤害/治疗量
	// 传负数没有意义，调用方保证传正数，这里不做防御性纠正。
	float GetHealth() const;
	float GetMaxHealth() const;
	// @amount: 扣除的生命值(正数)，返回值是扣除后的剩余生命值(已clamp到[0, maxHealth])
	float TakeDamage(float amount);
	// @amount: 恢复的生命值(正数)，返回值是恢复后的生命值(已clamp到[0, maxHealth])
	float Heal(float amount);
	bool IsDead() const;

	// ---- 资产系统 ----
	// 创建一个Asset实例，id需要在config.json的"asset_mods"数组里启用，否则返回nullptr。
	// 独占持有权转交给调用方（调用方负责走AddByPath/AddContent等接口把它放进某个槽位/
	// 容器/房间，否则要自己delete）。
	Asset* CreateAsset(const std::string& id, const std::string& name);
	// 调用方确认这个Asset已经不在任何槽位/容器/房间表里之后调用——内部走
	// factory->DestroyAsset(mod)+delete this，不能跳过这个方法直接delete（见
	// AssetFactory跨DLL new/delete安全的约定）。
	void DestroyAsset(Asset* asset);

	static std::vector<std::string> SplitPath(const std::string& path);
	// 按路径查询/摘除/写入——"left"/"right"/"back"/"leftShoulder"/"rightShoulder"是五个
	// 槽位，"room"是当前房间，多段路径(比如"back/PistolAmmo")按'/'切分后先解析到容器
	// 再进GetContents()查。
	Asset* GetByPath(const std::string& path) const;
	Asset* RemoveByPath(const std::string& path); // 从槽位/容器/房间摘除，不delete
	// 放入规则：
	// - left/right(手)：要求mobility==Object且当前槽位为空，且asset->IsWeapon()==false
	//   （武器不能拿在手上，只能挂肩膀）。
	// - back：同上，额外要求asset->GetBackpack()==true。
	// - leftShoulder/rightShoulder：要求asset->IsWeapon()==true且当前槽位为空（只能放
	//   武器）。
	// - room：要求GetCurrentRoom()!=nullptr，转发给currentRoom->AddAsset(asset)。
	// - 多段路径：GetByPath找到容器，container->AddContent(asset)，无mobility限制。
	bool AddByPath(const std::string& path, Asset* asset);

	Asset* GetLeftHand() const { return leftHand; }
	Asset* GetRightHand() const { return rightHand; }
	Asset* GetBackPack() const { return backPack; }
	Asset* GetLeftShoulder() const { return leftShoulder; }
	Asset* GetRightShoulder() const { return rightShoulder; }

	// 不动产/载具——不走五槽位，mobility必须是Estate/Vehicle。
	bool AddEstateAsset(Asset* asset);
	Asset* GetEstateAsset(const std::string& name) const;
	void RemoveEstateAsset(const std::string& name);

	// 真实物理位置(不是剧情/场景语义)，由BuildingElement的Room重叠检测回调维护，见
	// Source/Forever/Element/BuildingElement.cpp的OnRoomOverlapBegin/End。
	Room* GetCurrentRoom() const { return currentRoom; }
	void SetCurrentRoom(Room* room) { currentRoom = room; }

	// 转发到backPack(不存在返回0/false)——供ForeverWeaponComponent换弹时查/扣备弹。
	int CountByType(const std::string& type) const;
	bool ConsumeByType(const std::string& type, int amount);

private:
	// 游戏时钟（持有所有权）
	Time* time = nullptr;

	// 上一次Tick/SetTime时的游戏天数，用于跨天检测
	int day = -1;

	// 默认值和原来硬编码的kTimeFlowRatio一致，未被脚本覆盖时行为不变。
	double timeFlowRatio = 2.0;

	// 进入武器系统新增。100点是随手给的默认值，具体数值等PIE里试手感再调。
	float health = 100.f;
	float maxHealth = 100.f;

	// ---- 资产系统 ----
	AssetFactory& assetFactory;

	Asset* leftHand = nullptr;
	Asset* rightHand = nullptr;
	Asset* backPack = nullptr;
	Asset* leftShoulder = nullptr;
	Asset* rightShoulder = nullptr;

	// 不动产/载具，key=name，mobility校验在AddEstateAsset里做。
	std::unordered_map<std::string, Asset*> estateAssets;

	Room* currentRoom = nullptr;
};
