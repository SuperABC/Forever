#pragma once

#include "map/terrain_mod.h"
#include "map/terrain_factory.h"

#include <string>
#include <utility>
#include <functional>


// 一个地形type的三套贴图资产路径(diffuse一定非空，normal/roughness可以留空)。
struct TerrainTexturePaths {
	std::string diffuse;
	std::string normal;
	std::string roughness;
};

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

	// 持有的mod实例——以后需要读mod内部数据的调用方直接用，不需要另外拷贝。
	TerrainMod* GetMod() const;

	float GetPriority() const;
	void SetupTexture() const;
	TerrainTexturePaths GetTextures() const;

	void DistributeTerrain(int width, int height,
		const std::function<std::string(int, int)>& getTerrain,
		const std::function<bool(int, int, std::string)>& setTerrain,
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
