#pragma once


// 空骨架：Society域这次还没有真正迁移（Job/Organization两个Mod扩展点已经在
// Dependence/Basic层铺好，业务聚合逻辑留到PHASE4_PLAN.md阶段4-4再点名做）。这次先搭出这个
// 类只是为了让PostImplement（Core/common/implement.h）能拿到7个域的真实指针，见society.md。
class Society {
public:
	Society() = default;
	~Society() = default;
};
