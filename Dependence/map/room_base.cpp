#include "room_base.h"


using namespace std;

void RoomFactory::RegisterRoom(const string& id, function<unique_ptr<Room>()> creator) {
    registry[id] = creator;
}

unique_ptr<Room> RoomFactory::CreateRoom(const string& id) {
    auto it = registry.find(id);
    if (it != registry.end()) {
        return it->second();
    }
    return nullptr;
}

bool RoomFactory::CheckRegistered(const string& id) {
    return registry.find(id) != registry.end();
}

