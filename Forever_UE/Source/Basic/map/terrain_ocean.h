#pragma once

#include "map/terrain_mod.h"

#include <string>


// 海洋地形:从1-4条随机地图边界向内啃出摆动的海岸线,再从海岸线BFS向内陆传播深度。详见terrain_ocean.md。
class OceanTerrain : public TerrainMod {
public:
	OceanTerrain();
	virtual ~OceanTerrain();

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
