#pragma once

#include "map/roadnet_mod.h"


// 井字路网生成器，详见roadnet_basic.md。
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
