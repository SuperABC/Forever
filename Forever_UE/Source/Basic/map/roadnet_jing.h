#pragma once

#include "map/roadnet_mod.h"


// 井字路网生成器，详见roadnet_jing.md。
class JingRoadnet : public RoadnetMod {
public:
	// args是config.json里mod id后面的参数串："--length 3"(四个臂向外最多伸出几个lot，<0或不填=不限制)。
	explicit JingRoadnet(const std::string& args = "");
	virtual ~JingRoadnet();

	static const char* GetId();
	virtual const char* GetType() const override;
	virtual const char* GetName() override;

	virtual void DistributeRoadnet(int width, int height,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		const std::function<float(float, float)>& getHeight,
		int nodeStaticCount) override;
	// 道路两侧人行道上的行道树，避开开口，见roadnet_jing.md"行道树"。
	virtual void LayoutTrees(const std::vector<Road*>& hostRoads) override;
	// 大路两侧的两排路灯，位置在行道树外侧，见roadnet_jing.md"路灯"。
	virtual void LayoutLamps(const std::vector<Road*>& hostRoads) override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	// 行道树/路灯共用的"沿路两排"几何：每条地面大路每侧一排，避开路口和开口。emit每次收到一段
	// 连续的位置：side(0/1)、起止世界坐标、盏数(含两端)、沿路方向角(度，数学角)。
	using RowEmitter = std::function<void(int side, float x0, float y0, float x1, float y1, int count, float roadAngleDegrees)>;
	void LayoutRows(const std::vector<Road*>& hostRoads, float spacing, float sidewalkRatio, const RowEmitter& emit);

	static int count;
	int id;
	std::string name;
	// 每个臂向外最多伸出的lot数(紧邻中心的那块算第1个)，<0=不限制。
	int length = -1;
};
