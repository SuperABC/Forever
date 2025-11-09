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

	Facility(FACILITY_TYPE type, float x, float y, float w, float h)
		: Rect(x, y, w, h), type(type) {
	}

	FACILITY_TYPE getType() const { return type; }

private:
	FACILITY_TYPE type;
};

// 楼层
class Floor : public Rect {
public:
	Floor(int level, float width, float height)
		: level(level), Rect(0, 0, width, height) {
	}
	~Floor() {}

	void AddFacility(Facility facility) {
		facilities.push_back(facility);
	}

	void AddUsage(std::pair<Rect, int> row) {
		rows.push_back(row);
	}

	void AddRoom(std::shared_ptr<Room> room) {
		rooms.push_back(room);
	}

	// 获取楼层
	int GetLevel() const { return level; }

	// 访问组件
	std::vector<Facility>& GetFacilities() { return facilities; }
	std::vector<std::pair<Rect, int>>& GetRows() { return rows; }
	std::vector<std::shared_ptr<Room>>& GetRooms() { return rooms; }

private:
	int level;

	std::vector<Facility> facilities;
	std::vector<std::pair<Rect, int>> rows;
	std::vector<std::shared_ptr<Room>> rooms;
};

class Building : public Rect {
public:
    Building() = default;
    virtual ~Building() = default;

    // 动态返回建筑静态信息
    static std::string GetId();
    virtual std::string GetName() const = 0;

	// 获取/设置属性
	int GetLayers() { return layers; }
	int GetBasements() { return basements; }

	// 获取/设置组织/房间/楼层
	std::vector<std::shared_ptr<Component>>& GetComponents() { return components; }
	std::vector<std::shared_ptr<Room>>& GetRooms() { return rooms; }
	std::shared_ptr<Floor> GetFloor(int floor) {
		if (basements + floor >= 0 && basements + floor < floors.size())
			return floors[basements + floor];
		else return nullptr;
	}

    // 功能区中的建筑权重
    static std::vector<float> GetPower();

    // 建筑面积范围
    virtual float RandomAcreage() const = 0;
    virtual float GetAcreageMin() const = 0;
    virtual float GetAcreageMax() const = 0;

    // 内部房间布局
    virtual void LayoutRooms() = 0;

protected:
	std::vector<std::shared_ptr<Floor>> floors;
	std::vector<std::shared_ptr<Component>> components;
	std::vector<std::shared_ptr<Room>> rooms;

	int layers = 1;
	int basements = 0;

	// 建筑中添加组织
	template<class T>
	std::shared_ptr<T> CreateComponent() {
		std::shared_ptr<T> component = std::make_shared<T>();
		components.push_back(component);
		return component;
	}

	// 建筑中添加房间
	template<class T>
	std::shared_ptr<T> CreateRoom(int layer, int acreage) {
		std::shared_ptr<T> room = std::make_shared<T>();
		room->SetLayer(layer);
		room->SetAcreage(acreage);
		rooms.push_back(room);
		return room;
	}
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