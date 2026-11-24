#pragma once

#include "map/terrain_mod.h"
#include "map/terrain_factory.h"

#include <string>
#include <utility>
#include <functional>
#include <cstdint>
#include <vector>


// 地形在地图界面(MapWidget)里的颜色，由mod在SetupTexture里用SetMapColor指定；has为false表示未指定。
struct TerrainMapColor {
	bool has = false;
	uint8_t r = 0, g = 0, b = 0;
};

// 一个地形类型的外观数据(地图界面颜色+地表材质声明)，由mod在SetupTexture里声明，见Map::GetTerrainAppearances。
struct TerrainAppearance {
	TerrainMapColor mapColor;
	TerrainMaterialSpec material;
};

// 某个TerrainMod实例的薄包装，不持有地图格子数据，详见terrain.md
class Terrain {
public:
	Terrain() = delete;

	Terrain(TerrainFactory* factory, const std::string& terrainId);
	~Terrain();

	std::string GetType() const;
	std::string GetName() const;

	TerrainMod* GetMod() const;

	float GetPriority() const;
	void SetupTexture() const;
	TerrainMapColor GetMapColor() const;
	TerrainMaterialSpec GetMaterialSpec() const;

	void DistributeTerrain(int width, int height,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<bool(int, int, const std::string&)>& setTerrain,
		const std::function<float(int, int)>& getHeight,
		const std::function<bool(int, int, float)>& setHeight,
		const std::function<bool(int, int, bool, float)>& setWater,
		const std::function<bool(int, int, const float*)>& setFineHeights) const;

private:
	TerrainMod* mod;
	TerrainFactory* factory;
	std::string type;
	std::string name;
};
