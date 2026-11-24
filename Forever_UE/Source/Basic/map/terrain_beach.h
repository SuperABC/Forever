#pragma once

#include "map/terrain_mod.h"

#include <string>


// 沙滩地形:只登记地形类型和地表材质(干沙到湿沙按水线渐变)，不自动生成——沙滩格子由地图编辑器手绘(terrain_file读入)。详见terrain_beach.md。
class BeachTerrain : public TerrainMod {
public:
	FOREVER_MOD_GENERIC_UNIT_TEST("terrain")
	explicit BeachTerrain(const std::string& args = "");
	virtual ~BeachTerrain();

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
	// 存档保存/恢复命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
