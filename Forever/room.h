#pragma once

#include "room_base.h"


// 子类注册函数
typedef void (*RegisterModRoomsFunc)(RoomFactory* factory);

// 公寓房间
class FlatRoom : public Room {
public:
    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;

    virtual bool IsResidential() const override;
    virtual bool IsWorkspace() const override;
    virtual int GetLivingCapacity() const override;
    virtual int GetPersonnelCapacity() const override;
};
