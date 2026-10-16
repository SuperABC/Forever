#include "terrain.h"

#include "common/error.h"

#include <algorithm>
#include <cmath>
#include <map>


using namespace std;

Terrain::Terrain(TerrainFactory* factory, const string& terrainId) :
	mod(factory->CreateTerrain(terrainId)),
	factory(factory),
	type(),
	name() {
	if (!mod) {
		THROW_EXCEPTION(NullPointerException, "Terrain " + terrainId + " mod is null.\n");
	}

	type = mod->GetType();
	name = mod->GetName();
}

Terrain::~Terrain() {
	factory->DestroyTerrain(mod);
}

string Terrain::GetType() const {
	return type;
}

string Terrain::GetName() const {
	return name;
}

TerrainMod* Terrain::GetMod() const {
	return mod;
}

float Terrain::GetPriority() const {
	return mod->GetPriority();
}

void Terrain::SetupTexture() const {
	mod->SetupTexture();
}

TerrainTexturePaths Terrain::GetTextures() const {
	return { mod->diffusePath, mod->normalPath, mod->roughnessPath };
}

void Terrain::DistributeTerrain(int width, int height,
	const function<string(int, int)>& getTerrain, const function<bool(int, int, string)>& setTerrain,
	const function<float(int, int)>& getHeight, const function<bool(int, int, float)>& setHeight,
	const function<bool(int, int, bool, float)>& setWater,
	const function<bool(int, int, const float*)>& setFineHeights) const {
	mod->DistributeTerrain(width, height, getTerrain, setTerrain, getHeight, setHeight, setWater, setFineHeights);
}

// ---------------------------------------------------------------- 近处地形混合(NearTerrainBlend)

namespace {
	constexpr int MAX_DISTINCT = 64;

	void WriteOneHot(uint8_t id, uint8_t ids[4], uint8_t weights[4]) {
		for (int i = 0; i < 4; i++) {
			ids[i] = id;
			weights[i] = (i == 0) ? 255 : 0;
		}
	}

	// 把权重向construction的纯槽位拉s：(1-s)*原权重 + s*onehot(id)。
	void ApplyPull(uint8_t* ids, float* weights, int& n, float s, uint8_t pullId) {
		int found = -1;
		for (int i = 0; i < n; i++) {
			weights[i] *= (1.f - s);
			if (ids[i] == pullId) found = i;
		}
		if (found < 0) {
			ids[n] = pullId;
			weights[n] = 0.f;
			found = n;
			n++;
		}
		weights[found] += s;
	}

	// ids/weights是半径内按地形id累加后的结果(权重和约等于1，条目数n)，整理成最终的4个槽位。
	// 返回是否发生了前四大截断。
	bool FinalizeTexel(uint8_t* ids, float* weights, int n, uint8_t outIds[4], uint8_t outWeights[4]) {
		// 权重降序，权重相同按id升序，保证结果与输入顺序无关。
		for (int i = 1; i < n; i++) {
			uint8_t id = ids[i];
			float w = weights[i];
			int j = i - 1;
			while (j >= 0 && (weights[j] < w || (weights[j] == w && ids[j] > id))) {
				ids[j + 1] = ids[j];
				weights[j + 1] = weights[j];
				j--;
			}
			ids[j + 1] = id;
			weights[j + 1] = w;
		}

		bool truncated = n > 4;
		int m = min(n, 4);
		float kept[4] = { 0.f, 0.f, 0.f, 0.f };
		for (int i = 0; i < m; i++) kept[i] = weights[i];

		// 减去第5大的权重再归一化：一个id从第4名掉到第5名时它的权重正好连续降到0。
		if (truncated) {
			float fifth = weights[4];
			float shifted[4];
			float shiftedSum = 0.f;
			for (int i = 0; i < 4; i++) {
				shifted[i] = max(weights[i] - fifth, 0.f);
				shiftedSum += shifted[i];
			}
			if (shiftedSum > 1e-6f) {
				for (int i = 0; i < 4; i++) kept[i] = shifted[i];
			}
		}

		float sum = 0.f;
		for (int i = 0; i < m; i++) sum += kept[i];
		for (int i = 0; i < m; i++) kept[i] /= sum;

		// 最大余数法量化，保证和恰好是255。
		int q[4] = { 0, 0, 0, 0 };
		float frac[4] = { -1.f, -1.f, -1.f, -1.f };
		int total = 0;
		for (int i = 0; i < m; i++) {
			float v = kept[i] * 255.f;
			q[i] = static_cast<int>(floorf(v));
			frac[i] = v - static_cast<float>(q[i]);
			total += q[i];
		}
		int remaining = 255 - total;
		while (remaining > 0) {
			int best = 0;
			for (int i = 1; i < m; i++) {
				if (frac[i] > frac[best]) best = i;
			}
			q[best]++;
			frac[best] -= 1.f;
			remaining--;
		}
		while (remaining < 0) {
			int best = 0;
			for (int i = 1; i < m; i++) {
				if (q[i] > q[best]) best = i;
			}
			q[best]--;
			remaining++;
		}

		for (int i = 0; i < 4; i++) {
			if (i < m) {
				outIds[i] = ids[i];
				outWeights[i] = static_cast<uint8_t>(q[i]);
			}
			else {
				outIds[i] = ids[0];
				outWeights[i] = 0;
			}
		}
		return truncated;
	}
}

NearTerrainBlend::NearTerrainBlend(const NearBlendParams& inParams) : params(inParams) {
	params.texelsPerElement = max(params.texelsPerElement, 1);
	params.radius = min(max(params.radius, 1.f), 3.f);
	params.constructionRamp = max(params.constructionRamp, 0.f);

	cellReach = static_cast<int>(ceilf(params.radius + 0.5f)) - 1;
	boxSide = 2 * cellReach + 1;

	const int tpe = params.texelsPerElement;
	kernelTable.assign(static_cast<size_t>(tpe) * tpe * boxSide * boxSide, 0.f);
	for (int sy = 0; sy < tpe; sy++) {
		for (int sx = 0; sx < tpe; sx++) {
			float* row = &kernelTable[static_cast<size_t>(sy * tpe + sx) * boxSide * boxSide];
			for (int oy = -cellReach; oy <= cellReach; oy++) {
				for (int ox = -cellReach; ox <= cellReach; ox++) {
					row[(oy + cellReach) * boxSide + (ox + cellReach)] = KernelAt(sx, sy, ox, oy);
				}
			}
		}
	}
}

const NearBlendParams& NearTerrainBlend::GetParams() const {
	return params;
}

float NearTerrainBlend::KernelAt(int subX, int subY, int offsetX, int offsetY) const {
	const float tpe = static_cast<float>(params.texelsPerElement);
	float ux = (static_cast<float>(subX) + 0.5f) / tpe;
	float uy = (static_cast<float>(subY) + 0.5f) / tpe;
	float dx = ux - (static_cast<float>(offsetX) + 0.5f);
	float dy = uy - (static_cast<float>(offsetY) + 0.5f);
	float t = sqrtf(dx * dx + dy * dy) / params.radius;
	if (t >= 1.f) return 0.f;
	float a = 1.f - t * t;
	return a * a;
}

int NearTerrainBlend::CellIndex(const NearBlendRegion& region, int x, int y) const {
	x = min(max(x, 0), region.mapWidth - 1);
	y = min(max(y, 0), region.mapHeight - 1);
	return y * region.mapWidth + x;
}

bool NearTerrainBlend::NeighborhoodUniform(const NearBlendRegion& region, int x, int y) const {
	uint8_t id = region.sliceIds[CellIndex(region, x, y)];
	for (int dy = -cellReach; dy <= cellReach; dy++) {
		for (int dx = -cellReach; dx <= cellReach; dx++) {
			if (region.sliceIds[CellIndex(region, x + dx, y + dy)] != id) return false;
		}
	}
	return true;
}

float NearTerrainBlend::ConstructionPull(const NearBlendRegion& region, int x, int y, int subX, int subY, uint8_t& outId) const {
	// 半径超过1.5时核会越过construction外的第二圈，构造出来的construction边界上会有不连续；
	// 用一段随到construction距离线性升到1的拉力把权重拉向construction自己的槽位，保证边界上正好纯净。
	if (!params.constructionGuard || params.constructionRamp <= 0.f || params.radius <= 1.5f) return 0.f;

	const float tpe = static_cast<float>(params.texelsPerElement);
	float ux = (static_cast<float>(subX) + 0.5f) / tpe;
	float uy = (static_cast<float>(subY) + 0.5f) / tpe;
	float best = 0.f;
	for (int dy = -cellReach; dy <= cellReach; dy++) {
		for (int dx = -cellReach; dx <= cellReach; dx++) {
			int cell = CellIndex(region, x + dx, y + dy);
			if (!region.isConstruction[cell]) continue;
			float gapX = max(max(static_cast<float>(dx) - ux, ux - static_cast<float>(dx + 1)), 0.f);
			float gapY = max(max(static_cast<float>(dy) - uy, uy - static_cast<float>(dy + 1)), 0.f);
			float pull = 1.f - sqrtf(gapX * gapX + gapY * gapY) / params.constructionRamp;
			if (pull > best) {
				best = pull;
				outId = region.sliceIds[cell];
			}
		}
	}
	return best;
}

void NearTerrainBlend::ComputeTexel(const NearBlendRegion& region, int x, int y, int subX, int subY,
	uint8_t ids[4], uint8_t weights[4], NearBlendStats* stats) const {
	int own = CellIndex(region, x, y);
	if (params.constructionGuard && region.isConstruction[own]) {
		WriteOneHot(region.sliceIds[own], ids, weights);
		if (stats) stats->guardTexels++;
		return;
	}

	const float* kernel = &kernelTable[static_cast<size_t>(subY * params.texelsPerElement + subX) * boxSide * boxSide];
	uint8_t accIds[MAX_DISTINCT];
	float accWeights[MAX_DISTINCT];
	int n = 0;
	float total = 0.f;
	for (int dy = -cellReach; dy <= cellReach; dy++) {
		for (int dx = -cellReach; dx <= cellReach; dx++) {
			float w = kernel[(dy + cellReach) * boxSide + (dx + cellReach)];
			if (w <= 0.f) continue;
			uint8_t id = region.sliceIds[CellIndex(region, x + dx, y + dy)];
			int k = 0;
			while (k < n && accIds[k] != id) k++;
			if (k == n) {
				accIds[n] = id;
				accWeights[n] = 0.f;
				n++;
			}
			accWeights[k] += w;
			total += w;
		}
	}
	for (int k = 0; k < n; k++) accWeights[k] /= total;

	uint8_t pullId = 0;
	float pull = ConstructionPull(region, x, y, subX, subY, pullId);
	if (pull > 0.f) ApplyPull(accIds, accWeights, n, pull, pullId);

	bool truncated = FinalizeTexel(accIds, accWeights, n, ids, weights);
	if (stats) {
		stats->slowPathTexels++;
		if (truncated) stats->truncatedTexels++;
		stats->distinctIdHistogram[min(n, 5)]++;
	}
}

void NearTerrainBlend::BuildRegion(const NearBlendRegion& region, vector<uint8_t>& outIds,
	vector<uint8_t>& outWeights, NearBlendStats* stats, bool useFastPath) const {
	const int tpe = params.texelsPerElement;
	const int res = region.size * tpe;
	outIds.assign(static_cast<size_t>(res) * res * 4, 0);
	outWeights.assign(static_cast<size_t>(res) * res * 4, 0);

	for (int ey = 0; ey < region.size; ey++) {
		for (int ex = 0; ex < region.size; ex++) {
			int gx = region.originX + ex;
			int gy = region.originY + ey;
			int own = CellIndex(region, gx, gy);

			bool guardHit = params.constructionGuard && region.isConstruction[own];
			bool uniform = false;
			if (useFastPath) uniform = guardHit || NeighborhoodUniform(region, gx, gy);
			uint8_t ownId = region.sliceIds[own];

			for (int sy = 0; sy < tpe; sy++) {
				for (int sx = 0; sx < tpe; sx++) {
					int tx = ex * tpe + sx;
					int ty = ey * tpe + sy;
					size_t base = (static_cast<size_t>(ty) * res + tx) * 4;
					if (uniform) {
						WriteOneHot(ownId, &outIds[base], &outWeights[base]);
						if (stats) (guardHit ? stats->guardTexels : stats->fastPathTexels)++;
					}
					else {
						ComputeTexel(region, gx, gy, sx, sy, &outIds[base], &outWeights[base], stats);
					}
					if (stats) stats->texelCount++;
				}
			}
		}
	}
}

void NearTerrainBlend::ComputeTexelReference(const NearBlendRegion& region, int texelX, int texelY,
	uint8_t ids[4], uint8_t weights[4]) const {
	const int tpe = params.texelsPerElement;
	int gx = region.originX + texelX / tpe;
	int gy = region.originY + texelY / tpe;
	int sx = texelX % tpe;
	int sy = texelY % tpe;

	int own = CellIndex(region, gx, gy);
	if (params.constructionGuard && region.isConstruction[own]) {
		WriteOneHot(region.sliceIds[own], ids, weights);
		return;
	}

	map<uint8_t, float> acc;
	float total = 0.f;
	for (int dy = -cellReach; dy <= cellReach; dy++) {
		for (int dx = -cellReach; dx <= cellReach; dx++) {
			float w = KernelAt(sx, sy, dx, dy);
			if (w <= 0.f) continue;
			acc[region.sliceIds[CellIndex(region, gx + dx, gy + dy)]] += w;
			total += w;
		}
	}

	uint8_t accIds[MAX_DISTINCT];
	float accWeights[MAX_DISTINCT];
	int n = 0;
	for (auto& entry : acc) {
		accIds[n] = entry.first;
		accWeights[n] = entry.second / total;
		n++;
	}
	uint8_t pullId = 0;
	float pull = ConstructionPull(region, gx, gy, sx, sy, pullId);
	if (pull > 0.f) ApplyPull(accIds, accWeights, n, pull, pullId);
	FinalizeTexel(accIds, accWeights, n, ids, weights);
}
