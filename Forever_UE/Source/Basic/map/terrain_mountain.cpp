#include "terrain_mountain.h"

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
// MountainTerrain：随机游走生成若干条山脊多段线，沿脊线衰减+侧向BFS扩散生成高度。
// 详见terrain_mountain.md"MountainTerrain"一节。
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
	return "terrain_mountain";
}

const char* MountainTerrain::GetType() const {
	return "terrain_mountain";
}

const char* MountainTerrain::GetName() {
	name = "山区地形" + to_string(id);
	return name.data();
}

float MountainTerrain::GetPriority() const {
	// 必须高于OceanTerrain(0.9)：山先跑，海岸线雕刻才能看到并绕开山体。
	return 0.95f;
}

void MountainTerrain::SetupTexture() {
	diffusePath = "/Game/Asset/Terrain/Rock/RockDiffuse.RockDiffuse";
	normalPath = "/Game/Asset/Terrain/Rock/RockNormal.RockNormal";
	roughnessPath = "/Game/Asset/Terrain/Rock/RockRoughness.RockRoughness";
}

void MountainTerrain::DistributeTerrain(int width, int height,
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

		// 山脊高度沿走向递减到0就停止：山脊在这一端到此为止，更远的脊格不再生成(否则速率随机游走
		// 到负数时会从0重新爬起来，在主山外面长出又小又矮的孤立山包)。lastRidgeIdx/firstRidgeIdx是
		// 保留下来的脊格下标范围，BFS种子只从这个范围里的脊格出发。
		int firstRidgeIdx = 0, lastRidgeIdx = static_cast<int>(ridgeCells.size()) - 1;
		float forwardHeight = peakHeight, forwardRate = ALONG_BASE_RATE;
		for (int ridgeIdx = peakIndex + 1; ridgeIdx < static_cast<int>(ridgeCells.size()); ++ridgeIdx) {
			forwardRate = clampf(forwardRate + randf(-ALONG_RATE_JITTER, ALONG_RATE_JITTER), ALONG_RATE_MIN, ALONG_RATE_MAX);
			if (forwardHeight - forwardRate <= 0.0f) { lastRidgeIdx = ridgeIdx - 1; break; }
			forwardHeight = clampf(forwardHeight - forwardRate, 0.0f, peakHeight);
			auto [localX, localY] = ridgeCells[ridgeIdx];
			localHeight[localX][localY] = forwardHeight;
			isProcessed[localX][localY] = true;
		}

		float backwardHeight = peakHeight, backwardRate = ALONG_BASE_RATE;
		for (int ridgeIdx = peakIndex - 1; ridgeIdx >= 0; --ridgeIdx) {
			backwardRate = clampf(backwardRate + randf(-ALONG_RATE_JITTER, ALONG_RATE_JITTER), ALONG_RATE_MIN, ALONG_RATE_MAX);
			if (backwardHeight - backwardRate <= 0.0f) { firstRidgeIdx = ridgeIdx + 1; break; }
			backwardHeight = clampf(backwardHeight - backwardRate, 0.0f, peakHeight);
			auto [localX, localY] = ridgeCells[ridgeIdx];
			localHeight[localX][localY] = backwardHeight;
			isProcessed[localX][localY] = true;
		}

		// BFS 初始化,设置紧邻山脊格子的侧向坡度并加入第一圈
		vector<pair<int, int>> currentFront;
		for (int ridgeIdx = firstRidgeIdx; ridgeIdx <= lastRidgeIdx; ++ridgeIdx) {
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
