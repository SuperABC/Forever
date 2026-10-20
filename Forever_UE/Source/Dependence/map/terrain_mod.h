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

// 阶段4-1:Terrain是Map域第一个补齐真正业务接口的concept,详见 Source/Dependence/map/terrain_mod.md。
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

	// width/height单位是地图元素(1元素=10m)。按优先级降序被依次调用,通过getTerrain/
	// getHeight读取当前(可能已被更高优先级地形写过的)地图状态,通过setTerrain/setHeight写入。
	// setWater独立于setTerrain,每个格子可以单独设置是否有水+水面高度(地图元素单位)——
	// 同一次DistributeTerrain调用里不同格子可以给不同水位,构造倾斜水面,见terrain_mod.md
	// "水面"一节。
	// setFineHeights(x,y,data)给单个element设置细化高度数组(data指向TERRAIN_FINE_SIZE^2个
	// float,Core拷贝一份,不持有mod的内存;data传nullptr清除)。四个角必须等于对应4个格点的
	// 粗高度;setHeight会清掉共用那个格点的4块细化数组,所以必须先setHeight再setFineHeights。
	virtual void DistributeTerrain(int width, int height,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<bool(int, int, const std::string&)>& setTerrain,
		const std::function<float(int, int)>& getHeight,
		const std::function<bool(int, int, float)>& setHeight,
		const std::function<bool(int, int, bool, float)>& setWater,
		const std::function<bool(int, int, const float*)>& setFineHeights) const = 0;

	// 地形贴图资产路径,SetupTexture()里填充。
	std::string diffusePath;

	// 法线/粗糙度贴图资产路径,SetupTexture()里填充,留空则UE侧用平的法线+中性粗糙度
	// 填充对应贴图数组槡位,见ForeverTerrainFrameworkComponent.md"近处地形混合"一节。
	std::string normalPath;
	std::string roughnessPath;

	// ---- 存档(见Source/Dependence/common/mod_state.md) ----
	// mod私有数据的存档/读档钩子，默认不存任何东西。基类里的公共声明字段由Core直接快照，
	// 这里只放mod自己的私有数据。
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	// SaveState写出数据的格式版本：改了自己SaveState的格式就加一，读档时从
	// StateReader::GetSavedVersion()拿到当时的版本自己迁移。默认0。
	virtual int GetStateVersion() const { return 0; }
	// 这个mod类用来生成唯一实例名的静态计数器——存档保存、读档后恢复，避免读档后新建的实例和
	// 恢复出来的名字撞车。没有计数器的类保持默认(返回-1、Set不做事)。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
