#pragma once

#include "map/component_mod.h"
#include "map/component_factory.h"

#include <string>
#include <vector>

class Building;
class Room;

// Component(组合)：若干Room的集合，用来表达"一个公司/组织在一栋building里占据的一片
// 连续或不连续的房间"（比如某公司租了写字楼的半层，这半层楼的所有房间就是一个Component）。
// 一个Building可以有多个Component(比如同一栋写字楼里租给不同公司的不同楼层)。这次只做
// Component本身(绑定在单个Building内部)，不做Organization(公司/组织，持有跨building的
// 多个Component)——Organization依赖的Populace/Job这次还没迁移，留到以后Society阶段，
// 见Source/Core/map/building.md。
//
// 和Room一样，Component永远是Building自己在Layout()里通过AssignRoom/ArrangeRow
// (component名字,id)间接创建的，不参与地块竞争，不需要Assign/RandomAcreage这套static
// 注册机制——照Zone/Building的独占持有模式：Component独占一个ComponentMod实例，构造时
// 创建、析构时factory->DestroyComponent(mod)。
class Component {
public:
	Component() = delete;

	// @factory: component工厂(用于~Component()里DestroyComponent); @mod: 这个Component
	// 独占持有的mod实例; @parentBuilding: 归属的Building(不持有生命周期)。
	Component(ComponentFactory* factory, ComponentMod* mod, Building* parentBuilding);
	~Component();

	std::string GetType() const;
	std::string GetName() const;

	// 这个Component持有的mod实例——不需要另外拷贝一份。
	ComponentMod* GetMod() const;

	Building* GetParentBuilding() const;

	// 不持有生命周期，Room由Building自己的rooms数组统一持有/销毁。
	const std::vector<Room*>& GetRooms() const;
	void AddRoom(Room* room);

private:
	ComponentMod* mod;
	ComponentFactory* factory;
	std::string type;
	std::string name;
	Building* parentBuilding;
	std::vector<Room*> rooms;
};
