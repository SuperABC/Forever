#pragma once

#include "map/terrain_mod.h"

#include <string>


// 河流地形:山体内D8流量累积确定起点,流出山体后按角度/角速度模型继续蔓延,流到地图边界/海/并入已有河
// 才保留;加宽后降河床、铺水面,河岸附近格子写入17x17细化高度数组让水岸线光滑。详见terrain_river.md。
class RiverTerrain : public TerrainMod {
public:
	RiverTerrain();
	virtual ~RiverTerrain();

	static const char* GetId();
	virtual const char* GetType() const override;
	virtual const char* GetName() override;
	virtual float GetPriority() const override;
	virtual void SetupTexture() override;
	virtual void DistributeTerrain(int width, int height,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<bool(int, int, const std::string&)>& setTerrain,
		const std::function<float(int, int)>& getHeight,
		const std::function<bool(int, int, float)>& setHeight,
		const std::function<bool(int, int, bool, float)>& setWater,
		const std::function<bool(int, int, const float*)>& setFineHeights) const override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
