# party_mod.h

## 职责

`PartyMod`：一个党派的行为定义——有多少党员、每次选举前提什么政策目标、什么时候开党代会、在哪开、赢得选举后把哪些权力分给哪些党员。
政治域(`polit`)的第一个concept，Core侧的领域类是`Core/polit/party.h`，管理者是`Polit`，见`Core/polit/polit.md`。

## 关键设计

- **接口只用字符串/数值**(Dependence看不到`Citizen`/`Room`)：党员用姓名、会议室用房间地址、政策和权力用id字符串。
- **容器约定**：Core传给mod的`const&`容器mod只读；mod的输出(`targets`/`assignments`/`meetingAddress`)写在mod自己的成员里，Core只复制读走，
  **不修改**。所以`Propose`/`AssignPowers`实现必须自己在方法开头`clear`输出成员(Core不会替它清，避免跨DLL释放，见memory cross_dll_allocator_crash)。
- `memberCount`：`SetProperty()`里填，`PartyFactory::CreateParty`创建后立刻调。新开局时Core从成年市民里按这个数量随机抽党员，跨党互斥，池不够抽到没人为止；党员只会被移出，不动态加入。
- `Propose(current)`：每次选举前调。mod在`targets`里填**目标值(绝对值)**，如共和党的企业税目标是50(%)，它就一直提议50。Core按`clamp(target/当前值, 0.9, 1.1)`换算成比例，所以一次最多调整±10%；没出现在`targets`里的政策=没有主张(比例1)。接口收到当前值，所以以后可以写"按现状动态调整"的党派。
- `GetCongress(year, month)`：每月初调一次，返回`{day, start, end}`，`day==0`=本月不开。`start/end`只取时分秒，同一天内。两个内置党派都写死每月1号12:00–15:00方便调试。
- `ChooseMeetingRoom(post)`：创建城市时调，mod自己用`post`查询(如`{"post":"random room","type":"room_meeting"}`)挑房间，地址写进`meetingAddress`。Core检查地址有效且没被别的党占用，重叠则重调，最多4次，仍不行就当没有会议室。读档直接用存档里的地址，不再调。以后党派可以自带一个building mod+room mod、直接指定那个唯一的room，就不会重叠。
- `AssignPowers(ctx)`：赢得选举(成为执政党)时调，`ctx.powers`是Core动态提供的全部权力词汇(`PowerInfo{id,name,policy}`)，mod返回"党员→权力"。一个党员可以担任0/1/多项权力，一项权力可以没人或多人担任；`policy`为空的权力(与可调政策无关，如以后的"发放游戏版号")内置党派不指派。`ctx.leader`可为空，党首目前不参与指派。城市运转不依赖任何职位，权力只是数据(以后做贪腐用)。
- 存档：同其它concept，`SaveState/LoadState/GetStateVersion/GetInstanceCounter`，见`common/mod_state.md`。

## 依赖关系

- 依赖：`common/mod_test.h`、`common/mod_state.h`、`common/utility.h`(`Time`)、`common/handle.h`(`PostHandle`)。
- 被谁依赖：`party_factory.h`；`Core/polit/*`消费；具体实现在`Source/Basic/polit/party_*.h`。
