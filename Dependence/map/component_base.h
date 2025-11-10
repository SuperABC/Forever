#pragma once

#include "room_base.h"

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Component {
public:
    Component() = default;
    virtual ~Component() = default;

    // 动态返回组合静态信息
    static std::string GetId();
    virtual std::string GetName() const = 0;

    // 获取/添加房间
    std::vector<std::shared_ptr<Room>>& GetRooms() { return rooms; }
    void AddRoom(std::shared_ptr<Room> room) { rooms.push_back(room); }

protected:
    std::vector<std::shared_ptr<Room>> rooms;
};

class ComponentFactory {
public:
    void RegisterComponent(const std::string& id, std::function<std::unique_ptr<Component>()> creator);
    std::unique_ptr<Component> CreateComponent(const std::string& id);
    bool CheckRegistered(const std::string& id);
    void SetConfig(std::string name, bool config);

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Component>()>> registries;
    std::unordered_map<std::string, bool> configs;
};