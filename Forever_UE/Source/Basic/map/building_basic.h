#pragma once

#include "map/building_mod.h"


// Building域默认内容，各类型的设计见building_basic.md。RandomAcreage/GetAcreageMin/
// GetAcreageMax/GetPower/Assign都是不需要实例的static方法(通过BuildingFactory::RegisterBuilding
// 注册)。住宅/商店/工厂/办公楼各分高/中/低三档，12个类各自独立实现、没有共用基类，三档的
// 逻辑目前是同一份的拷贝，以后各自改。

// 住宅(高密度)：只在住宅高密度分区(AREA_RESIDENCE_HIGH)上有权重。
class ResidenceHighBuilding : public BuildingMod {
public:
	ResidenceHighBuilding();

	static const char* GetId() { return "building_residence_high"; }
	virtual const char* GetType() const override { return "building_residence_high"; }
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

// 住宅(中密度)：只在住宅中密度分区(AREA_RESIDENCE_MIDDLE)上有权重。
class ResidenceMiddleBuilding : public BuildingMod {
public:
	ResidenceMiddleBuilding();

	static const char* GetId() { return "building_residence_middle"; }
	virtual const char* GetType() const override { return "building_residence_middle"; }
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

// 住宅(低密度)：只在住宅低密度分区(AREA_RESIDENCE_LOW)上有权重。
class ResidenceLowBuilding : public BuildingMod {
public:
	ResidenceLowBuilding();

	static const char* GetId() { return "building_residence_low"; }
	virtual const char* GetType() const override { return "building_residence_low"; }
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

// 商店(高密度)：只在商店高密度分区(AREA_COMMERCE_HIGH)上有权重。
class ShopHighBuilding : public BuildingMod {
public:
	ShopHighBuilding();

	static const char* GetId() { return "building_shop_high"; }
	virtual const char* GetType() const override { return "building_shop_high"; }
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

// 商店(中密度)：只在商店中密度分区(AREA_COMMERCE_MIDDLE)上有权重。
class ShopMiddleBuilding : public BuildingMod {
public:
	ShopMiddleBuilding();

	static const char* GetId() { return "building_shop_middle"; }
	virtual const char* GetType() const override { return "building_shop_middle"; }
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

// 商店(低密度)：只在商店低密度分区(AREA_COMMERCE_LOW)上有权重。
class ShopLowBuilding : public BuildingMod {
public:
	ShopLowBuilding();

	static const char* GetId() { return "building_shop_low"; }
	virtual const char* GetType() const override { return "building_shop_low"; }
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

// 工厂(高密度)：只在工厂高密度分区(AREA_INDUSTRY_HIGH)上有权重。
class FactoryHighBuilding : public BuildingMod {
public:
	FactoryHighBuilding();

	static const char* GetId() { return "building_factory_high"; }
	virtual const char* GetType() const override { return "building_factory_high"; }
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

// 工厂(中密度)：只在工厂中密度分区(AREA_INDUSTRY_MIDDLE)上有权重。
class FactoryMiddleBuilding : public BuildingMod {
public:
	FactoryMiddleBuilding();

	static const char* GetId() { return "building_factory_middle"; }
	virtual const char* GetType() const override { return "building_factory_middle"; }
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

// 工厂(低密度)：只在工厂低密度分区(AREA_INDUSTRY_LOW)上有权重。
class FactoryLowBuilding : public BuildingMod {
public:
	FactoryLowBuilding();

	static const char* GetId() { return "building_factory_low"; }
	virtual const char* GetType() const override { return "building_factory_low"; }
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

// 办公楼(高密度)：只在办公楼高密度分区(AREA_OFFICE_HIGH)上有权重。
class OfficeHighBuilding : public BuildingMod {
public:
	OfficeHighBuilding();

	static const char* GetId() { return "building_office_high"; }
	virtual const char* GetType() const override { return "building_office_high"; }
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

// 办公楼(中密度)：只在办公楼中密度分区(AREA_OFFICE_MIDDLE)上有权重。
class OfficeMiddleBuilding : public BuildingMod {
public:
	OfficeMiddleBuilding();

	static const char* GetId() { return "building_office_middle"; }
	virtual const char* GetType() const override { return "building_office_middle"; }
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

// 办公楼(低密度)：只在办公楼低密度分区(AREA_OFFICE_LOW)上有权重。
class OfficeLowBuilding : public BuildingMod {
public:
	OfficeLowBuilding();

	static const char* GetId() { return "building_office_low"; }
	virtual const char* GetType() const override { return "building_office_low"; }
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
