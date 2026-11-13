#include "terrain_ocean.h"

#include "common/utility.h"

#include <vector>
#include <cmath>
#include <algorithm>
#include <tuple>
#include <unordered_set>
#include <string>

// 海平面水位(地图元素单位)：Ocean/River共用一个常量，分开写会让河口出现河水/海水的落差。
#define SEA_LEVEL -0.1f

using namespace std;

// 八方向邻居偏移量——Ocean/Mountain/River三个地形都要用，同样放在文件顶部。
static const int neighborOffsets[8][2] = { {1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1} };

// ============================================================================
// OceanTerrain：从1-4条随机地图边界向内啃出摆动的海岸线，再从海岸线BFS向内陆传播深度。
// 详见terrain_ocean.md"OceanTerrain"一节。
// ============================================================================

// 海洋深度衰减参数(沿离开地图中心方向,离海岸线越远越深)
#define OCEAN_DEPTH_RATE 0.05f
#define OCEAN_SLOPE_JITTER 0.01f
#define OCEAN_MAX_DEPTH 60.0f

int OceanTerrain::count = 0;

OceanTerrain::OceanTerrain() : id(count++) {

}

OceanTerrain::~OceanTerrain() {

}

const char* OceanTerrain::GetId() {
	return "terrain_ocean";
}

const char* OceanTerrain::GetType() const {
	return "terrain_ocean";
}

const char* OceanTerrain::GetName() {
	name = "海洋地形" + to_string(id);
	return name.data();
}

float OceanTerrain::GetPriority() const {
	// 必须低于MountainTerrain(0.95)：海后跑，海岸线雕刻绕开已生成的山体(见terrain_ocean.md)。
	return 0.9f;
}

void OceanTerrain::SetupTexture() {
	diffusePath = "/Game/Asset/Terrain/Plain/PlainDiffuse.PlainDiffuse";
	normalPath = "/Game/Asset/Terrain/Plain/PlainNormal.PlainNormal";
	roughnessPath = "/Game/Asset/Terrain/Plain/PlainRoughness.PlainRoughness";
}

void OceanTerrain::DistributeTerrain(int width, int height,
	const function<const string&(int, int)>& getTerrain, const function<bool(int, int, const string&)>& setTerrain,
	const function<float(int, int)>& getHeight, const function<bool(int, int, float)>& setHeight,
	const function<bool(int, int, bool, float)>& setWater,
	const function<bool(int, int, const float*)>& setFineHeights) const {

	auto randf = [](float lo, float hi) -> float {
		return lo + static_cast<float>(GetRandom(100000)) / 99999.0f * (hi - lo);
	};
	auto clampf = [](float value, float lo, float hi) -> float {
		if (!(value > lo)) return lo;
		return value > hi ? hi : value;
	};

	float mapCenterX = width / 2.0f;
	float mapCenterY = height / 2.0f;

	// 随机决定哪些方向临海,至少保证一面
	int coastMask = 0;
	while (coastMask == 0)
		coastMask = GetRandom(16);

	// 仅先标记海洋范围(地形类型),高度留给后续BFS统一计算
	vector<vector<bool>> isOcean(width, vector<bool>(height, false));

	// 西面海岸
	if (coastMask & 1) {
		int coastDepth = width / 16 + GetRandom(width / 16);
		float coastOffset = 0.0f, coastSlope = 0.0f;
		debugf("Log: Sea in the west %d.\n", coastDepth);
		for (int y = 0; y < height; y++) {
			int coastLimit = coastDepth + static_cast<int>(coastOffset);
			if (coastLimit > width)
				coastLimit = width;
			for (int x = 0; x < coastLimit; x++) {
				// 已经是山的格子不雕刻成海；isOcean保持false，对后面的海岸线BFS种子判定等同普通陆地。
				if (getTerrain(x, y) == "terrain_mountain") continue;
				setTerrain(x, y, GetType());
				isOcean[x][y] = true;
			}
			coastSlope += float(GetRandom(33) - 16) / 256.0f;
			coastOffset += coastSlope;
			if (coastDepth + coastOffset <= width / 64) {
				coastSlope = 0.0f;
				coastOffset = width / 64 - static_cast<float>(coastDepth);
			}
			if (coastDepth + coastOffset >= width / 8) {
				coastSlope = 0.0f;
				coastOffset = width / 8 - static_cast<float>(coastDepth);
			}
		}
	}

	// 东面海岸
	if (coastMask & 2) {
		int coastDepth = width / 16 + GetRandom(width / 16);
		float coastOffset = 0.0f, coastSlope = 0.0f;
		debugf("Log: Sea in the east %d.\n", coastDepth);
		for (int y = 0; y < height; y++) {
			int coastStart = width - coastDepth - static_cast<int>(coastOffset);
			if (coastStart < 0)
				coastStart = 0;
			for (int x = width - 1; x >= coastStart; x--) {
				if (getTerrain(x, y) == "terrain_mountain") continue; // 绕开已有山体,见西面海岸那段注释
				setTerrain(x, y, GetType());
				isOcean[x][y] = true;
			}
			coastSlope += float(GetRandom(33) - 16) / 256.0f;
			coastOffset += coastSlope;
			if (coastDepth + coastOffset <= width / 64) {
				coastSlope = 0.0f;
				coastOffset = width / 64.f - coastDepth;
			}
			if (coastDepth + coastOffset >= width / 8) {
				coastSlope = 0.0f;
				coastOffset = width / 8.f - coastDepth;
			}
		}
	}

	// 北面海岸
	if (coastMask & 4) {
		int coastDepth = height / 16 + GetRandom(height / 16);
		float coastOffset = 0.0f, coastSlope = 0.0f;
		debugf("Log: Sea in the north %d.\n", coastDepth);
		for (int x = 0; x < width; x++) {
			int coastLimit = coastDepth + static_cast<int>(coastOffset);
			if (coastLimit > height)
				coastLimit = height;
			for (int y = 0; y < coastLimit; y++) {
				if (getTerrain(x, y) == "terrain_mountain") continue; // 绕开已有山体,见西面海岸那段注释
				setTerrain(x, y, GetType());
				isOcean[x][y] = true;
			}
			coastSlope += float(GetRandom(33) - 16) / 256.0f;
			coastOffset += coastSlope;
			if (coastDepth + coastOffset <= height / 64) {
				coastSlope = 0.0f;
				coastOffset = height / 64.f - coastDepth;
			}
			if (coastDepth + coastOffset >= height / 8) {
				coastSlope = 0.0f;
				coastOffset = height / 8.f - coastDepth;
			}
		}
	}

	// 南面海岸
	if (coastMask & 8) {
		int coastDepth = height / 16 + GetRandom(height / 16);
		float coastOffset = 0.0f, coastSlope = 0.0f;
		debugf("Log: Sea in the south %d.\n", coastDepth);
		for (int x = 0; x < width; x++) {
			int coastStart = height - coastDepth - static_cast<int>(coastOffset);
			if (coastStart < 0)
				coastStart = 0;
			for (int y = height - 1; y >= coastStart; y--) {
				if (getTerrain(x, y) == "terrain_mountain") continue; // 绕开已有山体,见西面海岸那段注释
				setTerrain(x, y, GetType());
				isOcean[x][y] = true;
			}
			coastSlope += float(GetRandom(33) - 16) / 256.0f;
			coastOffset += coastSlope;
			if (coastDepth + coastOffset <= height / 64) {
				coastSlope = 0.0f;
				coastOffset = height / 64.f - coastDepth;
			}
			if (coastDepth + coastOffset >= height / 8) {
				coastSlope = 0.0f;
				coastOffset = height / 8.f - coastDepth;
			}
		}
	}

	// 以海岸线(与陆地相邻的海洋格)为基准,高度0,用BFS向外扩散;下降切线方向取当前点到地图中心连线的反方向(即离开地图中心的方向)
	vector<vector<float>> oceanHeight(width, vector<float>(height, 0.0f));
	vector<vector<bool>> isProcessed(width, vector<bool>(height, false));
	vector<vector<bool>> isEnqueued(width, vector<bool>(height, false));
	vector<vector<float>> slopeX(width, vector<float>(height, 0.0f));
	vector<vector<float>> slopeY(width, vector<float>(height, 0.0f));

	// BFS初始化:找出所有与陆地相邻的海洋格作为海岸线种子,高度为0
	vector<pair<int, int>> currentFront;
	for (int x = 0; x < width; x++)
		for (int y = 0; y < height; y++) {
			if (!isOcean[x][y]) continue;

			bool isCoast = false;
			for (auto& neighborOffset : neighborOffsets) {
				int neighborX = x + neighborOffset[0];
				int neighborY = y + neighborOffset[1];
				if (neighborX < 0 || neighborX >= width || neighborY < 0 || neighborY >= height) continue;
				if (!isOcean[neighborX][neighborY]) { isCoast = true; break; }
			}
			if (!isCoast) continue;

			oceanHeight[x][y] = 0.0f;
			isProcessed[x][y] = true;
			isEnqueued[x][y] = true;

			float awayX = x - mapCenterX, awayY = y - mapCenterY;
			float awayLen = sqrt(awayX * awayX + awayY * awayY);
			if (awayLen > 1e-5f) {
				slopeX[x][y] = -OCEAN_DEPTH_RATE * awayX / awayLen;
				slopeY[x][y] = -OCEAN_DEPTH_RATE * awayY / awayLen;
			}
			currentFront.push_back({ x, y });
		}

	// BFS逐圈向外扩散高度,越远离海岸线下降越多;第0圈是海岸线本身,第1圈维持0,从第2圈起才开始下降
	int ringIndex = 0;
	while (!currentFront.empty()) {
		vector<pair<int, int>> nextFront;
		for (auto& [cellX, cellY] : currentFront) {
			float heightSum = 0.0f;
			int neighborCount = 0;

			// X 方向找最近已处理邻居,用坡度预测当前格子高度
			for (int stepX : {-1, 1}) {
				int neighborX = cellX + stepX;
				if (neighborX < 0 || neighborX >= width) continue;
				if (!isOcean[neighborX][cellY] || !isProcessed[neighborX][cellY]) continue;
				float predicted = oceanHeight[neighborX][cellY] + slopeX[cellX][cellY] * (cellX - neighborX);
				if (predicted > oceanHeight[neighborX][cellY]) {
					slopeX[cellX][cellY] = -OCEAN_DEPTH_RATE * static_cast<float>(cellX - neighborX);
					predicted = oceanHeight[neighborX][cellY] - OCEAN_DEPTH_RATE;
				}
				heightSum += predicted;
				++neighborCount;
				break;
			}

			// Y 方向找最近已处理邻居,用坡度预测当前格子高度
			for (int stepY : {-1, 1}) {
				int neighborY = cellY + stepY;
				if (neighborY < 0 || neighborY >= height) continue;
				if (!isOcean[cellX][neighborY] || !isProcessed[cellX][neighborY]) continue;
				float predicted = oceanHeight[cellX][neighborY] + slopeY[cellX][cellY] * (cellY - neighborY);
				if (predicted > oceanHeight[cellX][neighborY]) {
					slopeY[cellX][cellY] = -OCEAN_DEPTH_RATE * static_cast<float>(cellY - neighborY);
					predicted = oceanHeight[cellX][neighborY] - OCEAN_DEPTH_RATE;
				}
				heightSum += predicted;
				++neighborCount;
				break;
			}

			float cellHeight = (neighborCount > 0) ? clampf(heightSum / neighborCount, -OCEAN_MAX_DEPTH, 0.0f) : 0.0f;
			if (ringIndex <= 1) cellHeight = 0.0f;
			oceanHeight[cellX][cellY] = cellHeight;
			isProcessed[cellX][cellY] = true;

			// 将未入队邻居加入下一圈并传播坡度
			for (auto& neighborOffset : neighborOffsets) {
				int neighborX = cellX + neighborOffset[0];
				int neighborY = cellY + neighborOffset[1];
				if (neighborX < 0 || neighborX >= width || neighborY < 0 || neighborY >= height) continue;
				if (!isOcean[neighborX][neighborY] || isProcessed[neighborX][neighborY] || isEnqueued[neighborX][neighborY]) continue;
				slopeX[neighborX][neighborY] = slopeX[cellX][cellY] + randf(-OCEAN_SLOPE_JITTER, OCEAN_SLOPE_JITTER);
				slopeY[neighborX][neighborY] = slopeY[cellX][cellY] + randf(-OCEAN_SLOPE_JITTER, OCEAN_SLOPE_JITTER);
				isEnqueued[neighborX][neighborY] = true;
				nextFront.push_back({ neighborX, neighborY });
			}
		}
		currentFront = move(nextFront);
		ringIndex++;
	}

	// 写回全局地图高度+水面(per-cell setWater，水位固定SEA_LEVEL)。
	for (int x = 0; x < width; x++)
		for (int y = 0; y < height; y++)
			if (isOcean[x][y]) {
				setHeight(x, y, oceanHeight[x][y]);
				setWater(x, y, true, SEA_LEVEL);
			}
}
