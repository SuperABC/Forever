# world_step

## 职责
世界按时间推进一步(`WorldStep::Step`)的唯一实现：地图→人口→社会→政治(每步，不受crossedDay限制)→产业物流→每日经济链(含离城/迁入)→交通→剧情。UE层`AForeverFrameworkActor::StepWorld`和自动化测试(`Source/Test/Core`的econ/save/perf套件)调用同一份，保证被测的就是线上跑的。

## 关键设计
- Core看不到UE，所以UE特有的部分通过`WorldStepHooks`注入：脚本Change的应用(`onCitizenChanges/onOrganizationChanges`)、离城/迁入市民的Actor处理、交通停站事件的转发(`onTrafficTicked`，夹在Traffic::Tick和Story::Tick之间，顺序与原StepWorld一致)、党代会开始/结束通知的转发(`onPoliticsTicked`，紧跟在`Polit::Step`之后；UE层用`Polit::ConsumeMeetingNotices`取走并发给党员脚本)。`WorldDomains::polit`为空(没有政治域)时整段跳过。测试程序不注入就等于忽略Change。
- 调用方负责先把`Player`时钟推进到本步时间并算好`crossedDay`；`Step`自己同步人口的当前年份。
- 经济链只在`crossedDay`且产业/社会/地图/人口都在时才跑。账本总和校验留给调用方(UE打日志，测试做断言)。
- `TickPopulace/TickSociety`单独暴露，跳时间"同一时刻反复Tick排空到期计时器"要用。
- **强制睡觉**(2026-11-26)：`TickPopulace`在`Populace::Tick`之后取`ConsumeForcedSleepers()`(瞌睡值掉到0、已经被强制睡下的市民)。
  只有玩家正在操控的市民扣瞌睡值(`Populace::Tick`收`hooks.controlledCitizen`)，所以名单里最多就他一个，世界要跟着跳过这16小时：Core现场构造一条`PlayerSleepChange("16")`交给`hooks.onCitizenChanges`，
  UE层执行它时跳时间。这条逻辑不经过任何脚本。

## 依赖关系
- 依赖：Map/Populace/Society/Polit/Industry/Traffic/Story/Player各域、Economy、Immigration、PostImplement。
- 被谁用：`Source/Forever/Framework/ForeverFrameworkActor.cpp`、`Source/Test/Core`。
