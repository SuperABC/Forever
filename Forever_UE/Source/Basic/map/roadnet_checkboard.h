#pragma once

#include "map/roadnet_mod.h"

#include <string>


// 棋盘格路网生成器，详见roadnet_checkboard.md。
class CheckboardRoadnet : public RoadnetMod {
public:
	// args是config.json里mod id后面的参数串："--merge 0.2 --bridge 0.5 --min 24 --max 48"，解析不到的用默认值。
	explicit CheckboardRoadnet(const std::string& args);
	virtual ~CheckboardRoadnet();

	static const char* GetId();
	virtual const char* GetType() const override;
	virtual const char* GetName() override;

	virtual void DistributeRoadnet(int width, int height,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		const std::function<float(float, float)>& getHeight,
		int nodeStaticCount) override;

	// 道路两侧人行道上的行道树/路灯，避开路口和开口(和井字路网同一套几何，代码是复制过来的)。
	virtual void LayoutTrees(const std::vector<Road*>& hostRoads) override;
	virtual void LayoutLamps(const std::vector<Road*>& hostRoads) override;

	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }

private:
	using RowEmitter = std::function<void(int side, float x0, float y0, float x1, float y1, int count, float roadAngleDegrees)>;
	void LayoutRows(const std::vector<Road*>& hostRoads, float spacing, float sidewalkRatio, const RowEmitter& emit);

	static int count;
	int id;
	std::string name;

	// 四个格子合成大lot的概率、相邻横/竖路间距的最小/最大整数距离(地图单位，1=10m)。
	float mergeProbability = 0.2f;
	// 只跨河又没有lot相邻的路的保留概率(没被保留的仍会按连通必需性补回)。
	float bridgeProbability = 0.5f;
	int minSpacing = 24;
	int maxSpacing = 48;
};
