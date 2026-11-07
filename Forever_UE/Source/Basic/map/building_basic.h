#pragma once

#include "map/building_mod.h"


// Building域默认内容，各类型的设计见building_basic.md。RandomAcreage/GetAcreageMin/
// GetAcreageMax/GetPower/Assign都是不需要实例的static方法(通过BuildingFactory::RegisterBuilding
// 注册)。
class ResidenceBuilding : public BuildingMod {
public:
	// 构造函数不设footprint/楼层等，这些要等Layout()拿到落地上下文才能定。
	ResidenceBuilding();

	static const char* GetId() { return "building_residence"; }
	virtual const char* GetType() const override { return "building_residence"; }
	virtual const char* GetName() override;

	// direction==-1(FillRemainder落地)时从boundaryRoads随机挑一个有边界路的方向回写，见building_basic.md。
	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;

	// 不用显式占位，空实现。
	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	static float RandomAcreage();
	static float GetAcreageMin();
	static float GetAcreageMax();
	static float GetPower(AREA_TYPE area);

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

// 商店建筑。
class ShopBuilding : public BuildingMod {
public:
	ShopBuilding();

	static const char* GetId() { return "building_shop"; }
	virtual const char* GetType() const override { return "building_shop"; }
	virtual const char* GetName() override;

	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;

	// 不用显式占位，走权重CDF随机填充路径(空实现)。
	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	static float RandomAcreage();
	static float GetAcreageMin();
	static float GetAcreageMax();
	static float GetPower(AREA_TYPE area);

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

// 工厂建筑。
class FactoryBuilding : public BuildingMod {
public:
	FactoryBuilding();

	static const char* GetId() { return "building_factory"; }
	virtual const char* GetType() const override { return "building_factory"; }
	virtual const char* GetName() override;

	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	static float RandomAcreage();
	static float GetAcreageMin();
	static float GetAcreageMax();
	static float GetPower(AREA_TYPE area);

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

// 火车站，全图唯一一个，承载StationMod"station_train"。公交站不走Building/Lot路径，见
// station_basic.h的BusStation::AssignRoads。
class TrainStationBuilding : public BuildingMod {
public:
	TrainStationBuilding();

	static const char* GetId() { return "building_train_station"; }
	virtual const char* GetType() const override { return "building_train_station"; }
	virtual const char* GetName() override;

	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	static float RandomAcreage();
	static float GetAcreageMin();
	static float GetAcreageMax();
	static float GetPower(AREA_TYPE area); // 恒0，不参与CDF

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

// 机场，全图唯一一个，承载StationMod"station_air"。
class AirportBuilding : public BuildingMod {
public:
	AirportBuilding();

	static const char* GetId() { return "building_airport"; }
	virtual const char* GetType() const override { return "building_airport"; }
	virtual const char* GetName() override;

	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	static float RandomAcreage();
	static float GetAcreageMin();
	static float GetAcreageMax();
	static float GetPower(AREA_TYPE area); // 恒0，不参与CDF

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

// 公园：0层，不放楼体，只种树。
class ParkBuilding : public BuildingMod {
public:
	ParkBuilding();

	static const char* GetId() { return "building_park"; }
	virtual const char* GetType() const override { return "building_park"; }
	virtual const char* GetName() override;

	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;
	// 在整块quad里随机种树(地图全部生成完之后由Map::InitVegetation调用)。
	virtual void LayoutTrees(const Quad& quad, const std::unordered_map<int, Road*>& boundaryRoads) override;

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	static float RandomAcreage();
	static float GetAcreageMin();
	static float GetAcreageMax();
	static float GetPower(AREA_TYPE area);

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

// Mohar：用AddDecoration放一个成品建筑模型(3rdParty/Mohar)，并和楼层/房间系统融合，详见building_basic.md。
class MoharBuilding : public BuildingMod {
public:
	MoharBuilding();

	static const char* GetId() { return "building_mohar"; }
	virtual const char* GetType() const override { return "building_mohar"; }
	virtual const char* GetName() override;

	// direction==-1时从boundaryRoads随机挑一个有路的方向回写(模型朝向靠它决定)。
	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	static float RandomAcreage();
	static float GetAcreageMin();
	static float GetAcreageMax();
	static float GetPower(AREA_TYPE area);

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
