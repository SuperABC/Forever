#include "vegetation.h"

#include "common/utility.h"

#include <algorithm>
#include <cmath>


using namespace std;

namespace {
	const float kTwoPi = 6.28318530718f;
	// 随机缩放：均值1、σ=0.1的高斯，夹到[0.8,1.2](正好±2σ)。
	const float kScaleSigma = 0.1f;
	const float kScaleMin = 0.8f;
	const float kScaleMax = 1.2f;

	float RandomUnit() {
		return GetRandom(100000) / 100000.f;
	}
}

int Vegetation::RegisterMesh(const string& meshPath) {
	auto found = meshIndices.find(meshPath);
	if (found != meshIndices.end()) return found->second;
	int index = static_cast<int>(meshPaths.size());
	meshPaths.push_back(meshPath);
	meshIndices[meshPath] = index;
	return index;
}

int Vegetation::ExpandLine(const TreeLineRequest& request, const PlaceFilter& filter) {
	if (request.num <= 0 || request.meshPath.empty()) return 0;
	int meshIndex = RegisterMesh(request.meshPath);
	int planted = 0;
	for (int i = 0; i < request.num; i++) {
		float t = request.num == 1 ? 0.5f : float(i) / float(request.num - 1);
		float x = request.fromX + (request.toX - request.fromX) * t;
		float y = request.fromY + (request.toY - request.fromY) * t;
		if (filter && !filter(x, y)) continue;
		AddTree(x, y, request.scale, meshIndex);
		planted++;
	}
	return planted;
}

int Vegetation::ExpandArea(const TreeAreaRequest& request, float rotation, const PlaceFilter& filter) {
	if (request.num <= 0 || request.meshPath.empty()) return 0;
	int meshIndex = RegisterMesh(request.meshPath);
	const Quad& quad = request.quad;
	float c = cosf(rotation), s = sinf(rotation);
	int planted = 0;
	for (int i = 0; i < request.num; i++) {
		// 局部坐标(原点在quad中心、沿quad自己的轴向)均匀采样，再绕中心旋转——和
		// Building::LocalToWorld同一套cos/sin写法。
		float relX = (RandomUnit() - 0.5f) * quad.GetSizeX();
		float relY = (RandomUnit() - 0.5f) * quad.GetSizeY();
		float x = quad.GetPosX() + relX * c - relY * s;
		float y = quad.GetPosY() + relX * s + relY * c;
		if (filter && !filter(x, y)) continue;
		AddTree(x, y, request.scale, meshIndex);
		planted++;
	}
	return planted;
}

const vector<string>& Vegetation::GetMeshPaths() const {
	return meshPaths;
}

const vector<TreeInstance>& Vegetation::GetTrees() const {
	return trees;
}

void Vegetation::AddTree(float x, float y, float baseScale, int meshIndex) {
	TreeInstance tree;
	tree.id = nextId++;
	tree.x = x;
	tree.y = y;
	tree.yaw = RandomUnit() * kTwoPi;
	tree.scale = baseScale * clamp(GetRandomNormal(1.f, kScaleSigma), kScaleMin, kScaleMax);
	tree.meshIndex = meshIndex;
	trees.push_back(tree);
}
