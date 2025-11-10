#pragma once

#include "../common/rect.h"

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Room : public Rect {
public:
    Room() = default;
    virtual ~Room() = default;

    // 动态返回房间静态信息
    static std::string GetId();
    virtual std::string GetName() const = 0;

    int GetLayer() const { return layer; }
    void SetLayer(int layer) { this->layer = layer; }

protected:
    int layer;
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