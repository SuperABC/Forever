#pragma once

#include <string>
#include <utility>
#include <functional>


// 阶段4-1:Terrain是Map域第一个补齐真正业务接口的concept,详见 Source/Dependence/map/terrain_mod.md。
class TerrainMod {
public:
	TerrainMod() : diffusePath(), waterHeight(false, 0.f) {}
	virtual ~TerrainMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 构建优先级,越高越先执行DistributeTerrain(后执行的可以覆盖先执行的地形类型/高度)。
	virtual float GetPriority() const = 0;

	// 填充diffusePath/waterHeight,在DistributeTerrain之前调用一次。
	virtual void SetupTexture() = 0;

	// width/height单位是地图元素(1元素=10m)。按优先级降序被依次调用,通过getTerrain/
	// getHeight读取当前(可能已被更高优先级地形写过的)地图状态,通过setTerrain/setHeight写入。
	virtual void DistributeTerrain(int width, int height,
		const std::function<std::string(int, int)>& getTerrain,
		const std::function<bool(int, int, std::string)>& setTerrain,
		const std::function<float(int, int)>& getHeight,
		const std::function<bool(int, int, float)>& setHeight) const = 0;

	// 地形贴图资产路径,SetupTexture()里填充。
	std::string diffusePath;

	// {是否有水面, 水面高度(地图元素单位)},SetupTexture()里填充,默认无水面。
	std::pair<bool, float> waterHeight;
};
