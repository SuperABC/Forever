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

	// 时钟周期更新：按真实帧时长推进游戏时钟。time_flow_ratio这次写死1.0（1真实秒=1游戏
	// 分钟），不依赖Story，见player.md"time_flow_ratio"一节。
	// @delta: 本帧时长（秒）
	void Tick(float delta);

	// 获取游戏时钟
	Time* GetTime() const;

	// 直接将游戏时钟设为绝对时间（脚本change_time瞬间跳变用）；会记录跳变前的天数，
	// 供CrossDay正确检测跨天。
	// @newTime: 目标绝对时间
	void SetTime(const Time& newTime);

	// 是否发生跨天（相对上一次Tick/SetTime时的天数）
	bool CrossDay();

private:
	// 游戏时钟（持有所有权）
	Time* time = nullptr;

	// 上一次Tick/SetTime时的游戏天数，用于跨天检测
	int day = -1;
};
