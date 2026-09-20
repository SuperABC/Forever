#pragma once

#include "map/roadnet_mod.h"


// 井字路网，照抄老工程E:\Projects\Forever_UE\Source\Basic\map\roadnet_basic.h的JingRoadnet
// 算法迁移，改动点见roadnet_basic.md（去掉隧道逻辑、道路高度固定0、统一车道配置、
// 去掉挡住lot细分的return、mesh/unit指向default_1_1.uasset）。
class JingRoadnet : public RoadnetMod {
public:
	JingRoadnet();
	virtual ~JingRoadnet();

	static const char* GetId();
	virtual const char* GetType() const override;
	virtual const char* GetName() override;

	virtual void DistributeRoadnet(int width, int height,
		const std::function<std::string(int, int)>& getTerrain,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		int nodeStaticCount) override;

private:
	static int count;
	int id;
	std::string name;
};
