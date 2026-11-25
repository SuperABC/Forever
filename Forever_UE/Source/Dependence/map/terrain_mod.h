#pragma once

#include "common/mod_test.h"
#include "common/mod_state.h"

#include <string>
#include <cstdint>
#include <utility>
#include <functional>
#include <vector>

// 细化高度数组:一个element(10m)每边细分成TERRAIN_FINE_SUBDIV段,含两端共TERRAIN_FINE_SIZE个
// 采样点,数组按行主序存TERRAIN_FINE_SIZE*TERRAIN_FINE_SIZE个高度,data[j*SIZE+i]对应点
// (x+i/SUBDIV, y+j/SUBDIV)。16正好等于UE侧最细一级LOD每个element的网格段数,见terrain_mod.md。
#define TERRAIN_FINE_SUBDIV 16
#define TERRAIN_FINE_SIZE (TERRAIN_FINE_SUBDIV + 1)

// 地表材质声明(设计见terrain_mod.md"地表材质")：一个地形最多2个"层"(UE资产路径)，selector决定两层怎么混合——
//   None       只用layers[0]；
//   Slope      按坡度(1-法线z)在range0..range1之间从layers[0](平缓)渐变到layers[1](陡峭)；
//   WaterDepth 按到水面的深度(米，正=在水下)在range0..range1之间从layers[0]渐变到layers[1]。
// 层按name去重(不同地形mod可以共用同一个name，贴图只加载一份)；normal必填，orm(R=AO,G=粗糙度,B=金属度)和roughness(单独的灰度图)二选一。
// 注意：层贴图必须先过Tools/TerrainMaterial/prepare_terrain_textures.py(1024/不流送/BC7/BC5)，否则该层渲染成灰。
// pak.bat打包时自动跑(mod的贴图放插件Content/TerrainLayers/，名字后缀_B/_N/_ORM)；编辑器/PIE里属性不对会在内存里自动改并打Warning(不存盘)。
struct TerrainMaterialLayer {
	std::string name;
	std::string diffuse, normal, orm, roughness;
	float tileMeters = 4.f;   // 贴图一次平铺覆盖的世界米数
	bool triplanar = false;   // 陡峭层近处用三向投影，避免竖直面被拉伸
};
enum class TerrainMaterialSelector : int { None = 0, Slope = 1, WaterDepth = 2 };
struct TerrainMaterialSpec {
	bool defined = false;
	std::vector<TerrainMaterialLayer> layers;
	TerrainMaterialSelector selector = TerrainMaterialSelector::None;
	float range0 = 0.f, range1 = 1.f;
};

// 设计见terrain_mod.md。
class TerrainMod {
public:
	TerrainMod() = default;
	virtual ~TerrainMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 单元测试：print输出关键信息，返回是否通过。没有深度测试的mod用FOREVER_MOD_GENERIC_UNIT_TEST(见common/mod_test.h)。
	virtual bool UnitTest(TestPrintFn print) = 0;
	// 构建优先级,越高越先执行DistributeTerrain(后执行的可以覆盖先执行的地形类型/高度)。
	virtual float GetPriority() const = 0;

	// 填充地图界面颜色(SetMapColor)等外观数据,在DistributeTerrain之前调用一次。材质声明用SetMaterial。
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

	// 地表材质声明，SetupTexture()里用SetMaterial填充；没声明的地形由渲染层用默认地表材质兜底。
	TerrainMaterialSpec materialSpec;
	void SetMaterial(const TerrainMaterialLayer& only) {
		materialSpec = TerrainMaterialSpec();
		materialSpec.defined = true;
		materialSpec.layers = { only };
	}
	void SetMaterial(const TerrainMaterialLayer& a, const TerrainMaterialLayer& b, TerrainMaterialSelector selector, float range0, float range1) {
		materialSpec = TerrainMaterialSpec();
		materialSpec.defined = true;
		materialSpec.layers = { a, b };
		materialSpec.selector = selector;
		materialSpec.range0 = range0;
		materialSpec.range1 = range1;
	}

	// 小地图/地图界面绘制本地形用的颜色(0~255)，SetupTexture()里填充；hasMapColor为false则界面按名字哈希出一个颜色。
	bool hasMapColor = false;
	uint8_t mapColorR = 0, mapColorG = 0, mapColorB = 0;
	void SetMapColor(uint8_t r, uint8_t g, uint8_t b) { hasMapColor = true; mapColorR = r; mapColorG = g; mapColorB = b; }

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
