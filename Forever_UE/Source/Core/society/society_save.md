# society_save.h / society_save.cpp

## 职责

社会域的存档读写，类名 `SocietySaveCodec`，写成一个二进制段 `world.society`。内容：
- 组织：类型、占用的 Component、mod 状态、mod 名字、股东名单（市民下标+持股数）、倒闭标记、当前总股数(I64)；
- 每个组织的 Job：类型、工位房间、在职市民、mod 状态、mod 名字；
- 组织定时器；
- 账本：城市外/政府余额、市民和组织余额（按下标引用）、累计流水 `flowTotals`、开局注资 `initialInjection`；
- `lastMonthKey`（月结进度）；
- 政府负债簿 `Treasury`：天序号、存款表（市民下标升序）、国债表（持有人、本金、年利率基点、到期天序号）；
- 股市 `Market`：每支股票的价格系数和每股分红（组织下标升序）。挂单是当天的临时数据，不存档。

存档系统总说明见 `Source/Core/common/save.md`，人口部分见 `Source/Core/populace/populace_save.md`。

## 关键设计

- **存结果**：读档时不跑 `Society::Init`、`RecruitCitizens`、`GenerateEmploymentHistory`。
- **重建组织**：用"空 Component 列表"调用原来的构造函数。这样只创建 mod 和 Script，不会按 Component 重新设计岗位。之后把存档里的 Component 和 Job 原样填回去。
- **重建 Job**：用原来的构造函数（注册 id + 工位房间），再调用 `SetOccupant`/`Citizen::SetJob` 连回在职市民。
- **mod 名字**：
  - Job 和组织脚本里的 `$$self.name` 是 mod 自己的 `GetName()`（按命名计数器生成）。
  - 读档时重新创建的 mod 会拿到新名字，所以存档里另存一份，读档后用 `script->SetValue("name", …)` 改回存档时的名字。
  - mod 内部的名字仍是新的。Core 只在脚本变量里用到这个名字。
- **引用编码**：
  - 市民按 `populace->GetCitizens()` 的下标引用，所以必须和人口段一起恢复；`AForeverFrameworkActor` 只在人口也是按存档恢复的时候才读社会段。
  - Component 和房间用 `common/save_refs.h` 里的（Building 名字, 下标）编码。
- **工作经历**：读完后，把人口段留下的 `JobExperience` 连上组织（`JobExperience` 把这个类声明成 friend）。
- **定时器顺序**：组织定时器按 (时间, 组织下标, 节点名) 排序后写出，原因同人口段。
- `Load` 用 `unique_ptr<Society>` 暂存新建的 `Society`，失败路径（段截断、异常）自动析构，成功才 `release`。

## 依赖关系

- 依赖：`society/*`、`populace/*`、`story/script.h`（`SetValue`）、`common/save_refs.h`、`common/save.h`。
- 被谁依赖：
  - `common/save.cpp`；
  - `Forever/Framework/ForeverFrameworkActor.cpp`（`EnsureSocietyGenerated`）；
  - `Forever/Mod/ForeverSaveSubsystem.cpp`（`Forever.SaveRoundTrip`）。

## 待办 / 后续

- 组织和 Job 的 Script 状态由 `story/script_save` 恢复（键 `org:<i>`、`job:<i>:<k>`）。

## 岗位相关字段（追加，不升版本）

每个Job记录末尾追加`U8 closed`；组织记录在`I64 totalShares`之后追加`I64 lastSettledBalance`和一个字符串——mod岗位计划`vacancies`，由基类虚函数`OrganizationMod::SaveVacancies`写成JSON文本（`[room,type,room,type,...]`），读档时`LoadVacancies`在mod一侧还原，Core不碰`vacancies`的内存。
