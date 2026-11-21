# polit_log.h / polit_log.cpp

## 职责
政治域日志：Core里所有政治事件只调`PolitLog(now, category, body)`，由调用方用`SetPolitLogSink`决定落点——
CoreTest的sink写stdout(带`--out dir`时同时写`polit.log`)，UE的sink接`LogPolit`(PIE日志里搜"LogPolit")。没设sink时丢弃。

## 格式
机器可grep的单行：`[polit] <游戏时间> <类别> key=value ...`。小数统一用`PolitNum`(保留3位)。

## 类别一览
- 初始化：`init`(各党成员数)、`party`(党派mod id/成员名单)、`meeting_room`(会议室每次尝试的地址、是否重叠、最终结果)；
- 月末选举：`election`(选民人数/汇总/胜者/terms)、`proposal`/`proposal_rejected`(各党逐项政策的提议比例及被拒原因)、`votes`(各党得票)、
  `policy`(每项政策旧值→新值，是否触到上下限)、`power`/`power_rejected`(权力分配及被拒原因)、`ruling_lost`(旧执政党权力清空)；
- 党代会：`congress_plan`(本月日程)、`congress_skipped`(执政党本月不开)、`congress_start`(通知了多少党员)、`attendance`(到场名单)、
  `leader_vote`(逐个投票人投给谁及权重，有关系的标出respect)、`leader_tally`(各候选人得票)、`leader`(最终党首)、
  `leader_cleared`、`congress_end`；
- 成员：`member_removed`(死亡/离城)。

CoreTest的断言既解析这些行也读`Polit`状态，二者互相印证。

## 依赖关系
- 被谁依赖：`polit.cpp`/`election.cpp`/`congress.cpp`；落点由`ForeverFrameworkActor`(UE)和`suite_polit.cpp`(CoreTest)设置。
