#pragma once

// 空骨架：Traffic域这次还没有真正迁移（Route/Station/Vehicle三个Mod扩展点已经在
// Dependence/Basic层铺好，业务聚合逻辑留到PHASE4_PLAN.md阶段4-3再点名做）。这次先搭出这个
// 类只是为了让PostImplement（Core/common/implement.h）能拿到7个域的真实指针，见traffic.md。
// 注意：这个类和UForeverTrafficFrameworkComponent（阶段2骨架，UE层Traffic域组件）不是
// 一回事，两者都还是空的，没有互相持有关系。
class Traffic {
public:
	Traffic() = default;
	~Traffic() = default;
};
