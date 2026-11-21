# immigration.h / immigration.cpp

## 职责
人口迁入：城里人少了就从城外迁入成年新市民，让人口回升到开局人口的`GovernmentPolicy::immigrationTargetRatio`倍(默认1.5，是政策`immigration_ratio`，党派选举会调整它，见`polit/polit.md`)附近。

## 关键设计
- **每天迁入人数** = (R·x − y) × r，R=`immigrationTargetRatio`(原来写死在本文件里)。x是开局市民总数（`Populace::GetInitialPopulation`，`Init`跑完时记下，随人口段存档），y是当前在场（`IsActive`，不含死亡和离城）的市民数，r每天取[0, 0.5%]的随机数；结果是小数时，小数部分按概率进位。y≥R·x时不迁入。每天迁入比例上限[0, 0.5%]不可调。
- 每位新市民：`Populace::SpawnImmigrant`随机性别、姓名（重名重试）、20~45岁随机生日、按权重随机的日程类型；`Settlement::InitFunds(…, resetBaseline=false)`按开局同一正态分布领积蓄（城市外账户注资，**不更新开局注资基准**，否则出口/税率/补助的三重负反馈基准会被抬高）；住进一间**没人住的住宅房间**（`Map::SettleCitizen`）；`Society::RecruitCitizen`求职，没有空缺岗位时保持无业，领失业补助。
- 没有空房间时当天迁不进来，不挤进已有的户。
- 迁入的人没有亲属、同学、工作经历等关系。
- UE层：`DailyReport`之外由`AForeverFrameworkActor`在`Economy::Daily`之后调用，对返回的市民调`UForeverPopulaceFrameworkComponent::AddCitizen`，之后流式生成会按距离生成他们的Actor。

## 依赖关系
- 依赖：`Society`、`Populace`、`Map`、`Settlement`。
- 被谁依赖：`AForeverFrameworkActor`、EconSim。
