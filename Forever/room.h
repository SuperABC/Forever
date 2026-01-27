#pragma once

#include "room_base.h"


// 子类注册函数
typedef void (*RegisterModRoomsFunc)(RoomFactory* factory);

// 默认住宅房间
class DefaultResidentialRoom : public Room {
public:
    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;

    virtual bool IsResidential() const override;
    virtual bool IsWorkspace() const override;
    virtual int GetLivingCapacity() const override;
    virtual int GetPersonnelCapacity() const override;
};

// 默认工作房间
class DefaultWorkingRoom : public Room {
public:
    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;

    virtual bool IsResidential() const override;
    virtual bool IsWorkspace() const override;
    virtual int GetLivingCapacity() const override;
    virtual int GetPersonnelCapacity() const override;
};
