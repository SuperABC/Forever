#pragma once

#include "map/roadnet_mod.h"


// 读文件路网:读MapEditor导出的.roadnet(JSON)里的output段。详见roadnet_file.md。
class FileRoadnet : public RoadnetMod {
public:
	FOREVER_MOD_GENERIC_UNIT_TEST("roadnet")
	// args是config.json里mod id后面的参数串："--path C:/maps/city.roadnet"或"--path=C:/maps/city.roadnet"。
	explicit FileRoadnet(const std::string& args = "");
	virtual ~FileRoadnet();

	static const char* GetId();
	virtual const char* GetType() const override;
	virtual const char* GetName() override;

	virtual void DistributeRoadnet(int width, int height,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		const std::function<float(float, float)>& getHeight,
		int nodeStaticCount) override;

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
