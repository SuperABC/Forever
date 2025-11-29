#pragma once

#include "../common/rect.h"

#include "component_base.h"
#include "building_base.h"

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Building;
class Component;

class Room : public Rect {
public:
    Room() = default;
    virtual ~Room() = default;

	// 子类实现方法

    // 动态返回房间静态信息
    static std::string GetId();
    virtual std::string GetType() const = 0;
    virtual std::string GetName() const = 0;

	// 父类实现方法

    // 关联建筑
    void SetParent(std::shared_ptr<Building> building);
    void SetParent(std::shared_ptr<Component> component);
    std::shared_ptr<Building> GetParentBuilding() const;
    std::shared_ptr<Component> GetParentComponent() const;

    // 获取/设置属性
    int GetLayer() const;
    void SetLayer(int layer);
    int GetFace() const;
    void SetFace(int face);

protected:
    std::shared_ptr<Building> parentBuilding;
    std::shared_ptr<Component> parentComponent;

    int layer;
    int face;
};

class RoomFactory {
public:
    void RegisterRoom(const std::string& id, std::function<std::unique_ptr<Room>()> creator);
    std::unique_ptr<Room> CreateRoom(const std::string& id);
    bool CheckRegistered(const std::string& id);
    void SetConfig(std::string name, bool config);

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Room>()>> registries;
    std::unordered_map<std::string, bool> configs;
};