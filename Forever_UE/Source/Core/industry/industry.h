#pragma once

#include "common/class.h"
#include "common/handle.h"

struct ScriptContext;

// 空骨架：Industry域这次还没有真正迁移（Product/Storage/Manufacture三个Mod扩展点已经在
// Dependence/Basic层铺好，业务聚合逻辑留到PHASE4_PLAN.md阶段4-5再点名做）。这次先搭出这个
// 类只是为了让PostImplement（Core/common/implement.h）能拿到7个域的真实指针，见industry.md。
class Industry {
public:
	Industry() = default;
	~Industry() = default;

	// 阶段占位：Industry域这次还没有真正迁移，空实现——和Map/Populace/Society/Traffic/Story
	// 一起被AForeverFrameworkActor::Tick统一调用一遍，保持"每个域都有Tick"这个形状一致，等
	// Industry域真正落地时再补内容。
	void Tick(const Time& currentTime, bool crossedDay, PostHandle* post);

	// 阶段占位：目前没有任何Change子类是Industry域自己认识、需要处理的，空实现——
	// AForeverFrameworkActor::ApplyChange会把同一个Change转发给全部六个域，这里不打"未实现"
	// 警告（避免同一个Change被六个域各打一遍重复警告），唯一的兜底警告在Story::ApplyChange。
	void ApplyChange(const Change* change, const ScriptContext& context);
};
