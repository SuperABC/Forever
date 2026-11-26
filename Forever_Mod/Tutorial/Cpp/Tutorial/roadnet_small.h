// 自动生成(Tools/MapEmbed/embed_map.py roadnet small.roadnet --id roadnet_small --class SmallRoadnet)，勿手改：
// 以 Source/Basic/map/roadnet_file.* 为模板，数据内嵌在 roadnet_small_data.inc，不读文件。改 roadnet_file 后重新生成。

#pragma once

#include "map/roadnet_mod.h"


// 小地图路网：small.roadnet 内嵌进代码的roadnet mod，见roadnet_small.md。
class SmallRoadnet : public RoadnetMod {
public:
	FOREVER_MOD_GENERIC_UNIT_TEST("roadnet")
	explicit SmallRoadnet(const std::string& args = "");
	virtual ~SmallRoadnet();

	static const char* GetId();
	virtual const char* GetType() const override;
	virtual const char* GetName() override;

	virtual void DistributeRoadnet(int width, int height,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<bool(int, int)>& isConstruction,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		const std::function<float(float, float)>& getHeight,
		int nodeStaticCount) override;

	// 行道树/路灯：按每条道路的treeTag/lampTag(.roadnet的output.roads[])选资产，tag表见roadnet_file.cpp的TreeAssetFor/LampAssetFor。
	virtual void LayoutTrees(const std::vector<Road*>& hostRoads) override;
	virtual void LayoutLamps(const std::vector<Road*>& hostRoads) override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	using RowStyleFunc = std::function<bool(const std::string& tag, float& spacing, float& sidewalkRatio)>;
	using RowEmitter = std::function<void(const std::string& tag, int side, float x0, float y0, float x1, float y1, int num, float angleDegrees)>;
	// 沿每条有tag的路两侧的人行道排点，几何和roadnet_checkboard.cpp的LayoutRows一致(路口缩进、开口缺口、直路成排)，
	// 额外支持曲线路：逐点沿弧排，每点一个num=1的"排"。tags下标=mod->roads下标，hostRoads是Core的副本，前面和mod->roads一一对应。
	void LayoutRows(const std::vector<Road*>& hostRoads, const std::vector<std::string>& tags, const RowStyleFunc& style, const RowEmitter& emit);

	static int count;
	int id;
	std::string name;
	std::string path;
	// 每条道路的行道树/路灯tag(空=没有)，下标和roads一致。
	std::vector<std::string> treeTags, lampTags;
};
