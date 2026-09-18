#pragma once

// 注意：这是老工程的"物件/道具/手机"domain（Asset/App/Puzzle三个Mod扩展点），不是UE的
// AForeverPlayerController，两者是完全不同的东西，见PHASE4_PLAN.md"关于player domain改名
// 的说明"一节。
//
// 空骨架：这个域这次还没有真正迁移（Asset/App/Puzzle三个Mod扩展点已经在Dependence/Basic层
// 铺好，业务聚合逻辑留到PHASE4_PLAN.md阶段4-7再点名做）。这次先搭出这个类只是为了让
// PostImplement（Core/common/implement.h）能拿到7个域的真实指针，见player.md。
class Player {
public:
	Player() = default;
	~Player() = default;
};
