#pragma once

#include "scheduler_base.h"
#include "zone_base.h"


// 子类注册函数
typedef void (*RegisterModSchedulersFunc)(SchedulerFactory* factory);

// 纯工作调度
class WorkOnlyScheduler : public Scheduler {
public:
    WorkOnlyScheduler();
    ~WorkOnlyScheduler();

    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;

    static float GetPower() {
        return 1.0f;
    }

private:

};
