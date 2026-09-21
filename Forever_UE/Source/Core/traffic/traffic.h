#pragma once

#include "common/class.h"
#include "common/handle.h"

struct ScriptContext;

// 空骨架：Traffic域这次还没有真正迁移（Route/Station/Vehicle三个Mod扩展点已经在
// Dependence/Basic层铺好，业务聚合逻辑留到PHASE4_PLAN.md阶段4-3再点名做）。这次先搭出这个
// 类只是为了让PostImplement（Core/common/implement.h）能拿到7个域的真实指针，见traffic.md。
// 注意：这个类和UForeverTrafficFrameworkComponent（阶段2骨架，UE层Traffic域组件）不是
// 一回事，两者都还是空的，没有互相持有关系。
class Traffic {
public:
	Traffic() = default;
	~Traffic() = default;

	// 阶段占位：Traffic域这次还没有真正迁移，空实现——和Map/Populace/Society/Industry/Story
	// 一起被AForeverFrameworkActor::Tick统一调用一遍，保持"每个域都有Tick"这个形状一致，等
	// Traffic域真正落地时再补内容。
	void Tick(const Time& currentTime, bool crossedDay, PostHandle* post);

	// 阶段占位：目前没有任何Change子类是Traffic域自己认识、需要处理的，空实现——
	// AForeverFrameworkActor::ApplyChange会把同一个Change转发给全部六个域，这里不打"未实现"
	// 警告（避免同一个Change被六个域各打一遍重复警告），唯一的兜底警告在Story::ApplyChange。
	void ApplyChange(const Change* change, const ScriptContext& context);
};
