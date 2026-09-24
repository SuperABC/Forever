#pragma once

#include "class.h"


// 注意：这是老工程的"物件/道具/手机"domain（Asset/App/Puzzle三个Mod扩展点），不是UE的
// AForeverPlayerController，两者是完全不同的东西，见PHASE4_PLAN.md"关于player domain改名
// 的说明"一节。
//
// 这次只迁移了老工程Player里的"全局时钟"这一小块（Time*+Init/Tick/GetTime/SetTime/
// CrossDay），手机(Phone)、资产(Asset)、存款等其余字段/方法还没有迁移，留到
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
};
