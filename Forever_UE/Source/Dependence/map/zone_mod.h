#pragma once

#include <string>
#include <vector>

#include "map/geometry.h"

// ZoneMod：Zone这次只有"显式指定矩形"一种生成方式(不参与权重/CDF随机填充，见
// Source/Core/map/map.md"InitZones"一节)。
class ZoneMod {
public:
	ZoneMod() = default;
	virtual ~ZoneMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// config.json里"zone_mods"数组中该mod id后面的命令行式参数字符串(如
	// "pengzhan --density 1.0"里的"--density 1.0"),由<Concept>Factory::Create<Concept>
	// 创建实例后立刻调用一次。默认空实现,不需要参数的mod不用重写,格式解析完全由重写者
	// 自己决定。
	virtual void ApplyArgs(const std::string& args) {}

	// Distribute()里push显式占位请求，引擎读取后逐条调用对应lot->RequestPlacement(...)。
	std::vector<LotPlacementRequest> explicitPlacements;

	// 引擎按当前全图lot列表（剩余空闲面积降序，用Lot::GetFreeAcreage()排序）调用一次。mod
	// 在其中对自己想要的lot直接push一条explicitPlacements请求（自己决定direction/margin/
	// depth，也就是自己决定这块Zone的尺寸），引擎逐条尝试裁剪，不额外做权重/随机面积填充。
	virtual void Distribute(const std::vector<Lot*>& lots) = 0;
};
