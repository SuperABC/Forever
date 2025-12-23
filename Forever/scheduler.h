#pragma once

#include "scheduler_base.h"
#include "zone_base.h"


// 子类注册函数
typedef void (*RegisterModSchedulersFunc)(SchedulerFactory* factory);

// 主程序检测子类
class WorkOnlyScheduler : public Scheduler {
public:
    WorkOnlyScheduler();
    ~WorkOnlyScheduler();

    static std::string GetId() { return "test"; }
    virtual std::string GetType() const override { return "test"; }
    virtual std::string GetName() const override { return "测试调度"; }

    static float GetPower() {
        return 1.0f;
    }

private:

};
