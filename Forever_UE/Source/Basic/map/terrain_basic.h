#pragma once

#include "map/terrain_mod.h"

#include <string>


// 海洋地形:从1-4条随机地图边界向内啃出摆动的海岸线,再从海岸线BFS向内陆传播深度。详见terrain_basic.md。
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
		const std::function<std::string(int, int)>& getTerrain,
		const std::function<bool(int, int, std::string)>& setTerrain,
		const std::function<float(int, int)>& getHeight,
		const std::function<bool(int, int, float)>& setHeight,
		const std::function<bool(int, int, bool, float)>& setWater,
		const std::function<bool(int, int, const float*)>& setFineHeights) const override;

private:
	static int count;
	int id;
	std::string name;
};

// 山区地形:随机游走生成若干条山脊多段线,沿脊线衰减+侧向BFS扩散生成高度。详见terrain_basic.md。
class MountainTerrain : public TerrainMod {
public:
	MountainTerrain();
	virtual ~MountainTerrain();

	static const char* GetId();
	virtual const char* GetType() const override;
	virtual const char* GetName() override;
	virtual float GetPriority() const override;
	virtual void SetupTexture() override;
	virtual void DistributeTerrain(int width, int height,
		const std::function<std::string(int, int)>& getTerrain,
		const std::function<bool(int, int, std::string)>& setTerrain,
		const std::function<float(int, int)>& getHeight,
		const std::function<bool(int, int, float)>& setHeight,
		const std::function<bool(int, int, bool, float)>& setWater,
		const std::function<bool(int, int, const float*)>& setFineHeights) const override;

private:
	static int count;
	int id;
	std::string name;
};

// 河流地形(第一阶段):D8单流向算法只在山体内部结算累积流量,超过阈值的山体格子标记成
// "river",流量流到非山体格子就停止(不继续算,也不标记)。河岸附近的格子会额外写入17x17
// 细化高度数组,让水岸线光滑。详见terrain_basic.md。
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
		const std::function<std::string(int, int)>& getTerrain,
		const std::function<bool(int, int, std::string)>& setTerrain,
		const std::function<float(int, int)>& getHeight,
		const std::function<bool(int, int, float)>& setHeight,
		const std::function<bool(int, int, bool, float)>& setWater,
		const std::function<bool(int, int, const float*)>& setFineHeights) const override;

private:
	static int count;
	int id;
	std::string name;
};
