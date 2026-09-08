#pragma once

#include "terrain.h"
#include "map/terrain_factory.h"
#include "map/geometry.h"
#include "common/loader.h"

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>

// 10m*10m地图元素。当前只有Terrain域需要的字段;zone/building字段等Zone/Building阶段
// 迁移时再补。hatches现在就接好(挖洞用),但在Roadnet/Building迁移前始终为空,详见map.md。
struct Element {
	std::string terrain = "plain";
	float height = 0.f;
	std::pair<bool, float> water{ false, 0.f };
	std::vector<std::pair<Quad, float>> hatches;
};

// 阶段4-1 Map域的Map(聚合)雏形:只承担Terrain域需要的职责(尺寸、Element存储、
// TerrainFactory归属、地形分发、plain->construction晋升规则)。Zone/Block/Component/
// Room/Building/Roadnet迁移时会在这个类基础上继续扩展,不是最终形态,详见map.md。
class Map {
public:
	Map(int width, int height);
	~Map();

	// 用ModLoader发现/注册config.json配置的terrain mod dll,假定调用方已经完成过一次
	// Config::ReadConfig。
	void InitTerrains();

	// 按GetPriority()降序对所有已注册地形执行DistributeTerrain,再执行plain/construction
	// 的3x3晋升规则,最后重建terrainTextures索引表。
	void InitContents();

	std::pair<int, int> GetSize() const;

	std::string GetTerrain(int x, int y) const;
	bool SetTerrain(int x, int y, const std::string& terrain, std::pair<bool, float> water = { false, 0.f });
	float GetHeight(int x, int y) const;
	bool SetHeight(int x, int y, float height);
	std::pair<bool, float> GetWater(int x, int y) const;
	const std::vector<std::pair<Quad, float>>& GetHatches(int x, int y) const;

	// 自动分发到所有与q重叠的element(按q的旋转AABB计算重叠范围),和旧工程Map::AddHatch同语义。
	void AddHatch(Quad q, float rotation);

	// 地形类型 -> {纹理数组槽位索引, diffuse资产路径}
	const std::unordered_map<std::string, std::pair<int, std::string>>& GetTerrainTextures() const;

private:
	bool CheckXY(int x, int y) const;
	Element& At(int x, int y);
	const Element& At(int x, int y) const;

	int width;
	int height;
	std::vector<Element> elements; // 行主序扁平数组(y*width+x),取代旧工程Chunk分块

	TerrainFactory terrainFactory;

	// ModLoader持有mod dll句柄,必须活得至少和terrainFactory一样长——terrainFactory里存的
	// creator/deleter函数指针指向这些dll的代码段,一旦ModLoader析构FreeLibrary掉dll,
	// 这些指针就悬空了(实测复现:CreateTerrain调用creator()时access violation)。所以这里
	// 是Map的成员,不是InitTerrains()内的局部变量。
	ModLoader modLoader;

	std::unordered_map<std::string, std::pair<int, std::string>> terrainTextures;
};
