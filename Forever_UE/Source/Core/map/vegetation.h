#pragma once

#include "map/vegetation_spec.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>


// 一棵展开好的树：位置(地图element单位，世界坐标)、绕Z轴旋转(弧度)、最终缩放、mesh下标
// (指向Vegetation::GetMeshPaths())。高度不在这里存——UE层按Map::SampleHeight现场取。
struct TreeInstance {
	int id = 0;
	float x = 0.f, y = 0.f;
	float yaw = 0.f;
	float scale = 1.f;
	int meshIndex = 0;
};

// 全图植被的统一存储：把mod提交的TreeLineRequest/TreeAreaRequest展开成一棵棵TreeInstance，
// 随机旋转/随机缩放的规则只在这里实现一份。由Map持有，UE层的
// UForeverVegetationFrameworkComponent一次性读出来按mesh分组实例化渲染。请求结构体在Dependence的map/vegetation_spec.h。详见vegetation.md。
class Vegetation {
public:
	// 返回false表示这个位置不能种树(水面/挖洞等)，由Map提供。
	using PlaceFilter = std::function<bool(float, float)>;

	// 去重登记mesh路径，返回它在GetMeshPaths()里的下标。
	int RegisterMesh(const std::string& meshPath);

	// 沿线段均匀展开(含两端，num==1时取中点)，返回实际种下的棵数。
	int ExpandLine(const TreeLineRequest& request, const PlaceFilter& filter);
	// 在request.quad局部范围内均匀随机采样，再绕quad中心旋转rotation(弧度)，返回实际种下的棵数。
	int ExpandArea(const TreeAreaRequest& request, float rotation, const PlaceFilter& filter);

	const std::vector<std::string>& GetMeshPaths() const;
	const std::vector<TreeInstance>& GetTrees() const;

private:
	void AddTree(float x, float y, float baseScale, int meshIndex);

	std::vector<std::string> meshPaths;
	std::unordered_map<std::string, int> meshIndices;
	std::vector<TreeInstance> trees;
	int nextId = 0;
};
