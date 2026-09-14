#pragma once

#include "map/room_mod.h"
#include "map/room_factory.h"
#include "map/geometry.h"

#include <string>
#include <unordered_map>
#include <vector>

class Building;
class Component;
class Node;

// WallHole(一面墙上的门/窗开口列表)定义在geometry.h——Room和building.h的Corridor/Single/
// Row都要用，geometry.h是两边都已经包含的公共底层头。

// Room(房间)：Building内部布局实例化出来的一个具体房间，继承Quad表示自己在Building楼体
// 局部坐标系(以Building::GetBodySizeX/Y()为宽高，原点在楼体左下角，未旋转)里占据的矩形。
// 这次只保留和"布局/渲染/寻址"直接相关的部分——不迁移老工程Room身上的furniture/pivots/
// storages/manufactures/parkings/vehicles/ownership/tenancy/workers等字段，那些依赖
// Industry/Populace/Society这些还没迁移的领域，等对应领域迁移到了再回来加。
//
// 和Component一样，Room永远是Building自己在Layout()里通过AssignRoom/ArrangeRow显式创建
// 的，不参与地块竞争，不需要Assign/RandomAcreage这套static注册机制——独占持有一个RoomMod
// 实例，构造时创建、析构时factory->DestroyRoom(mod)。
class Room : public Quad {
public:
	Room() = delete;

	Room(RoomFactory* factory, RoomMod* mod, Building* parentBuilding, Component* parentComponent, int layer);
	~Room();

	std::string GetType() const;
	std::string GetName() const;

	// 这个Room持有的mod实例——不需要另外拷贝一份。
	RoomMod* GetMod() const;

	int GetLayer() const;
	int GetDirection() const;
	void SetDirection(int direction);

	Building* GetParentBuilding() const;
	Component* GetParentComponent() const;

	const WallHole& GetDoors() const;
	const WallHole& GetWindows() const;
	void SetDoors(WallHole doors);
	void SetWindows(WallHole windows);

	// 门牌号，"b3-0001"这种格式(basement前缀+4位序号)，照抄老工程Room::SetNumber。
	const std::string& GetNumber() const;
	void SetNumber(int level, int number);

	// 这个房间中心点的导航锚点，Building::Layout()在实例化完所有Room之后统一创建、
	// 供BuildPedestrianNavigation()里"single"/"row"类型的导航端点引用。不持有生命周期
	// (和其它building导航节点一样交给Map::navAnchorNodes统一管理)。
	Node* GetNavigationNode() const;
	void SetNavigationNode(Node* node);

private:
	RoomMod* mod;
	RoomFactory* factory;
	std::string type;
	std::string name;
	Building* parentBuilding;
	Component* parentComponent;
	int layer;
	int direction = FACE_WEST;
	WallHole doors;
	WallHole windows;
	std::string number;
	Node* navigationNode = nullptr;
};
