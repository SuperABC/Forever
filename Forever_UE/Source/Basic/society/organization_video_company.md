# organization_video_company.h / organization_video_company.cpp

## 职责

`VideoCompanyOrganization`(`organization_video_company`)：视频公司，认领1~2个`component_office`，在`room_office`里配`job_video_maker`，向市民提供`video`服务。

## 关键设计

- `GetRequirements()`：`{component_office, {1,2}}`；`GetPower()`=1。目前只有它需要办公Component，所以全城办公楼的Component会被视频公司分完。
- 经济参数：初始资金20万元、10个股东各占0.1、日薪150元(15000分)。`MonthlySettle`同商店：增发、按利润扩缩视频制作员(50%~200%)。
- 服务：`CollectServices`上报一条`video`服务，**单价10元(1000分)、质量0.1**，容量=`self.employees`(Core每天在`RunServices`前写入的在岗人数)x20份/人/天(`kVideosPerWorkerPerDay`，我定的产能参数，可调)。没人在岗就不提供。没有库存，`ApplyServiceUsage`无事可做。
- 市民侧需求见`Core/society/economy.md`的"video需求"。
- `GetName()`全局唯一：static计数器+id+现拼。

## 依赖关系

- 依赖：`society/organization_mod.h`、`organization_common.h`。被谁依赖：`basic.cpp`注册(依赖`component_office`/`room_office`/`job_video_maker`)。
