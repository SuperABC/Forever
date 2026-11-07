#pragma once

#include "map/feature_spec.h"
#include "map/vegetation.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>


// 一盏展开好的路灯：位置(地图element单位，世界坐标)、绕Z轴旋转(弧度，已叠加宿主旋转)、蓝图下标
// (指向Lamps::GetBlueprintPaths())。高度不在这里存——UE层按Map::SampleHeight现场取。
struct LampInstance {
	int id = 0;
	float x = 0.f, y = 0.f;
	float yaw = 0.f;
	int blueprintIndex = 0;
};

// 全图路灯的统一存储，详见lamp.md
class Lamps {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	// 返回false表示这个位置不能放灯(水面/挖洞等)，由Map提供。
	using PlaceFilter = std::function<bool(float, float)>;

	// 把一个宿主的全部请求按frame换算成世界坐标后逐个展开，返回实际放下的盏数。frame沿用植被的TreeFrame。
	int ExpandRequests(const LampRequests& requests, const TreeFrame& frame, const PlaceFilter& filter);

	// 去重登记蓝图路径，返回它在GetBlueprintPaths()里的下标。
	int RegisterBlueprint(const std::string& blueprintPath);

	// 世界坐标(x,y)放一盏，yaw是弧度(世界朝向)，被过滤掉返回0。
	int ExpandSingle(float x, float y, float yaw, const std::string& blueprintPath, const PlaceFilter& filter);
	// 沿线段均匀展开(含两端，num==1时取中点)，返回实际放下的盏数。
	int ExpandLine(float fromX, float fromY, float toX, float toY, int num, float yaw,
		const std::string& blueprintPath, const PlaceFilter& filter);
	// 在quad局部范围内均匀随机采样，再绕quad中心旋转rotation(弧度)，返回实际放下的盏数。
	int ExpandArea(const Quad& quad, float rotation, int num, float yaw,
		const std::string& blueprintPath, const PlaceFilter& filter);

	const std::vector<std::string>& GetBlueprintPaths() const;
	const std::vector<LampInstance>& GetLamps() const;

private:
	void AddLamp(float x, float y, float yaw, int blueprintIndex);

	std::vector<std::string> blueprintPaths;
	std::unordered_map<std::string, int> blueprintIndices;
	std::vector<LampInstance> lamps;
	int nextId = 0;
};
