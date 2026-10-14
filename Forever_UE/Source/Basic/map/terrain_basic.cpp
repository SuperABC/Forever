#include "terrain_basic.h"

#include "common/utility.h"

#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <tuple>
#include <unordered_set>
#include <string>

// 海平面水位(地图元素单位)——Ocean/River都要用，两边共用一个常量，不要各自写各自的
// 字面量，不然谁改了忘记改另一边，河口就会出现"河水比海水还低/高一截"的落差。这是
// 唯一横跨多个地形类的常量，所以留在文件顶部；其余各地形专属的常量都挪到各自那一段
// 代码前面，不再和其他地形的常量混在一起。
#define SEA_LEVEL -0.1f

using namespace std;

// 八方向邻居偏移量——Ocean/Mountain/River三个地形都要用，同样放在文件顶部。
static const int neighborOffsets[8][2] = { {1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1} };

// ============================================================================
// OceanTerrain：从1-4条随机地图边界向内啃出摆动的海岸线，再从海岸线BFS向内陆传播深度。
// 详见terrain_basic.md"OceanTerrain"一节。
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
	return "ocean";
}

const char* OceanTerrain::GetType() const {
	return "ocean";
}

const char* OceanTerrain::GetName() {
	name = "海洋地形" + to_string(id);
	return name.data();
}

float OceanTerrain::GetPriority() const {
	// 必须低于MountainTerrain(见下面0.95)——海洋要后跑，这样碰到山体的海岸线段不会把
	// 山直接覆盖成海(下面4条海岸线雕刻循环里有专门的"已经是mountain就跳过"保护)，而是
	// 绕着已经生成好的山体雕刻，山体自己原有的侧向衰减坡度(平滑降到0)留在海岸边，不会
	// 出现"海直接切掉半座山"的硬悬崖。
	return 0.9f;
}

void OceanTerrain::SetupTexture() {
	diffusePath = "/Game/Asset/Textures/Terrain/PlainDiffuse.PlainDiffuse";
	normalPath = "/Game/Asset/Textures/Terrain/PlainNormal.PlainNormal";
	roughnessPath = "/Game/Asset/Textures/Terrain/PlainRoughness.PlainRoughness";
}

void OceanTerrain::DistributeTerrain(int width, int height,
	const function<string(int, int)>& getTerrain, const function<bool(int, int, string)>& setTerrain,
	const function<float(int, int)>& getHeight, const function<bool(int, int, float)>& setHeight,
	const function<bool(int, int, bool, float)>& setWater) const {

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
				// 已经是山的格子不雕刻成海——山先跑，这里绕开已经摆好的山体，让山自己
				// 原有的侧向衰减坡度(平滑降到0)留在海岸边，不会出现海直接切掉半座山的
				// 硬悬崖。isOcean不标记为true，这个格子对后面"是不是海岸线"的BFS种子
				// 判定来说，跟普通平地邻居没有任何区别——不需要额外特判。
				if (getTerrain(x, y) == "mountain") continue;
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
				if (getTerrain(x, y) == "mountain") continue; // 绕开已有山体,见西面海岸那段注释
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
				if (getTerrain(x, y) == "mountain") continue; // 绕开已有山体,见西面海岸那段注释
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
				if (getTerrain(x, y) == "mountain") continue; // 绕开已有山体,见西面海岸那段注释
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

	// 写回全局地图高度+水面——水位固定-0.1(和改之前完全一样的视觉效果),只是改成调
	// 新的per-cell setWater而不是让SetupTexture里的waterHeight统一铺一遍。
	for (int x = 0; x < width; x++)
		for (int y = 0; y < height; y++)
			if (isOcean[x][y]) {
				setHeight(x, y, oceanHeight[x][y]);
				setWater(x, y, true, SEA_LEVEL);
			}
}

// ============================================================================
// MountainTerrain：随机游走生成若干条山脊多段线，沿脊线衰减+侧向BFS扩散生成高度。
// 详见terrain_basic.md"MountainTerrain"一节。
// ============================================================================

// 山脊走向衰减参数
#define ALONG_BASE_RATE 0.30f
#define ALONG_RATE_JITTER 0.10f
#define ALONG_RATE_MIN -0.15f
#define ALONG_RATE_MAX 1.00f
// 侧向坡度扩散参数
#define LATERAL_BASE_RATE 0.35f
#define LATERAL_INIT_JITTER 0.15f
#define LATERAL_PROP_JITTER 0.06f
// 山脊游走参数
#define RIDGE_STEP_LEN 1.0f
#define RIDGE_TURN_JITTER 0.15f
#define RIDGE_SUB_STEPS 4

int MountainTerrain::count = 0;

MountainTerrain::MountainTerrain() : id(count++) {

}

MountainTerrain::~MountainTerrain() {

}

const char* MountainTerrain::GetId() {
	return "mountain";
}

const char* MountainTerrain::GetType() const {
	return "mountain";
}

const char* MountainTerrain::GetName() {
	name = "山区地形" + to_string(id);
	return name.data();
}

float MountainTerrain::GetPriority() const {
	// 必须高于OceanTerrain(见上面0.9)——山要先跑，海岸线雕刻的时候才能看到已经摆好的
	// 山体，绕开它雕刻，而不是在空地图上先铺好海，之后山再被迫跳过/硬撞上海洋格子。
	return 0.95f;
}

void MountainTerrain::SetupTexture() {
	diffusePath = "/Game/Asset/Textures/Terrain/RockDiffuse.RockDiffuse";
	normalPath = "/Game/Asset/Textures/Terrain/RockNormal.RockNormal";
	roughnessPath = "/Game/Asset/Textures/Terrain/RockRoughness.RockRoughness";
}

void MountainTerrain::DistributeTerrain(int width, int height,
	const function<string(int, int)>& getTerrain, const function<bool(int, int, string)>& setTerrain,
	const function<float(int, int)>& getHeight, const function<bool(int, int, float)>& setHeight,
	const function<bool(int, int, bool, float)>& setWater) const {

	auto randf = [](float lo, float hi) -> float {
		return lo + static_cast<float>(GetRandom(100000)) / 99999.0f * (hi - lo);
	};
	auto clampf = [](float value, float lo, float hi) -> float {
		if (!(value > lo)) return lo;
		return value > hi ? hi : value;
	};

	int densityScale = width * height / (512 * 512);
	int mountainCount = densityScale > 1 ? (4 + GetRandom(densityScale * 2)) : 0;
	int centerExcludeRadius = min(width, height) / 4;
	int successCount = 0;
	Counter counter(200);

	for (int attempt = 0; attempt < mountainCount; attempt++) {
		int centerX = width / 8 + GetRandom(width * 3 / 4);
		int centerY = height / 8 + GetRandom(height * 3 / 4);

		// 必须落在平原且远离地图中心
		if (getTerrain(centerX, centerY) != "plain" ||
			sqrt(pow(centerX - width / 2, 2) + pow(centerY - height / 2, 2)) < centerExcludeRadius) {
			if (!counter.Count())
				attempt--;
			continue;
		}

		float ridgeLength = 50.0f + densityScale * ((GetRandom(2) ? 0 : 1) * GetRandom(50)) + GetRandom(100);
		float peakHeight = 20.0f + GetRandom(30);

		// 从中心向两端随机游走,生成弯曲山脊控制点
		float ridgeAngle = randf(0.0f, 2.0f * 3.14159265f);
		float floatCenterX = static_cast<float>(centerX);
		float floatCenterY = static_cast<float>(centerY);
		int halfStepCount = max(1, static_cast<int>(ridgeLength * 0.5f / RIDGE_STEP_LEN));

		vector<pair<float, float>> backPoints, forwardPoints;

		float backWalkX = floatCenterX, backWalkY = floatCenterY, backAngle = ridgeAngle + 3.14159265f;
		for (int step = 0; step < halfStepCount; ++step) {
			backAngle += randf(-RIDGE_TURN_JITTER, RIDGE_TURN_JITTER);
			backWalkX += cosf(backAngle) * RIDGE_STEP_LEN;
			backWalkY += sinf(backAngle) * RIDGE_STEP_LEN;
			backPoints.push_back({ backWalkX, backWalkY });
		}

		float forwardWalkX = floatCenterX, forwardWalkY = floatCenterY, forwardAngle = ridgeAngle;
		for (int step = 0; step < halfStepCount; ++step) {
			forwardAngle += randf(-RIDGE_TURN_JITTER, RIDGE_TURN_JITTER);
			forwardWalkX += cosf(forwardAngle) * RIDGE_STEP_LEN;
			forwardWalkY += sinf(forwardAngle) * RIDGE_STEP_LEN;
			forwardPoints.push_back({ forwardWalkX, forwardWalkY });
		}

		// 合并为从后端到前端的完整曲线点列表
		vector<pair<float, float>> curvePoints;
		for (auto backIter = backPoints.rbegin(); backIter != backPoints.rend(); ++backIter)
			curvePoints.push_back(*backIter);
		curvePoints.push_back({ floatCenterX, floatCenterY });
		for (auto& forwardPoint : forwardPoints)
			curvePoints.push_back(forwardPoint);

		// 计算曲线覆盖的边界框,加上侧向扩展余量
		int minGlobalX = static_cast<int>(lround(floatCenterX)), maxGlobalX = minGlobalX;
		int minGlobalY = static_cast<int>(lround(floatCenterY)), maxGlobalY = minGlobalY;
		for (auto& [curveX, curveY] : curvePoints) {
			int x = static_cast<int>(lround(curveX)), y = static_cast<int>(lround(curveY));
			if (x < minGlobalX) minGlobalX = x;
			if (x > maxGlobalX) maxGlobalX = x;
			if (y < minGlobalY) minGlobalY = y;
			if (y > maxGlobalY) maxGlobalY = y;
		}
		int expansionPad = static_cast<int>(peakHeight / (LATERAL_BASE_RATE - LATERAL_INIT_JITTER) + 5.0f);
		int regionLeft = max(0, minGlobalX - expansionPad);
		int regionBottom = max(0, minGlobalY - expansionPad);
		int regionRight = min(width - 1, maxGlobalX + expansionPad);
		int regionTop = min(height - 1, maxGlobalY + expansionPad);
		int regionWidth = regionRight - regionLeft + 1;
		int regionHeight = regionTop - regionBottom + 1;
		if (regionWidth <= 0 || regionHeight <= 0) continue;

		// 局部高度、处理标记与侧向坡度数组
		vector<vector<float>> localHeight(regionWidth, vector<float>(regionHeight, 0.0f));
		vector<vector<bool>> isProcessed(regionWidth, vector<bool>(regionHeight, false));
		vector<vector<bool>> isEnqueued(regionWidth, vector<bool>(regionHeight, false));
		vector<vector<float>> slopeX(regionWidth, vector<float>(regionHeight, 0.0f));
		vector<vector<float>> slopeY(regionWidth, vector<float>(regionHeight, 0.0f));

		// 将曲线点列线性插值并栅格化为局部格子序列
		vector<pair<int, int>> ridgeCells;
		int lastCellX = -1, lastCellY = -1;
		for (size_t segIdx = 0; segIdx + 1 < curvePoints.size(); ++segIdx) {
			float segStartX = curvePoints[segIdx].first, segStartY = curvePoints[segIdx].second;
			float segEndX = curvePoints[segIdx + 1].first, segEndY = curvePoints[segIdx + 1].second;
			for (int subStep = 0; subStep <= RIDGE_SUB_STEPS; ++subStep) {
				float segFrac = static_cast<float>(subStep) / RIDGE_SUB_STEPS;
				int globalX = static_cast<int>(lround(segStartX + (segEndX - segStartX) * segFrac));
				int globalY = static_cast<int>(lround(segStartY + (segEndY - segStartY) * segFrac));
				int localX = globalX - regionLeft;
				int localY = globalY - regionBottom;
				if (localX < 0 || localX >= regionWidth || localY < 0 || localY >= regionHeight) continue;
				if (localX != lastCellX || localY != lastCellY) {
					ridgeCells.push_back({ localX, localY });
					lastCellX = localX;
					lastCellY = localY;
				}
			}
		}
		if (ridgeCells.empty()) continue;

		// 计算每个山脊格子的单位切线方向
		vector<pair<float, float>> ridgeDirections(ridgeCells.size());
		for (size_t ridgeIdx = 0; ridgeIdx < ridgeCells.size(); ++ridgeIdx) {
			size_t prevIdx = (ridgeIdx == 0) ? 0 : ridgeIdx - 1;
			size_t nextIdx = (ridgeIdx + 1 < ridgeCells.size()) ? ridgeIdx + 1 : ridgeIdx;
			float deltaX = static_cast<float>(ridgeCells[nextIdx].first - ridgeCells[prevIdx].first);
			float deltaY = static_cast<float>(ridgeCells[nextIdx].second - ridgeCells[prevIdx].second);
			float tanLen = sqrtf(deltaX * deltaX + deltaY * deltaY);
			if (tanLen < 1e-5f) { deltaX = cosf(ridgeAngle); deltaY = sinf(ridgeAngle); tanLen = 1.0f; }
			ridgeDirections[ridgeIdx] = { deltaX / tanLen, deltaY / tanLen };
		}

		// 从随机峰值格子向两端沿山脊递减高度
		int peakIndex = GetRandom(static_cast<int>(ridgeCells.size()));
		auto [peakX, peakY] = ridgeCells[peakIndex];
		localHeight[peakX][peakY] = peakHeight;
		isProcessed[peakX][peakY] = true;

		float forwardHeight = peakHeight, forwardRate = ALONG_BASE_RATE;
		for (int ridgeIdx = peakIndex + 1; ridgeIdx < static_cast<int>(ridgeCells.size()); ++ridgeIdx) {
			forwardRate = clampf(forwardRate + randf(-ALONG_RATE_JITTER, ALONG_RATE_JITTER), ALONG_RATE_MIN, ALONG_RATE_MAX);
			forwardHeight = clampf(forwardHeight - forwardRate, 0.0f, peakHeight);
			auto [localX, localY] = ridgeCells[ridgeIdx];
			localHeight[localX][localY] = forwardHeight;
			isProcessed[localX][localY] = true;
		}

		float backwardHeight = peakHeight, backwardRate = ALONG_BASE_RATE;
		for (int ridgeIdx = peakIndex - 1; ridgeIdx >= 0; --ridgeIdx) {
			backwardRate = clampf(backwardRate + randf(-ALONG_RATE_JITTER, ALONG_RATE_JITTER), ALONG_RATE_MIN, ALONG_RATE_MAX);
			backwardHeight = clampf(backwardHeight - backwardRate, 0.0f, peakHeight);
			auto [localX, localY] = ridgeCells[ridgeIdx];
			localHeight[localX][localY] = backwardHeight;
			isProcessed[localX][localY] = true;
		}

		// BFS 初始化,设置紧邻山脊格子的侧向坡度并加入第一圈
		vector<pair<int, int>> currentFront;
		for (size_t ridgeIdx = 0; ridgeIdx < ridgeCells.size(); ++ridgeIdx) {
			int ridgeX = ridgeCells[ridgeIdx].first;
			int ridgeY = ridgeCells[ridgeIdx].second;
			float perpDirX = -ridgeDirections[ridgeIdx].second;
			float perpDirY = ridgeDirections[ridgeIdx].first;
			for (auto& neighborOffset : neighborOffsets) {
				int neighborX = ridgeX + neighborOffset[0];
				int neighborY = ridgeY + neighborOffset[1];
				if (neighborX < 0 || neighborX >= regionWidth || neighborY < 0 || neighborY >= regionHeight) continue;
				if (isProcessed[neighborX][neighborY] || isEnqueued[neighborX][neighborY]) continue;
				float perpDot = perpDirX * neighborOffset[0] + perpDirY * neighborOffset[1];
				float lateralSide = (perpDot >= 0.0f) ? 1.0f : -1.0f;
				float initLateralRate = LATERAL_BASE_RATE + randf(-LATERAL_INIT_JITTER, LATERAL_INIT_JITTER);
				slopeX[neighborX][neighborY] = -initLateralRate * lateralSide * perpDirX;
				slopeY[neighborX][neighborY] = -initLateralRate * lateralSide * perpDirY;
				isEnqueued[neighborX][neighborY] = true;
				currentFront.push_back({ neighborX, neighborY });
			}
		}

		// BFS 逐圈向外扩散高度
		while (!currentFront.empty()) {
			vector<pair<int, int>> nextFront;
			for (auto& [cellX, cellY] : currentFront) {
				float heightSum = 0.0f;
				int neighborCount = 0;

				// X 方向找最近已处理邻居,用坡度预测当前格子高度
				for (int stepX : {-1, 1}) {
					int neighborX = cellX + stepX;
					if (neighborX < 0 || neighborX >= regionWidth) continue;
					if (!isProcessed[neighborX][cellY]) continue;
					float predicted = localHeight[neighborX][cellY] + slopeX[cellX][cellY] * (cellX - neighborX);
					if (predicted > localHeight[neighborX][cellY]) {
						slopeX[cellX][cellY] = -LATERAL_BASE_RATE * static_cast<float>(cellX - neighborX);
						predicted = localHeight[neighborX][cellY] - LATERAL_BASE_RATE;
					}
					heightSum += predicted;
					++neighborCount;
					break;
				}

				// Y 方向找最近已处理邻居,用坡度预测当前格子高度
				for (int stepY : {-1, 1}) {
					int neighborY = cellY + stepY;
					if (neighborY < 0 || neighborY >= regionHeight) continue;
					if (!isProcessed[cellX][neighborY]) continue;
					float predicted = localHeight[cellX][neighborY] + slopeY[cellX][cellY] * (cellY - neighborY);
					if (predicted > localHeight[cellX][neighborY]) {
						slopeY[cellX][cellY] = -LATERAL_BASE_RATE * static_cast<float>(cellY - neighborY);
						predicted = localHeight[cellX][neighborY] - LATERAL_BASE_RATE;
					}
					heightSum += predicted;
					++neighborCount;
					break;
				}

				float cellHeight = (neighborCount > 0) ? clampf(heightSum / neighborCount, 0.0f, peakHeight) : 0.0f;

				// 九宫格连续性修正,抑制高度突变
				for (int adjX = -1; adjX <= 1; ++adjX)
					for (int adjY = -1; adjY <= 1; ++adjY) {
						if (adjX == 0 && adjY == 0) continue;
						int neighborX = cellX + adjX;
						int neighborY = cellY + adjY;
						if (neighborX < 0 || neighborX >= regionWidth || neighborY < 0 || neighborY >= regionHeight) continue;
						if (!isProcessed[neighborX][neighborY]) continue;
						float heightDiff = localHeight[neighborX][neighborY] - cellHeight;
						if (fabs(heightDiff) > 2.0f)
							cellHeight = clampf(cellHeight + 0.2f * heightDiff, 0.0f, peakHeight);
					}

				localHeight[cellX][cellY] = cellHeight;
				isProcessed[cellX][cellY] = true;

				if (cellHeight <= 0.0f) continue;

				// 将未入队邻居加入下一圈并传播坡度
				for (auto& neighborOffset : neighborOffsets) {
					int neighborX = cellX + neighborOffset[0];
					int neighborY = cellY + neighborOffset[1];
					if (neighborX < 0 || neighborX >= regionWidth || neighborY < 0 || neighborY >= regionHeight) continue;
					if (isProcessed[neighborX][neighborY] || isEnqueued[neighborX][neighborY]) continue;
					slopeX[neighborX][neighborY] = slopeX[cellX][cellY] + randf(-LATERAL_PROP_JITTER, LATERAL_PROP_JITTER);
					slopeY[neighborX][neighborY] = slopeY[cellX][cellY] + randf(-LATERAL_PROP_JITTER, LATERAL_PROP_JITTER);
					isEnqueued[neighborX][neighborY] = true;
					nextFront.push_back({ neighborX, neighborY });
				}
			}
			currentFront = move(nextFront);
		}

		// 5x5 均值平滑
		vector<vector<float>> smoothedHeight(regionWidth, vector<float>(regionHeight, 0.0f));
		for (int localX = 0; localX < regionWidth; ++localX)
			for (int localY = 0; localY < regionHeight; ++localY) {
				float heightSum = 0.0f;
				int neighborCount = 0;
				for (int adjX = -2; adjX <= 2; ++adjX)
					for (int adjY = -2; adjY <= 2; ++adjY) {
						int neighborX = localX + adjX, neighborY = localY + adjY;
						if (neighborX < 0 || neighborX >= regionWidth || neighborY < 0 || neighborY >= regionHeight) continue;
						heightSum += localHeight[neighborX][neighborY];
						++neighborCount;
					}
				smoothedHeight[localX][localY] = heightSum / neighborCount;
			}

		// 写回全局地图,只覆盖平原和已有山地
		for (int localX = 0; localX < regionWidth; ++localX)
			for (int localY = 0; localY < regionHeight; ++localY) {
				if (smoothedHeight[localX][localY] <= 0.0f) continue;
				int globalX = regionLeft + localX;
				int globalY = regionBottom + localY;
				string terrainType = getTerrain(globalX, globalY);
				if (terrainType != "plain" && terrainType != GetType()) continue;
				setTerrain(globalX, globalY, GetType());
				float existingHeight = getHeight(globalX, globalY);
				setHeight(globalX, globalY,
					(isfinite(existingHeight) && existingHeight > smoothedHeight[localX][localY])
					? existingHeight : smoothedHeight[localX][localY]);
			}

		successCount++;
	}

	debugf("Log: Generate mountain %d.\n", successCount);
}

// ============================================================================
// RiverTerrain：山体内D8单流向流量累积确定起点，流出山体后按角度/角速度模型继续蔓延，
// 全有或全无(只有流到地图边界/海洋/并入已有河才保留)。详见terrain_basic.md
// "RiverTerrain"一节。
// ============================================================================

// 累积流量超过这个值的山体格子才有资格成为河流起点——第一阶段debug用的起始值，看完
// Blue贴图标出来的位置效果再调。
#define RIVER_FLOW_THRESHOLD 30.0f
// 新河流起点与所有已有起点的最小距离(element单位，1 element=10m)——起点挤得太近会长出
// 一堆紧挨着的平行短河。200=2000m，从100加大到这个值。
#define RIVER_ORIGIN_MIN_SPACING 200.0f
// 流出山体之后，流向用角度(弧度)表示，偏向速度是角速度(标量，不是向量)——只保留偏向
// 速度这一层，不再叠加偏向加速度(导数阶数太高，行为难控制)。每步直接给角速度加一个
// 带正负号的随机小量(弧度/步)，角度按角速度积分更新。先给个起始值，看完效果再调。
#define RIVER_ANGULAR_VEL_JITTER 0.015f
// 角速度本身夹的上限(弧度/步)，防止扰动累积出转得太快、路径碎成一团——角速度是没有
// 均值回归的纯随机游走，夹了上限不代表只是偶尔碰一下，实际会在边界附近停留很长时间。
#define RIVER_ANGULAR_VEL_MAX 0.02f
// 沿流向每次前进的距离(element单位)，按这个步长采样落在哪个格子上。
#define RIVER_SAMPLE_STEP 0.5f
// 第三阶段：河流格子的河床比原地形低多少(地图元素单位)。水位不用单独的偏移常量——
// 直接用"原地形高度+SEA_LEVEL"，和海洋同一个公式，天然在河口处对上海平面。
#define RIVER_HEIGHT_DROP 0.5f
// 出山口附近坡度检查：山体内路径里连续两格(更靠源头-更靠山脚)的高度差超过这个值就截断，
// 河流起点改到截断点，不让河流从很陡的地方发源。
#define RIVER_MAX_STEP_DROP 0.2f
// 河岸平滑范围(格数)——非河流格子距河流越近，高度下降得越多(线性过渡)，超过这个距离
// 完全不下降，先给个起始值。
#define RIVER_BLUR_RADIUS 2
// 河道加宽起始半径——1.5刚好能把3x3内的8个邻居(对角距离sqrt(2)≈1.414)全部纳入，
// 对应原来固定宽度3的效果，作为"流程=0"时的基准半径。
#define RIVER_BASE_WIDEN_RADIUS 1.5f
// 河道每流经这么多格(riverDistance)，加宽半径增加0.5——对称膨胀一次增加的是
// 直径(两侧各加一点)，半径+0.5相当于宽度+1，所以是"每200格宽度+1"。
#define RIVER_WIDTH_GROW_PER_CELLS 200.0f
// 碰到海之后顺着原流向继续往海里推这么多格(element单位)，强制多接几格真海洋格子进
// 河流——海洋贴着陆地那一圈其实是"沙滩"(高度固定0，只比固定水位高一点)，只接触到的
// 这一两格可能还没真正越过沙滩，往深一点的海里多推几格，确保河床高度公式切过沙滩。
#define RIVER_MOUTH_EXTEND_CELLS 4.0f
// 地图正中心留给井字路网中心lot(AREA_OFFICIAL_HIGH，见roadnet_basic.cpp)用——河流只要
// 流经地图中心这片区域(宽高方向各3/8~5/8那一块)，不管是山体内还是流出山体之后，整条河
// (两段一起)直接作废，不让河流占掉地图中心那块地。
#define RIVER_CENTER_MIN_FRAC (3.0f / 8.0f)
#define RIVER_CENTER_MAX_FRAC (5.0f / 8.0f)

int RiverTerrain::count = 0;

RiverTerrain::RiverTerrain() : id(count++) {

}

RiverTerrain::~RiverTerrain() {

}

const char* RiverTerrain::GetId() {
	return "river";
}

const char* RiverTerrain::GetType() const {
	return "river";
}

const char* RiverTerrain::GetName() {
	name = "河流地形" + to_string(id);
	return name.data();
}

float RiverTerrain::GetPriority() const {
	// 必须低于Mountain(0.95)和Ocean(0.9)——DistributeTerrain跑到这里时要能读到两者
	// 写完之后的最终地形分类。
	return 0.5f;
}

void RiverTerrain::SetupTexture() {
	// 调试阶段用的纯色Blue贴图已经验证过河道位置没问题,正式效果改成跟平地同一张贴图——
	// 河床本身会凹下去、上面盖一层水面mesh,河道在视觉上已经靠地形凹陷+水面表现出来,
	// 不需要再用地表材质本身的颜色去额外强调。
	diffusePath = "/Game/Asset/Textures/Terrain/PlainDiffuse.PlainDiffuse";
	// normalPath/roughnessPath留空——现有fallback逻辑会填平的法线+中性粗糙度
}

void RiverTerrain::DistributeTerrain(int width, int height,
	const function<string(int, int)>& getTerrain, const function<bool(int, int, string)>& setTerrain,
	const function<float(int, int)>& getHeight, const function<bool(int, int, float)>& setHeight,
	const function<bool(int, int, bool, float)>& setWater) const {

	int cellCount = width * height;
	vector<float> heights(cellCount);
	vector<bool> isMountain(cellCount);
	vector<int> mountainIndices;
	mountainIndices.reserve(cellCount);

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int idx = y * width + x;
			heights[idx] = getHeight(x, y);
			isMountain[idx] = (getTerrain(x, y) == "mountain");
			if (isMountain[idx]) mountainIndices.push_back(idx);
		}
	}

	// 按高度降序结算D8:轮到每个山体格子时,它从更高邻居那里该收到的流量已经全部收到了
	// (更高的格子必然排在它前面处理)。
	sort(mountainIndices.begin(), mountainIndices.end(),
		[&heights](int a, int b) { return heights[a] > heights[b]; });

	vector<float> accum(cellCount, 0.0f);
	for (int idx : mountainIndices) accum[idx] = 1.0f;
	// 每个山体格子自己的D8下坡目标(8邻居里最低的那个山体格子)，流到非山体就停的格子记-1——
	// 和accum同一个条件判断，流量累积"要不要往外送"和河流"要不要往下蔓延"走的是同一条路径。
	vector<int> flowTarget(cellCount, -1);
	// 8邻居里绝对最低的那个,不管是不是山体都记下来——流到非山体就停的格子,flowTarget是-1,
	// 但exitTarget还留着"原本想流去哪个非山体格子",供流出山体之后的第二阶段续上。
	vector<int> exitTarget(cellCount, -1);

	for (int idx : mountainIndices) {
		int x = idx % width;
		int y = idx / width;
		// 比较的是坡度(高度差/距离)不是原始高度——对角邻居比轴向邻居远sqrt(2)倍,直接比
		// 原始高度会让对角方向系统性"看起来更低"(同样的坡度,对角方向覆盖的水平距离更长、
		// 高度差自然更大)，结果所有流向都挤成45度斜线，转弯也只能在4个对角方向之间跳、
		// 全是90度直角。按坡度比才能让轴向/对角公平竞争。
		float bestSlope = 0.f; // 必须严格>0才算"更低"，和原来heights[nIdx]<heights[idx]同一个门槛
		int lowestIdx = -1;
		for (auto& neighborOffset : neighborOffsets) {
			int nx = x + neighborOffset[0];
			int ny = y + neighborOffset[1];
			if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
			int nIdx = ny * width + nx;
			float dist = (neighborOffset[0] != 0 && neighborOffset[1] != 0) ? 1.41421356f : 1.0f;
			float slope = (heights[idx] - heights[nIdx]) / dist;
			if (slope > bestSlope) {
				bestSlope = slope;
				lowestIdx = nIdx;
			}
		}
		if (lowestIdx >= 0) exitTarget[idx] = lowestIdx;
		// 没有比自己更低的邻居(局部坑),或者最低的邻居不是山体:流到这里为止,不往外送
		// (这就是"流到非山体就停止")。
		if (lowestIdx < 0 || !isMountain[lowestIdx]) continue;
		accum[lowestIdx] += accum[idx];
		flowTarget[idx] = lowestIdx;
	}

	float maxAccum = 0.0f;
	for (int idx : mountainIndices) {
		if (accum[idx] > maxAccum) maxAccum = accum[idx];
	}

	vector<bool> isRiver(cellCount, false);
	// 每个河流格子相对自己所在这条链路起点的"流程"(经过的格子数，不是实际距离)——用来做
	// "越往下游越宽"：每条链路自己从0开始数，山体内和流出山体之后两段接续计数，不是从
	// 全图某个固定点算。
	vector<int> riverDistance(cellCount, 0);
	vector<pair<int, int>> riverOrigins; // 已确认的河流起点坐标，供间距检查用

	auto distanceToAllOriginsOk = [&](int x, int y) {
		for (auto& origin : riverOrigins) {
			float dx = float(x - origin.first), dy = float(y - origin.second);
			if (sqrtf(dx * dx + dy * dy) <= RIVER_ORIGIN_MIN_SPACING) return false;
		}
		return true;
		};

	auto hasRiverNeighbor = [&](int x, int y) {
		for (auto& off : neighborOffsets) {
			int nx = x + off[0], ny = y + off[1];
			if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
			if (isRiver[ny * width + nx]) return true;
		}
		return false;
		};

	// 地图正中心(宽高方向各3/8~5/8那一块，留给井字路网中心lot用)——河流只要流经这片区域
	// 就整条作废，不管是山体内还是流出山体之后，判定一视同仁。
	auto inCenterRegion = [&](int x, int y) {
		return x >= (int)(width * RIVER_CENTER_MIN_FRAC) && x < (int)(width * RIVER_CENTER_MAX_FRAC) &&
			y >= (int)(height * RIVER_CENTER_MIN_FRAC) && y < (int)(height * RIVER_CENTER_MAX_FRAC);
		};

	// flowTarget链路上流量只会越来越大(下坡目标的accum已经把起点的accum加进去了)，起点本身
	// 过线，整条下坡链路自然都过线，不用每一步再查一次accum，一路标记到流出山体/局部坑，
	// 或者撞上已经标记过的河流(提前停止，避免重复工作)为止。返回链路真正的终点下标+这条
	// 链路新标记的格子列表(只记"之前不是河流、这次改成true"的那些，供失败时整体撤销用)+
	// 完整路径(按经过顺序，供算出山时的初始流向用——只看最后两格算方向，方向必然是D8的
	// 8个离散方向之一，归一化之后角度还是只能是0/45/90/135这几个值，所以要往回多看几格)。
	auto propagateDownhill = [&](int startIdx) {
		vector<int> touched;
		vector<int> path;
		int cur = startIdx;
		while (true) {
			path.push_back(cur);
			if (!isRiver[cur]) {
				isRiver[cur] = true;
				riverDistance[cur] = (int)touched.size(); // 这条链路自己从0开始数
				touched.push_back(cur);
			}
			int next = flowTarget[cur];
			if (next < 0 || isRiver[next]) break;
			cur = next;
		}
		return make_tuple(cur, touched, path);
		};

	// 流出山体之后：连续坐标按流向每次前进RIVER_SAMPLE_STEP(0.5)距离，采样点落到哪个
	// element就把那个element标记成河流；流向用角度(标量)表示，偏向速度是角速度(标量，
	// 不是向量)，每步直接给角速度加随机扰动再积分进角度，没有"向量模长"这种概念，也
	// 不存在向量退化成0的问题。采样到的element比上一个高(说明偏向速度把路径带上坡了)，
	// 就在它的8邻居里找高度最低的那个(不是坡度最大的，这里单纯是脱困，不用再按距离
	// 归一化)，强制把角度刷新成指向那个邻居。
	// 结束原因：碰到mountain、真正的局部坑(连刷新都找不到更低的邻居)都算失败，连同
	// 这条链路山体内的那一段一起整体撤销；碰到地图边界/ocean才算成功保留；碰到别的河——
	// 和"起点+下坡蔓延"规则里的既有机制一致，算并入，不算失败也不算新起点。
	// (只装新标记的格子，同propagateDownhill的touched，失败时和山体内那段一起撤销)
	enum class RiverEnd { MapEdge, Ocean, OtherRiver, Mountain, Pit, StepLimit, CenterBlocked };
	auto randf = [](float lo, float hi) { return lo + GetRandom(100000) / 99999.0f * (hi - lo); };
	auto continueRiverOutsideMountain = [&](int startX, int startY, float initDirX, float initDirY,
		const vector<int>& ownChainSeed, int startDistance) {
		vector<int> touched;
		// "自己这条链路已经标记过的格子"不算"别的河"——山体内propagateDownhill那段
		// (ownChainSeed)和这个函数自己往下走标记的格子都要排除,否则刚走一步,上一个格子
		// (或者山体内最后那几格)就会被当成"碰到别的河"立刻中止,河流根本流不出去。
		unordered_set<int> ownChainSet(ownChainSeed.begin(), ownChainSeed.end());
		// riverDistance接着山体内那段(startDistance=mountainTouched.size())继续往下数,
		// 不从0重开——下游变宽是看整条链路的流程，不是只看流出山体之后这一段。
		auto markRiver = [&](int ex, int ey) {
			int idx = ey * width + ex;
			if (!isRiver[idx]) {
				isRiver[idx] = true;
				riverDistance[idx] = startDistance + (int)touched.size();
				touched.push_back(idx);
			}
			ownChainSet.insert(idx);
			};
		// "碰到别的河"按8邻居判定,不是非得正好采样到河流格子本身才算——挨着河流(哪怕还没
		// 踩上去)就应该停,不然两条河会贴在一起平行流一段才真正合并，缝隙处很突兀。
		auto hasNearbyRiver = [&](int cx, int cy) {
			int selfIdx = cy * width + cx;
			if (isRiver[selfIdx] && !ownChainSet.count(selfIdx)) return true;
			for (auto& off : neighborOffsets) {
				int nx = cx + off[0], ny = cy + off[1];
				if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
				int nIdx = ny * width + nx;
				if (isRiver[nIdx] && !ownChainSet.count(nIdx)) return true;
			}
			return false;
			};
		// 海洋也按8邻居判定——挨着海(哪怕采样点本身还没真的落在海洋格子上)就算到了,不用
		// 非得真的踩进海里一格才算成功,和"碰到别的河"同一个道理。
		auto hasNearbyOcean = [&](int cx, int cy) {
			if (getTerrain(cx, cy) == "ocean") return true;
			for (auto& off : neighborOffsets) {
				int nx = cx + off[0], ny = cy + off[1];
				if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
				if (getTerrain(nx, ny) == "ocean") return true;
			}
			return false;
			};
		// 规则1：判定"到海了"用的是8邻居(挨着海就算到，不用真的踩进海里一格)，但河流的
		// 格子本身到此为止——如果不把那个触发判定的海洋格子也并进河流，中间会缺一段，
		// 看起来像河流在岸边几米外凭空断掉。把检测到的那个/那些挨着的海洋格子直接标记成
		// 河流，接上这段缺口。
		auto convertTouchingOceanToRiver = [&](int cx, int cy) {
			if (getTerrain(cx, cy) == "ocean") markRiver(cx, cy);
			for (auto& off : neighborOffsets) {
				int nx = cx + off[0], ny = cy + off[1];
				if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
				if (getTerrain(nx, ny) == "ocean") markRiver(nx, ny);
			}
			};
		// 海洋最外面一圈其实是"沙滩"(贴着陆地那圈海洋格子,高度固定是0，只比固定水位
		// SEA_LEVEL高一点点，参见terrain_basic.md"入海口"一节)——只接到刚碰到的这一两格
		// 海洋，河床高度公式可能还没真正算到沙滩后面更深的海里，沙滩这一截挡在中间看起来
		// 还是断的。碰到海之后继续顺着原来的流向往海里再推RIVER_MOUTH_EXTEND_CELLS格，
		// 强制多标记这几格海洋格子为河流，让河床高度公式(原高度-下降量)在沙滩之外的更深
		// 海域也执行一遍，确保真正切过沙滩连到深一点的海里。
		auto extendIntoOcean = [&](float fromX, float fromY, float dirX, float dirY) {
			float len = sqrtf(dirX * dirX + dirY * dirY);
			if (len < 1e-6f) return; // 方向退化(理论上不会发生),不推
			float ux = dirX / len, uy = dirY / len;
			float px = fromX, py = fromY;
			int lastExtEx = (int)roundf(fromX), lastExtEy = (int)roundf(fromY);
			int steps = (int)ceilf(RIVER_MOUTH_EXTEND_CELLS / RIVER_SAMPLE_STEP);
			for (int s = 0; s < steps; s++) {
				px += ux * RIVER_SAMPLE_STEP;
				py += uy * RIVER_SAMPLE_STEP;
				if (px < 0.f || px >= float(width) || py < 0.f || py >= float(height)) break;
				int ex = (int)roundf(px), ey = (int)roundf(py);
				ex = max(0, min(width - 1, ex));
				ey = max(0, min(height - 1, ey));
				if (ex == lastExtEx && ey == lastExtEy) continue;
				markRiver(ex, ey);
				lastExtEx = ex; lastExtEy = ey;
			}
			};

		string startTerrain = getTerrain(startX, startY);
		if (inCenterRegion(startX, startY)) return make_pair(RiverEnd::CenterBlocked, touched);
		if (hasNearbyOcean(startX, startY)) {
			convertTouchingOceanToRiver(startX, startY);
			extendIntoOcean(float(startX), float(startY), initDirX, initDirY);
			return make_pair(RiverEnd::Ocean, touched);
		}
		if (startTerrain == "mountain") return make_pair(RiverEnd::Mountain, touched);
		if (hasNearbyRiver(startX, startY)) return make_pair(RiverEnd::OtherRiver, touched);
		markRiver(startX, startY);

		float posX = float(startX), posY = float(startY);
		int lastEx = startX, lastEy = startY;
		float prevHeight = getHeight(startX, startY);

		float theta = atan2f(initDirY, initDirX); // 流向角度(弧度)，初始值继承自山体出口方向
		float angularVel = 0.f; // 角速度(标量，弧度/步)

		int maxSteps = (int)((width + height) * 2 / RIVER_SAMPLE_STEP); // 步距变小,步数要相应放大

		for (int step = 0; step < maxSteps; step++) {
			// 规则3：每步直接给角速度加一个带正负号的随机小量，夹到绝对值上限；角度按
			// 角速度积分更新——只有这一层积分，没有"加速度"。
			angularVel = clamp(angularVel + randf(-RIVER_ANGULAR_VEL_JITTER, RIVER_ANGULAR_VEL_JITTER), -RIVER_ANGULAR_VEL_MAX, RIVER_ANGULAR_VEL_MAX);
			theta += angularVel;
			float dirX = cosf(theta), dirY = sinf(theta);

			posX += dirX * RIVER_SAMPLE_STEP;
			posY += dirY * RIVER_SAMPLE_STEP;
			if (posX < 0.f || posX >= float(width) || posY < 0.f || posY >= float(height))
				return make_pair(RiverEnd::MapEdge, touched); // 规则：流到地图边界算成功

			int ex = (int)roundf(posX), ey = (int)roundf(posY);
			ex = max(0, min(width - 1, ex));
			ey = max(0, min(height - 1, ey));
			if (ex == lastEx && ey == lastEy) continue; // 还在同一个element里,这次采样不触发任何判定

			// 规则：流经地图中心(留给路网中心lot的那片区域)就整条河作废——这里直接返回
			// CenterBlocked(不在kept的MapEdge/Ocean/OtherRiver白名单里)，外层调用方会连同
			// 山体内那段一起整体撤销，和Mountain/Pit同一个失败语义。
			if (inCenterRegion(ex, ey)) return make_pair(RiverEnd::CenterBlocked, touched);

			// 这里查isRiver(内存里的临时状态)而不是getTerrain——真实地图这次要等全部链路都
			// 决定完才一次性提交，不然先成功的链路还没写进真实地图，后面的链路就看不到它。
			if (hasNearbyRiver(ex, ey)) return make_pair(RiverEnd::OtherRiver, touched); // 碰到别的河(8邻居范围内),算并入,不是失败
			if (hasNearbyOcean(ex, ey)) { // 挨着海(8邻居范围内),算流到海,成功
				markRiver(ex, ey); // 这一格本身先接上(走到这一步说明还没真正setTerrain过)
				convertTouchingOceanToRiver(ex, ey); // 把触发判定的海洋格子也并进河流,接上缺口
				extendIntoOcean(posX, posY, dirX, dirY); // 顺着原流向继续往海里推几格,切过沙滩
				return make_pair(RiverEnd::Ocean, touched);
			}
			string t = getTerrain(ex, ey);
			if (t == "mountain") return make_pair(RiverEnd::Mountain, touched); // 规则：碰到山体,整条河作废

			float h = getHeight(ex, ey);
			if (h > prevHeight + 1e-4f) {
				// 采样到的格子比上一格高——角速度把路径带上坡了，在这个格子的8邻居里
				// 找高度最低的那个，强制把角度刷新成指向它，角速度清零(不带着刚才那个
				// 走错方向的转速继续转)；如果连这个"最低的"也不比当前更低，说明真的是
				// 一个局部坑，没有任何方向能逃出去——规则：流到洼地中止,整条河作废。
				int bestDx = 0, bestDy = 0; float bestHeight = 1e30f; bool found = false;
				for (auto& off : neighborOffsets) {
					int nx = ex + off[0], ny = ey + off[1];
					if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
					float nh = getHeight(nx, ny);
					if (nh < bestHeight) { bestHeight = nh; bestDx = off[0]; bestDy = off[1]; found = true; }
				}
				if (!found || bestHeight >= h - 1e-4f) return make_pair(RiverEnd::Pit, touched);
				theta = atan2f(float(bestDy), float(bestDx));
				angularVel = 0.f;
			}

			markRiver(ex, ey);
			prevHeight = h;
			lastEx = ex; lastEy = ey;
		}
		return make_pair(RiverEnd::StepLimit, touched); // 步数耗尽也没到边界/海,按失败处理
		};

	int originCount = 0, keptCount = 0;
	vector<vector<int>> oceanMouthChains; // 诊断用，见下面origin循环里的记录点
	for (int idx : mountainIndices) { // 同一个降序顺序，"先到达阈值的"就是先遍历到的
		if (isRiver[idx] || accum[idx] < RIVER_FLOW_THRESHOLD) continue;
		int x = idx % width, y = idx / width;
		// 能变成河流只有一条路：下坡蔓延(只有已有河流格子自己的最低邻居会被标记)。这里只
		// 负责判断"要不要批准成一个新起点"——挨着已有河流但不是它的下坡目标，不算，跳过；
		// 离所有已有起点都不够远，也跳过；两条都躲过了才能当新起点。
		if (hasRiverNeighbor(x, y)) continue;
		if (!distanceToAllOriginsOk(x, y)) continue;
		riverOrigins.push_back({ x, y });
		originCount++;

		auto [finalIdx, mountainTouched, mountainPath] = propagateDownhill(idx);
		// 规则：山体内这一段只要碰到地图中心区域，整条河也作废——跟流出山体之后那段用
		// 同一条inCenterRegion判定，这里没有"kept白名单"这种机制(propagateDownhill本身
		// 不区分成功/失败)，所以直接在这里扫一遍path，碰到就撤销mountainTouched、跳过
		// 这个起点，不往下走cutoff/continueRiverOutsideMountain那一套。
		bool mountainHitsCenter = false;
		for (int p : mountainPath) {
			if (inCenterRegion(p % width, p / width)) { mountainHitsCenter = true; break; }
		}
		if (mountainHitsCenter) {
			for (int t : mountainTouched) isRiver[t] = false;
			riverOrigins.pop_back();
			continue;
		}
		int exit = exitTarget[finalIdx];
		if (flowTarget[finalIdx] >= 0) continue; // 终点flowTarget仍然有效,说明是并入了既有河流,不是独立链路的终点,不用往下判定
		if (exit < 0) {
			// 山体内部的局部坑(exitTarget也是-1,连非山体邻居都没有)——规则：流到洼地中止,
			// 整条河作废，把这条链路刚标记的山体部分撤销。
			for (int t : mountainTouched) isRiver[t] = false;
			riverOrigins.pop_back();
			continue;
		}

		// 防止河流过陡：从山脚(mountainPath末尾)往山上(下标减小方向)逆推，只要连续两格之间
		// (更靠源头的那个减去更靠山脚的那个)高度差超过RIVER_MAX_STEP_DROP，就在那里截断——
		// 靠源头那一段(含原来的起点)整体撤销，河流起点改成截断点，只保留靠山脚、坡度更缓
		// 的那一段。
		int cutoffPos = 0; // mountainPath里真正保留下来的起始下标
		for (int p = (int)mountainPath.size() - 1; p >= 1; p--) {
			if (heights[mountainPath[p - 1]] - heights[mountainPath[p]] > RIVER_MAX_STEP_DROP) {
				cutoffPos = p;
				break;
			}
		}
		if (cutoffPos > 0) {
			for (int p = 0; p < cutoffPos; p++) isRiver[mountainPath[p]] = false;
			int newOriginIdx = mountainPath[cutoffPos];
			riverOrigins.back() = { newOriginIdx % width, newOriginIdx / width };
		}

		int ex = exit % width, ey = exit / width;
		// 初始流向不能只看出山前最后一格到出山第一格这一步——那永远是D8的8个离散方向之一，
		// 归一化后角度只能是0/45/90/135这几个值。往回倒最多4格(链路不够长就用截断之后剩下
		// 那段的起点)，从那一格直接指向出山格子，算出来的向量分量通常不再是±1，角度自然
		// 不再是离散值。
		int histLen = min(4, (int)mountainPath.size() - cutoffPos);
		int baseIdx = mountainPath[mountainPath.size() - histLen];
		int bx = baseIdx % width, by = baseIdx / width;
		float initDirX = float(ex - bx), initDirY = float(ey - by);
		// 流程计数接着"保留下来的那段山体路径"长度继续数(不是整条mountainTouched的原始
		// 长度)——过陡截断之后前面那段已经撤销，不算在"从源头流了多远"里面。
		int mountainKeptLen = (int)mountainPath.size() - cutoffPos;
		auto [reason, plainTouched] = continueRiverOutsideMountain(ex, ey, initDirX, initDirY, mountainTouched, mountainKeptLen);
		bool kept = (reason == RiverEnd::MapEdge || reason == RiverEnd::Ocean || reason == RiverEnd::OtherRiver);
		if (!kept) {
			for (int t : mountainTouched) isRiver[t] = false;
			for (int t : plainTouched) isRiver[t] = false;
			riverOrigins.pop_back();
		} else {
			keptCount++;
			// 诊断用：到底是哪几个格子接上了海——先只记下标，等最后提交完(地形/高度/水位
			// 都写进真实地图之后)再统一读出来打进日志，不在这里提前读(这时候还没commit，
			// 读到的还是没改过的原始值)。只记"碰到海"这种结束方式，其余(地图边界/并入别的
			// 河)跟这次排查的问题无关，不记。
			if (reason == RiverEnd::Ocean) oceanMouthChains.push_back(plainTouched);
		}
	}

	// 加宽河流：所有起点/碰撞/截断判定都是基于宽度1的中心线做的(hasNearbyRiver这类检测
	// 要是提前用加宽之后的格子去判断，中心线还没走开多远就会被自己的宽度碰到，逻辑会乱)，
	// 所以加宽放在最后。越往下游越宽——每个中心线格子按自己的riverDistance(这条链路从
	// 源头数起、流经了多少格)算一个专属半径，不再是所有格子统一膨胀一圈的固定宽度3。
	// 半径用欧几里得距离判断(跟河岸blur那次改法一样)，不用8邻居格数，避免同样的"方块/
	// 八边形"棱角问题；先试每200格宽度+1(对称膨胀一次两侧各加，所以是半径+0.5)。
	{
		vector<bool> widened = isRiver;
		for (int idx = 0; idx < cellCount; idx++) {
			if (!isRiver[idx]) continue;
			int x = idx % width, y = idx / width;
			float radius = RIVER_BASE_WIDEN_RADIUS + riverDistance[idx] / RIVER_WIDTH_GROW_PER_CELLS * 0.5f;
			int radiusCeil = (int)ceilf(radius);
			for (int dy = -radiusCeil; dy <= radiusCeil; dy++) {
				for (int dx = -radiusCeil; dx <= radiusCeil; dx++) {
					if (dx == 0 && dy == 0) continue;
					if (sqrtf(float(dx * dx + dy * dy)) > radius) continue;
					int nx = x + dx, ny = y + dy;
					if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
					widened[ny * width + nx] = true;
				}
			}
		}
		isRiver = move(widened);
	}

	// 河岸平滑范围也并进河流，不止是改个高度：算每个格子到(加宽+入海口兜底加宽之后)
	// 河流的距离，只算到RIVER_BLUR_RADIUS为止，这个范围内的格子直接并入isRiver——连地形类型、水面
	// 都按河流处理，不是只单独调一下高度。只调高度不改地形/不加水面的话，水面mesh的
	// 覆盖范围还是停在加宽后的河道边缘，边缘往外一格地形已经被削平滑了一截，水面却没有
	// 跟着铺过去，侧边界只是往外挪了一格，没有真正消失。falloff随距离线性减小(核心河道
	// 为1，到RIVER_BLUR_RADIUS+1格之外为0)，只用来决定高度降多少；水面统一用"原地形
	// 高度+SEA_LEVEL"这个公式，不受falloff影响，跟河道本身的水位计算方式一致。
	// 原来用BFS按"格数"算距离，8邻居每一步距离都算1，等距离线其实是个八边形/方块
	// (切比雪夫距离的那种棱角形状)，这才是"圆角锯齿"的真正来源——半径不够只会让这圈
	// 更窄，棱角本身不会消失。改成直接算每个格子到最近河流格子的欧几里得距离(真实的
	// 直线距离，不是走了几步)，等距离线才会是真正的圆弧。只在每个河流格子周围
	// (2*RIVER_BLUR_RADIUS+1)见方的局部范围内比较，不是整张地图两两比较距离，河流格子
	// 数量通常远小于全图格子数，复杂度仍然可控。
	vector<float> dropFalloff(cellCount, 1.0f); // 核心河道(加宽后)默认按满额下降
	{
		vector<float> distToRiver(cellCount, 1e30f);
		vector<int> riverCells;
		riverCells.reserve(cellCount / 8);
		for (int idx = 0; idx < cellCount; idx++) if (isRiver[idx]) riverCells.push_back(idx);
		for (int idx : riverCells) {
			int rx = idx % width, ry = idx / width;
			for (int dy = -RIVER_BLUR_RADIUS; dy <= RIVER_BLUR_RADIUS; dy++) {
				for (int dx = -RIVER_BLUR_RADIUS; dx <= RIVER_BLUR_RADIUS; dx++) {
					int nx = rx + dx, ny = ry + dy;
					if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
					float d = sqrtf(float(dx * dx + dy * dy));
					if (d > float(RIVER_BLUR_RADIUS)) continue;
					int nIdx = ny * width + nx;
					if (d < distToRiver[nIdx]) distToRiver[nIdx] = d;
				}
			}
		}
		for (int idx = 0; idx < cellCount; idx++) {
			if (isRiver[idx] || distToRiver[idx] > float(RIVER_BLUR_RADIUS)) continue;
			isRiver[idx] = true; // 平滑范围内的格子并入河流
			// 线性衰减基础上再过一次smoothstep缓动(3t^2-2t^3)，让下降速度本身也在半径
			// 边界顺滑地起止，不是一进圈就突然开始按固定速率下降。
			float t = 1.0f - distToRiver[idx] / float(RIVER_BLUR_RADIUS + 1);
			dropFalloff[idx] = t * t * (3.0f - 2.0f * t);
		}
	}

	// 诊断用：DistributeTerrain接口里setWater是只写的(没有配对的getWater回调)，提交完
	// 之后没法再读回"这个格子最后水位到底是多少"——这里单独存一份，commit循环里顺手记
	// 下来，供最后的诊断日志打印，不额外多算一次。
	vector<float> debugWaterHeight(cellCount, -9999.0f); // -9999=没有水(哨兵值,不是真实水位)
	int riverCount = 0;
	for (int idx = 0; idx < cellCount; idx++) {
		if (isRiver[idx]) {
			int x = idx % width, y = idx / width;
			// 必须在setTerrain覆盖之前读一下这个格子原来是不是"ocean"——根因就在这里：
			// 加宽/blur是纯几何扩散，不看原来是什么地形，会把边缘一圈真正的海洋格子也
			// 并进isRiver。这些格子的"原高度"是OceanTerrain自己算的海底深度(不是0)，如果
			// 水位也套河流那套"原高度+SEA_LEVEL"，算出来的水位会比海洋自己固定的-0.1更低
			// (深度越深、水位越低)，在入海口附近凹下去一块；旁边没被卷进来的真正海洋格子
			// 水位还是固定的-0.1，相对之下，海洋自己那圈贴着海岸、本来就只比水面高一点点
			// 的海岸线格子(高度0)，相对这片凹陷的水面就显得"从水里凸出来了一截"——水面凹陷
			// +海岸线凸起这两个一叠加，看起来就是河流和海洋从中间断开了。
			bool wasOcean = (getTerrain(x, y) == "ocean");
			setTerrain(x, y, GetType());
			// 第三阶段：河床比原来的地形低一截，水面盖在河床上方一点，水位跟着(已经降过的)
			// 河床高度走——相邻河流格子高度本来就不一样(沿下坡方向递减)，水位自然跟着不同，
			// 倾斜就是这样自动出来的，不用另外算坡度方向。falloff<1的那一圈(河岸平滑范围)
			// 降得比核心河道少，越往外越接近原高度，做出缓坡而不是直角。
			float originalHeight = getHeight(x, y);
			float riverBedHeight = originalHeight - RIVER_HEIGHT_DROP * dropFalloff[idx];
			setHeight(x, y, riverBedHeight);
			// 水位不是"河床高度+偏移"，是跟海洋同一个公式："原地形高度+SEA_LEVEL"——
			// 河口附近原地形(plain)高度本来就接近0，算出来的水位天然就是SEA_LEVEL，正好
			// 和海面对上，不需要再夹一下；原地形越往上游/越高，水位跟着一起抬，倾斜还是
			// 同样会自然出来(相邻格子原地形高度本来就不一样，常数偏移不影响相对高低)。
			// 水位不随falloff打折——河岸平滑范围的格子水位和核心河道用同一个公式，水面才能
			// 连续地盖过去，不会在平滑范围边缘又断一截。
			// 唯一的例外：这个格子原来就是ocean的话，水位必须沿用海洋自己那个固定值
			// (SEA_LEVEL)，不能用"原高度+SEA_LEVEL"重新算——原高度是海底深度，不是河流
			// 沿岸的地形高度，跟河流那套公式的假设(原地形高度本来接近0)完全不是一回事，
			// 套上去只会凹陷。这不是"入海口加宽"那种基于坐标范围的特殊区域，是基于这个
			// 格子真实的原始地形类型判断的，覆盖所有被加宽/blur卷入的真海洋格子。
			float waterHeight = wasOcean ? SEA_LEVEL : originalHeight + SEA_LEVEL;
			setWater(x, y, true, waterHeight);
			debugWaterHeight[idx] = waterHeight;
			// 河床比周围矮一截，只给河流格子自己铺水面的话，凹陷边缘会露出水面mesh的侧面
			// (看起来像一块水"悬浮"在坑里，能看到切面)——把8个邻居也铺上同一片水面接住这个
			// 落差，挡住侧边界。跳过已经是河流/海洋的邻居，不覆盖它们各自更准确的水位
			// (海洋优先级更高，这次river跑的时候海洋早就定好了；河流邻居轮到它自己那个
			// idx时会用自己算出来的水位覆盖回来，顺序不影响最终结果)。
			for (auto& off : neighborOffsets) {
				int nx = x + off[0], ny = y + off[1];
				if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
				string nt = getTerrain(nx, ny);
				if (nt == "river" || nt == "ocean") continue;
				setWater(nx, ny, true, waterHeight);
				debugWaterHeight[ny * width + nx] = waterHeight;
			}
			riverCount++;
		}
	}

	debugf("Log: River mountain cells=%d, max accum=%.2f, threshold=%.2f, origins=%d, kept=%d, marked river=%d.\n",
		(int)mountainIndices.size(), maxAccum, RIVER_FLOW_THRESHOLD, originCount, keptCount, riverCount);

	// debugf走OutputDebugStringA,不进UE的Forever.log,调试阶段临时再写一份到文件方便直接读——
	// 验证完效果之后会删掉这段。
	{
		std::ofstream f("C:/Workspace/UE5/Projects/Forever/Forever_UE/Saved/river_debug.log", std::ios::app);
		if (f) {
			f << "mountain cells=" << mountainIndices.size()
				<< " max accum=" << maxAccum
				<< " threshold=" << RIVER_FLOW_THRESHOLD
				<< " origins=" << originCount
				<< " kept=" << keptCount
				<< " marked river=" << riverCount << "\n";
			// 排查"河流入海口和海洋之间断开"问题的专用诊断：第一版诊断按"neighbor
			// terrain==ocean"筛选，结果一个都没打出来——因为convertTouchingOceanToRiver
			// 已经把触发判定的那个/那些海洋格子本身也改成了"river"，提交完之后它自然不再
			// 显示"ocean"，不代表没有邻居。改成不筛选，直接把链路最后一个touched格子周围
			// 一块9x9的方格(terrain+height+water)整块打出来，不管是river/ocean/plain，
			// 这样才能看到真正未被转换的海洋从哪里开始、中间到底有没有真的留着原高度的
			// 陆地、以及水位是不是在边界处突然跳了一截（接口没有getWater回调读不回已经
			// 提交的水位，ocean格子没被river动过的话水位必然是固定的SEA_LEVEL，river/被
			// 并入的格子用commit循环里顺手记下来的debugWaterHeight，没被设过水的格子打
			// "no water"）。
			for (auto& chain : oceanMouthChains) {
				if (chain.empty()) continue;
				int lastIdx = chain.back();
				int cx = lastIdx % width, cy = lastIdx / width;
				f << "  [ocean mouth chain, 9x9 grid around last cell (" << cx << "," << cy << ")]\n";
				for (int gy = cy - 4; gy <= cy + 4; gy++) {
					f << "    y=" << gy << ": ";
					for (int gx = cx - 4; gx <= cx + 4; gx++) {
						if (gx < 0 || gx >= width || gy < 0 || gy >= height) {
							f << "[out] ";
							continue;
						}
						string t = getTerrain(gx, gy);
						float h = getHeight(gx, gy);
						int gIdx = gy * width + gx;
						string waterStr;
						if (t == "ocean") waterStr = "w=" + std::to_string(SEA_LEVEL) + "(fixed)";
						else if (debugWaterHeight[gIdx] > -9000.0f) waterStr = "w=" + std::to_string(debugWaterHeight[gIdx]);
						else waterStr = "no water";
						f << "[" << t << " h=" << h << " " << waterStr << "] ";
					}
					f << "\n";
				}
			}
		}
	}
}
