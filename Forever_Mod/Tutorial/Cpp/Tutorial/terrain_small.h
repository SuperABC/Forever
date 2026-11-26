// 自动生成(Tools/MapEmbed/embed_map.py terrain small.terrain --id terrain_small --class SmallTerrain)，勿手改：
// 以 Source/Basic/map/terrain_file.* 为模板，数据内嵌在 terrain_small_data.inc，不读文件。改 terrain_file 后重新生成。

#pragma once

#include "map/terrain_mod.h"

#include <string>


// 小地图地形：small.terrain 内嵌进代码的terrain mod，见terrain_small.md。
class SmallTerrain : public TerrainMod {
public:
	FOREVER_MOD_GENERIC_UNIT_TEST("terrain")
	SmallTerrain(const std::string& args);
	virtual ~SmallTerrain();

	static const char* GetId();
	virtual const char* GetType() const override;
	virtual const char* GetName() override;
	virtual float GetPriority() const override;
	virtual void SetupTexture() override;
	virtual void DistributeTerrain(int width, int height,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<bool(int, int, const std::string&)>& setTerrain,
		const std::function<float(int, int)>& getHeight,
		const std::function<bool(int, int, float)>& setHeight,
		const std::function<bool(int, int, bool, float)>& setWater,
		const std::function<bool(int, int, const float*)>& setFineHeights) const override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
	std::string path;
};
