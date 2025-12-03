#pragma once

#include "../common/rect.h"
#include "../common/plot.h"

#include "room_base.h"
#include "component_base.h"

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


enum AREA_TYPE;

class Plot;
class Zone;
class Component;
class Room;

class RoomFactory;

// 建筑方向
enum FACE_DIRECTION {
	FACE_WEST,
	FACE_EAST,
	FACE_NORTH,
	FACE_SOUTH
};
static char faceAbbr[4] = { 'w', 'e', 'n', 's' };

// 走廊&电梯&楼梯
class Facility : public Rect {
public:
	enum FACILITY_TYPE { FACILITY_CORRIDOR, FACILITY_STAIR, FACILITY_ELEVATOR };

	Facility(FACILITY_TYPE type, float x, float y, float w, float h);

	FACILITY_TYPE getType() const;

private:
	FACILITY_TYPE type;
};

// 楼层
class Floor : public Rect {
public:
	Floor(int level, float width, float height);
	~Floor();

	void AddFacility(Facility facility);

	void AddRow(std::pair<Rect, int> row);

	void AddRoom(std::pair<Rect, int> room);

	// 获取楼层
	int GetLevel() const;

	// 访问组件
	std::vector<Facility>& GetFacilities();
	std::vector<std::pair<Rect, int>>& GetRows();
	std::vector<std::pair<Rect, int>>& GetRooms();

private:
	int level;

	std::vector<Facility> facilities;
	std::vector<std::pair<Rect, int>> rows;
	std::vector<std::pair<Rect, int>> rooms;
};

class Layout {
public:
	std::unordered_map<std::string, std::vector<std::vector<std::pair<Facility::FACILITY_TYPE, std::vector<float>>>>> templateFacilities;
	std::unordered_map<std::string, std::vector<std::vector<std::pair<FACE_DIRECTION, std::vector<float>>>>> templateRows;
	std::unordered_map<std::string, std::vector<std::vector<std::pair<FACE_DIRECTION, std::vector<float>>>>> templateRooms;
};

class Building : public Rect {
public:
    Building() = default;
    virtual ~Building() = default;

	// 子类实现方法

    // 动态返回建筑静态信息
    static std::string GetId();
    virtual std::string GetType() const = 0;
    virtual std::string GetName() const = 0;

    // 功能区中的建筑权重
    static std::vector<float> GetPower();

    // 建筑面积范围
    virtual float RandomAcreage() const = 0;
    virtual float GetAcreageMin() const = 0;
    virtual float GetAcreageMax() const = 0;

    // 内部房间布局
    virtual void LayoutRooms(RoomFactory *factory, std::unique_ptr<Layout>& layout) = 0;

	// 父类实现方法

	// 关联地块
	void SetParent(std::shared_ptr<Plot> plot);
	void SetParent(std::shared_ptr<Zone> zone);
	std::shared_ptr<Plot> GetParentPlot() const;
	std::shared_ptr<Zone> GetParentZone() const;

	// 获取/设置房东
	int GetOwner() const;
	void SetOwner(int id);
	bool GetStateOwned() const;
	void SetStateOwned(bool state);

	// 获取/设置属性
	int GetLayers() const;
	int GetBasements() const;

	// 获取/设置组织/房间/楼层
	std::vector<std::shared_ptr<Component>>& GetComponents();
	std::vector<std::shared_ptr<Room>>& GetRooms();
	std::shared_ptr<Floor> GetFloor(int level) const;

	// 补充初始化
	void FinishInit();

	// 读入布局模板
	static std::unique_ptr<Layout> ReadTemplates(std::string path);

protected:
	std::shared_ptr<Zone> parentZone;
	std::shared_ptr<Plot> parentPlot;

	bool stateOwned = false;
	int ownerId = -1;

	std::vector<std::shared_ptr<Floor>> floors;
	std::vector<std::shared_ptr<Component>> components;
	std::vector<std::shared_ptr<Room>> rooms;

	int layers = 1;
	int basements = 0;

	// 根据布局文件分配房间
	void ReadFloor(int level, float width, float height, int face, std::string name, std::unique_ptr<Layout>& layout);
	void ReadFloors(float width, float height, int face, std::string name, std::unique_ptr<Layout>& layout);
	void ReadFloors(float width, float height, int face, std::vector<std::string> names, std::unique_ptr<Layout>& layout);
	void AssignRoom(int level, int slot, std::string name, std::shared_ptr<Component> component, RoomFactory* factory);
	void ArrangeRow(int level, int slot, std::string name, float acreage, std::shared_ptr<Component> component, RoomFactory* factory);

	// 建筑中添加组织
	template<class T>
	std::shared_ptr<T> CreateComponent() {
		std::shared_ptr<T> component = std::make_shared<T>();
		components.push_back(component);
		return component;
	}

	// 建筑中添加房间
	template<class T>
	std::shared_ptr<T> CreateRoom(int level, float acreage) {
		std::shared_ptr<T> room = std::make_shared<T>();
		room->SetLayer(level);
		room->SetAcreage(acreage);
		rooms.push_back(room);
		return room;
	}

private:
	static std::vector<float> InverseParams(std::vector<float>& params, int face);
	static int InverseDirection(int direction, int face);
};

class BuildingFactory {
public:
    void RegisterBuilding(const std::string& id,
		std::function<std::unique_ptr<Building>()> creator, std::vector<float> powers);
    std::unique_ptr<Building> CreateBuilding(const std::string& id);
    bool CheckRegistered(const std::string& id);
    void SetConfig(std::string name, bool config);
    const std::unordered_map<std::string, std::vector<float>>& GetPowers() const;

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Building>()>> registries;
    std::unordered_map<std::string, bool> configs;
    std::unordered_map<std::string, std::vector<float>> powers;
};