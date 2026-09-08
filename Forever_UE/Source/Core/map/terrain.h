#pragma once

#include "map/terrain_mod.h"
#include "map/terrain_factory.h"

#include <string>
#include <utility>
#include <functional>

// Terrain实体:持有一个具体TerrainMod实例(由TerrainFactory创建/销毁),把它的业务方法
// 转发出来。自身不持有任何地图格子数据——格子数据(地形类型/高度/水面/hatch)在Map/Element
// 里,详见 Source/Core/map/terrain.md、map.md。
class Terrain {
public:
	Terrain() = delete;

	// @factory: 地形工厂; @terrainId: 地形静态类型标识(工厂里已注册的id)
	Terrain(TerrainFactory* factory, const std::string& terrainId);
	~Terrain();

	std::string GetType() const;
	std::string GetName() const;
	float GetPriority() const;
	void SetupTexture() const;
	std::string GetTexture() const;
	std::pair<bool, float> GetWater() const;

	void DistributeTerrain(int width, int height,
		const std::function<std::string(int, int)>& getTerrain,
		const std::function<bool(int, int, std::string)>& setTerrain,
		const std::function<float(int, int)>& getHeight,
		const std::function<bool(int, int, float)>& setHeight) const;

private:
	TerrainMod* mod;
	TerrainFactory* factory;
	std::string type;
	std::string name;
};
