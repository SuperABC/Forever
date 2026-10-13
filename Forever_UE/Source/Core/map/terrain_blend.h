#pragma once

#include <cstdint>
#include <vector>


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

// 近处地形的混合权重生成：纯C++，只依赖标准库。详见terrain_blend.md。
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
