#pragma once

#include "map/terrain_mod.h"
#include "map/terrain_factory.h"

#include <string>
#include <utility>
#include <functional>
#include <cstdint>
#include <vector>


// 一个地形type的三套贴图资产路径(diffuse一定非空，normal/roughness可以留空)。
struct TerrainTexturePaths {
	std::string diffuse;
	std::string normal;
	std::string roughness;
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
	TerrainTexturePaths GetTextures() const;

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

// ---------------------------------------------------------------- 近处地形混合
// 给近处地形材质生成混合控制纹理，由ForeverTerrainFrameworkComponent::RebuildNearControl调用

struct NearBlendParams {
	int texelsPerElement = 16;
	float radius = 1.5f;
	bool constructionGuard = true;
	float constructionRamp = 0.5f;
};

struct NearBlendRegion {
	int originX = 0;
	int originY = 0;
	int size = 0;
	int mapWidth = 0;
	int mapHeight = 0;
	const uint8_t* sliceIds = nullptr;
	const uint8_t* isConstruction = nullptr;
};

struct NearBlendStats {
	int64_t texelCount = 0;
	int64_t fastPathTexels = 0;
	int64_t guardTexels = 0;
	int64_t slowPathTexels = 0;
	int64_t truncatedTexels = 0;
	int64_t distinctIdHistogram[6] = { 0, 0, 0, 0, 0, 0 };
};

// 近处地形的混合权重生成(只依赖标准库)，详见terrain.md"近处地形混合"一节
class NearTerrainBlend {
public:
	explicit NearTerrainBlend(const NearBlendParams& params);

	const NearBlendParams& GetParams() const;

	void BuildRegion(const NearBlendRegion& region, std::vector<uint8_t>& outIds,
		std::vector<uint8_t>& outWeights, NearBlendStats* stats = nullptr, bool useFastPath = true) const;

	void ComputeTexelReference(const NearBlendRegion& region, int texelX, int texelY,
		uint8_t ids[4], uint8_t weights[4]) const;

private:
	float KernelAt(int subX, int subY, int offsetX, int offsetY) const;
	int CellIndex(const NearBlendRegion& region, int x, int y) const;
	bool NeighborhoodUniform(const NearBlendRegion& region, int x, int y) const;
	float ConstructionPull(const NearBlendRegion& region, int x, int y, int subX, int subY, uint8_t& outId) const;
	void ComputeTexel(const NearBlendRegion& region, int x, int y, int subX, int subY,
		uint8_t ids[4], uint8_t weights[4], NearBlendStats* stats) const;

	NearBlendParams params;
	int cellReach;
	int boxSide;
	std::vector<float> kernelTable;
};
