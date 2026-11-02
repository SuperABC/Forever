# job_basic.h / job_basic.cpp

## 职责

`ShopSalerJob`(`job_shop_saler`)：店员，天天上班——早上9点从家出门去商店，中午12点从商店下班回家。

## 关键设计

- **"哪几天上班/几点上下班"直接写在`DailyPlan`里**(不要Calendar)；`DailyPlan`/`ExecNode`完全是C++直接构造`Change*`(`NPCNavigateChange`)，和milestone/JSON无关，见`Core/society/job.md`。
- 导航目的地用`post`查occupant家/工位的具体房间地址，不能写描述性文本，见`Dependence/society/job_mod.md`。
- `SetProperty()`设`scriptModName="script_empty"`+`milestoneNames={"job_shop_saler"}`，让`Job`据此创建一个挂了`Resource/Story/job_shop_saler.script`的`Script`。
- `GetName()`必须全局唯一。

## 依赖关系

- 依赖：`society/job_mod.h`、`story/change.h`(`NPCNavigateChange`)。
- 被谁依赖：`basic.cpp`注册(依赖`script_empty`)；`ShopOrganization::DesignJobsForRoom`配它；`Core/society/job.h`消费。

## 待办/后续阶段

- 无。
