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
	// 道路两侧人行道上的行道树，避开开口，见roadnet_basic.md"行道树"。
	virtual void LayoutTrees(const std::vector<Road*>& hostRoads) override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
