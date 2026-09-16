#pragma once

#include "map/zone_mod.h"
#include "map/zone_factory.h"
#include "map/geometry.h"

#include <string>
#include <unordered_map>
#include <vector>

class Building;
class Citizen;

// Zone：持有一个具体ZoneMod实例，代表一块已经落地的Zone占位（继承Quad表示自己占据的矩形，
// 和Lot本身"继承Quad表示自己的矩形"是同一种写法）。这次不实现Zone内部再摆Building的递归
// 布局（关键设计决策1），所以只是一个footprint+类型的占位对象，详见map.md"InitZones"一节。
//
// 仿照老工程：Zone从构造到析构只持有**一个**ZoneMod实例，不存在"扫描用/落地用两个不同实例"
// 这种分裂——Map::InitZones()先对这个类型调一次static`ZoneMod::Assign(lots, emit, context)`
// 一次性扫完全部lot拿到想要的显式占位请求(不需要任何实例)，只有`lot->RequestPlacement(...)`
// 也真的成功了，才`zoneFactory.CreateZone(id)`一个新实例、调用`zone->Layout(direction)`
// 填好围墙等数据，这个mod实例就直接交给新建的Zone持有，不会出现"一个mod实例的产出被分给好几个
// Zone"这种共享所有权的情况，析构时`~Zone()`可以放心`factory->DestroyZone(mod)`。围墙/大门
// (`ZoneWallSpec`/`ZoneGateSpec`)是mod自己的数据、不需要Core转换，`GetWalls()`/`GetGates()`
// 直接转发`mod->walls`/`mod->gates`，不再单独拷贝一份到Zone自己身上。
//
// 不自己存一份rotation——落地的Zone来自某个Lot的freeLots切出来的一块，freeLots全部继承同一个
// 顶层Lot的rotation(SplitWithPath产出的每一段都传了同一个rotation，见geometry.cpp)，所以
// GetRotation()直接转发parentLot->GetRotation()就是正确值，没必要在Zone自己身上再存一份
// 冗余拷贝（早前"照抄Lot自己加一个旋转角度"的做法多此一举，parentLot本来就有）。这意味着
// GetRotation()必须在SetParentLot(lot)之后调用才有意义，构造完/SetParentLot之前调用返回0。
class Zone : public Quad {
public:
	Zone() = delete;

	// @factory: zone工厂(用于~Zone()里DestroyZone); @mod: 这个Zone独占持有的mod实例(调用方
	// 保证不会再有别的Zone共用同一个mod指针，Layout()会在构造之后单独调用)。
	Zone(ZoneFactory* factory, ZoneMod* mod);
	~Zone();

	std::string GetType() const;
	std::string GetName() const;

	// 这个Zone持有的mod实例——Map::InitBuildings()用它读mod->internalBuildings(园区内部
	// 建筑的原始spec列表)实例化Building，不需要Zone另外拷贝一份。
	ZoneMod* GetMod() const;

	// 转发parentLot->GetRotation()；parentLot为空时返回0.f。
	float GetRotation() const;

	// 格式"<road> <index> <zoneName>"，和Map::LocateZone的解析格式一一对应
	// （parentLot->GetAddress() + " " + GetName()，照抄老工程Zone::GetAddress）。
	std::string GetAddress() const;

	// 调用方在SetPosition/SetBoundaryRoad都设好之后调用一次：转发mod->Layout(direction, *this,
	// GetBoundaryRoads())——Zone这一层没有需要额外解析缓存的数据(GetWalls()/GetGates()本来就
	// 直接转发mod->walls/mod->gates)，这个方法纯粹是为了和Building::Layout()同样的调用形态，
	// 不需要Map::InitZones()自己摸mod指针。
	void Layout(int direction);

	Lot* GetParentLot() const;
	void SetParentLot(Lot* lot);

	// 四周边界Road：下标按FACE_DIRECTION(0-3)，和Lot::boundaryRoads语义完全一致，不持有
	// 指针生命周期(由RoadnetMod/Roadnet管理)。
	void SetBoundaryRoad(int direction, Road* road);
	Road* GetBoundaryRoad(int direction) const;
	const std::unordered_map<int, Road*>& GetBoundaryRoads() const;

	// 围墙/大门：局部坐标语义(参考边+margin+depth)见ZoneWallSpec/ZoneGateSpec注释
	// (zone_mod.h)。直接转发mod自己的walls/gates(mod是这个Zone独占持有的，数据不会失效)，
	// 不需要Core另外拷贝一份；Forever层渲染时直接拿这份数据+自己的
	// GetPosX/PosY/SizeX/SizeY/GetRotation()现算世界坐标。
	const std::vector<ZoneWallSpec>& GetWalls() const;
	const std::vector<ZoneGateSpec>& GetGates() const;

	// 园区内部道路：Map::InitZones()按mod->internalRoads(ZoneInternalRoadSpec列表)实例化出的
	// 真正Road，Zone持有生命周期(~Zone()里delete)——这些Road是专门为这个Zone新建的，不像
	// boundaryRoads那样借用RoadnetMod的既有Road。
	void SetInternalRoads(const std::vector<Road*>& roads);
	const std::vector<Road*>& GetInternalRoads() const;

	// 园区内部建筑：只登记指针，不持有生命周期——这些Building的所有权在Map::buildings
	// (和其余顶层Building一样被~Map()统一释放)，这里只是方便"这个zone里有哪些building"
	// 的枚举查询。由Map::InitBuildings()读mod->internalBuildings实例化后调用。
	void AddInternalBuilding(Building* building);
	const std::vector<Building*>& GetInternalBuildings() const;

	// 归属——照抄老工程Zone::GetOwner/SetOwner/GetStated/SetStated(Map::Checkin()分配
	// 房产归属时用)。owner和stated互斥：owner非空表示这整个zone(连同内部所有
	// building/room)归一个citizen私有；stated为true表示公有(owner保持空)。这个zone没有
	// 整体归属(内部不同building/room各自独立归属)时两者都保持默认值(nullptr/false)，
	// 不需要显式"重置"——Map::Checkin()只在"整个zone统一归属"分支才会调用这两个setter，
	// 见room.h同一套owner/stated设计说明。
	Citizen* GetOwner() const;
	void SetOwner(Citizen* value);
	bool GetStated() const;
	void SetStated(bool value);

private:
	ZoneMod* mod;
	ZoneFactory* factory;
	std::string type;
	std::string name;
	Lot* parentLot = nullptr;
	std::unordered_map<int, Road*> boundaryRoads;
	std::vector<Road*> internalRoads;
	std::vector<Building*> internalBuildings;

	Citizen* owner = nullptr;
	bool stated = false;
};
