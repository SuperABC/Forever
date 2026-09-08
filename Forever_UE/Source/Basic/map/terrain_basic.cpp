#include "terrain_basic.h"

#include "common/utility.h"

#include <vector>
#include <cmath>
#include <algorithm>


using namespace std;

// 海洋深度衰减参数(沿离开地图中心方向,离海岸线越远越深)
#define OCEAN_DEPTH_RATE 0.05f
#define OCEAN_SLOPE_JITTER 0.01f
#define OCEAN_MAX_DEPTH 60.0f

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

// 八方向邻居偏移量
static const int neighborOffsets[8][2] = { {1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1} };

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
	return 1.0f;
}

void OceanTerrain::SetupTexture() {
	diffusePath = "/Game/Asset/Textures/Terrain/PlainDiffuse.PlainDiffuse";
	waterHeight = { true, -0.1f };
}

void OceanTerrain::DistributeTerrain(int width, int height,
	const function<string(int, int)>& getTerrain, const function<bool(int, int, string)>& setTerrain,
	const function<float(int, int)>& getHeight, const function<bool(int, int, float)>& setHeight) const {

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

	// 写回全局地图高度
	for (int x = 0; x < width; x++)
		for (int y = 0; y < height; y++)
			if (isOcean[x][y])
				setHeight(x, y, oceanHeight[x][y]);
}

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
	return 0.9f;
}

void MountainTerrain::SetupTexture() {
	diffusePath = "/Game/Asset/Textures/Terrain/RockDiffuse.RockDiffuse";
}

void MountainTerrain::DistributeTerrain(int width, int height,
	const function<string(int, int)>& getTerrain, const function<bool(int, int, string)>& setTerrain,
	const function<float(int, int)>& getHeight, const function<bool(int, int, float)>& setHeight) const {

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
