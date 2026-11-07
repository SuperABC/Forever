#pragma once

#include "common/mod_state.h"

#include <string>
#include <utility>
#include <functional>

// 细化高度数组:一个element(10m)每边细分成TERRAIN_FINE_SUBDIV段,含两端共TERRAIN_FINE_SIZE个
// 采样点,数组按行主序存TERRAIN_FINE_SIZE*TERRAIN_FINE_SIZE个高度,data[j*SIZE+i]对应点
// (x+i/SUBDIV, y+j/SUBDIV)。16正好等于UE侧最细一级LOD每个element的网格段数,见terrain_mod.md。
#define TERRAIN_FINE_SUBDIV 16
#define TERRAIN_FINE_SIZE (TERRAIN_FINE_SUBDIV + 1)

// 设计见terrain_mod.md。
class TerrainMod {
public:
	TerrainMod() : diffusePath() {}
	virtual ~TerrainMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 构建优先级,越高越先执行DistributeTerrain(后执行的可以覆盖先执行的地形类型/高度)。
	virtual float GetPriority() const = 0;

	// 填充diffusePath等贴图路径,在DistributeTerrain之前调用一次。
	virtual void SetupTexture() = 0;

	// width/height单位是地图元素(1元素=10m),按优先级降序被依次调用,通过回调读写地图。
	// 约束:必须先setHeight再setFineHeights(setHeight会清掉共用格点的细化数组),
	// 其余见terrain_mod.md。
	virtual void DistributeTerrain(int width, int height,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<bool(int, int, const std::string&)>& setTerrain,
		const std::function<float(int, int)>& getHeight,
		const std::function<bool(int, int, float)>& setHeight,
		const std::function<bool(int, int, bool, float)>& setWater,
		const std::function<bool(int, int, const float*)>& setFineHeights) const = 0;

	// 地形贴图资产路径,SetupTexture()里填充。
	std::string diffusePath;

	// 法线/粗糙度贴图资产路径,SetupTexture()里填充,留空则UE侧用平法线+中性粗糙度。
	std::string normalPath;
	std::string roughnessPath;

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
