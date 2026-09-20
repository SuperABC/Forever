#pragma once


// 空骨架：Industry域这次还没有真正迁移（Product/Storage/Manufacture三个Mod扩展点已经在
// Dependence/Basic层铺好，业务聚合逻辑留到PHASE4_PLAN.md阶段4-5再点名做）。这次先搭出这个
// 类只是为了让PostImplement（Core/common/implement.h）能拿到7个域的真实指针，见industry.md。
class Industry {
public:
	Industry() = default;
	~Industry() = default;
};
