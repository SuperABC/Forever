# job_video_maker.h / job_video_maker.cpp

## 职责

`VideoMakerJob`(`job_video_maker`)：视频公司的员工，在`room_office`里上班。

## 关键设计

- 上下班全在`Resource/Story/job_video_maker.script`：10:00出门去办公室，20:00回家(`leave_home`/`leave_work`两个计时器+`npc_navigate`)，没有到岗选项。改时间就改脚本里的time。
- `SetProperty`不指定`scriptModName`(脚本没有ScriptMod)，只设`milestoneNames={"job_video_maker"}`，`DailyPlan`留空，和`ShopSalerJob`同一套路。
- 工资是组织的(`VideoCompanyOrganization::wage`，每日150元)，不在Job上。
- `GetName()`全局唯一：static计数器+id+现拼，计数器存档见`Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`society/job_mod.h`。被谁依赖：`basic.cpp`注册，`organization_video_company`配岗位。
