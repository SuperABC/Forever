# polit.h / polit.cpp / election.cpp / congress.cpp

## 职责
政治域，和Map/Populace/Society平级。首个concept是党派`Party`(Dependence/Core/Basic三级)。持有党派、`GovernmentPolicy`、
`PowerCatalog`；每月末全体成年市民自动投票选出执政党，执政党的提议调整政策基础值；党内由脚本式党代会选党首；
党首/执政党可分配Core定义的权力词汇。**城市运转不依赖任何职位**；没启用任何党派时所有政策保持默认，什么都不做。

## 生命周期
- `Polit()`取工厂；`Bind(map, populace, society)`接上其它域并`society.BindPolicy(&policy)`(Polit必须比Society活得久；
  UE侧删除顺序是先polit后society，Polit析构不碰Society)；
- `Init(post, now)`只在新开局调用：每个启用的党派id建一个`Party`，党员按mod的`memberCount`从"在城成年人"池无放回随机抽(跨党互斥，池不够就抽到没人)，
  再选会议室：mod在`ChooseMeetingRoom`里`post "random room"`(type=`room_meeting`)，Core校验地址有效且没被前面的党占用，否则重调，**最多4次**，仍失败则无会议室(地图上没有`room_meeting`时也是这样；没有会议室的党党代会线上召开，见下)；
  读档**不再调**(存档值为准，见`polit_save.md`)；
- `Step(now)`每个WorldStep调一次(不受crossedDay限制，所以步长<=1小时才不会漏党代会窗口)：月份变化→选举；每党推进党代会；清理离城/死亡党员。

## 选举(election.cpp)
- 开局没有执政党，第一次选举在第一个月末(`lastMonthKey`首次只记录不选举)。
- 选民=在城成年人。身份=财富百分位`pct`(`Emigration::NetWorth`排名0~1)、是否退休、是否失业、所在户有无未成年。
- 每党对每项政策的有效比例`r=clamp(target/current, 0.9, 1.1)`(没提议=1，mod的目标值由Core clamp，所以mod不能一步调超过±10%)。
- 选民利益权重`w`：税类`-(TAX_BASE+TAX_WEALTH*pct)`；福利类`(1-pct)*(BENEFIT_OTHER+(1-BENEFIT_OTHER)*是否该项受益人)`；移民`(pct-0.5)*2`。
  效用`u=sum w*(r-1)`，支持度`S=exp(VOTE_K*u)*(当前执政? 1/(1+FATIGUE*terms):1)`，投给S最大者，平局用`core.polit`随机流。
  系数是election.cpp顶部的`POLIT_*`宏(TAX_BASE 0.1、TAX_WEALTH 0.3、BENEFIT_OTHER 0.5、VOTE_K 4.0、FATIGUE 0.1)，**占位初值，等用户调**。
- 得票最多者执政(平票随机)；同党连任`terms++`，否则`terms=1`并清掉旧执政党的权力；然后把胜者的`r`乘到政策基础值上(夹[min,max])，
  立即`AssignPowers`(不依赖党首)。

## 党代会(congress.cpp)
- 日程由mod的`GetCongress(year, month)`给(`day==0`本月不开)；内置两党都是每月1号12:00~15:00。
- 执政党执政期间不开党代会(落选后恢复)；候选人排除缺席者。start/end同一步都已过(跳时间)就依次补发。
- 到点入队start通知；end时先统计到场，再选党首，再入队end通知。到场的算法分两种：
  - 有会议室：`Room::GetOccupants()`∩党员，也就是这时候真的在会议室里的党员。
  - **没有会议室(`Party::MeetsOnline()`)：线上召开**。通知照发但`address`为空，收到通知的人不用去任何地方(内置的`citizen_common.script`只在地址非空时才导航，所以不产生任何寻路)；全体在城党员都算到场，党首照常选。这样地图上没有会议室、或者会议室被别的党占了，都不影响党首选举。
- 党首选举：投票人=到场党员，候选人=到场党员(不含自己)；权重=投票人`GetAcquaintances()`里有该候选人则`respect*2`、没有则1，
  每人按权重加权随机抽一票，得票最多者当党首(平票随机)；到场不足2人不选，沿用旧党首。
- 通知队列`ConsumeMeetingNotices()`：Core不发脚本事件，UE层(`ForeverFrameworkActor`的`onPoliticsTicked`)取走后对每个党员(跳过玩家操控的)
  调`UForeverStoryFrameworkComponent::BroadcastMeeting`，由`citizen_common.script`的`common_meeting_start/end`里程碑让党员去会议室/回家(线上召开时地址为空，脚本什么都不做)。

## 成员与权力
- 死亡/离城的党员每步清除；党首被移出则清空党首，不触发其它逻辑；新生/迁入市民不动态入党。
- `AssignPowers`校验：mod返回的姓名必须是党员、权力id必须已在`PowerCatalog`登记，否则忽略(日志`power_rejected`)。
  `PowersOf(citizen)`/`HoldersOf(powerId)`做C++查询。

## 随机与日志
- Core自己的随机用`CORE_RANDOM_SCOPE("core.polit")`；各党mod的流走`RandomRegistry::BindInstance`。
- 所有政治事件都写政治日志，见`polit_log.md`。

## 依赖关系
- 依赖：Map/Populace/Society、`PartyFactory`、`PolicyCatalog`、`PowerCatalog`。被`WorldStep`(`WorldDomains::polit`)调用。
