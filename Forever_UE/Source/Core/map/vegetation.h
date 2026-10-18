#pragma once

#include "map/feature_spec.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <utility>


// 一棵展开好的树：位置(地图element单位，世界坐标)、绕Z轴旋转(弧度)、最终缩放、mesh下标
// (指向Vegetation::GetMeshPaths())。高度不在这里存——UE层按Map::SampleHeight现场取。
struct TreeInstance {
	int id = 0;
	float x = 0.f, y = 0.f;
	float yaw = 0.f;
	float scale = 1.f;
	int meshIndex = 0;
};

// 世界坐标(地图element单位)下的一排树/一片树——Core内部用，由宿主参考系里的TreeLineSpec/
// TreeAreaSpec换算而来(见ExpandRequests)。TreeAreaRequest::quad中心是世界坐标、尺寸沿宿主局部
// 轴向，ExpandArea再绕中心旋转。
struct TreeLineRequest {
	float fromX = 0.f, fromY = 0.f;
	float toX = 0.f, toY = 0.f;
	int num = 0;
	float scale = 1.f;
	std::string meshPath;
};

struct TreeAreaRequest {
	Quad quad;
	int num = 0;
	float scale = 1.f;
	std::string meshPath;
};

// 宿主(路网/园区/建筑)的参考系：toLocal把宿主参考系里的PointParams换算成宿主局部坐标(轴向沿
// 宿主自身，园区/建筑按朝向旋转过)，localToWorld把局部坐标换成世界坐标，rotation是宿主局部轴相对
// 世界的旋转(弧度，TreeArea的矩形要跟着转)。
struct TreeFrame {
	std::function<std::pair<float, float>(const PointParams&)> toLocal;
	std::function<std::pair<float, float>(float, float)> localToWorld;
	float rotation = 0.f;
};

// 全图植被的统一存储：把mod提交的TreeRequests(单棵/一排/一片，见feature_spec.h)按宿主参考系换算后
// 展开成一棵棵TreeInstance，随机旋转/随机缩放的规则只在这里实现一份。由Map持有，UE层的
// UForeverVegetationFrameworkComponent一次性读出来按mesh分组实例化渲染。详见vegetation.md。
class Vegetation {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	// 返回false表示这个位置不能种树(水面/挖洞等)，由Map提供。
	using PlaceFilter = std::function<bool(float, float)>;

	// 把一个宿主的全部请求(singles/lines/areas)按frame换算成世界坐标后逐个展开，返回实际种下的棵数。
	int ExpandRequests(const TreeRequests& requests, const TreeFrame& frame, const PlaceFilter& filter);

	// 去重登记mesh路径，返回它在GetMeshPaths()里的下标。
	int RegisterMesh(const std::string& meshPath);

	// 世界坐标(x,y)种一棵，被过滤掉返回0。
	int ExpandSingle(float x, float y, float scale, const std::string& meshPath, const PlaceFilter& filter);
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
