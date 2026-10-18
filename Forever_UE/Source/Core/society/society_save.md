# society_save.h / society_save.cpp

## 职责

社会域的存档读写，类名 `SocietySaveCodec`，写成一个二进制段 `world.society`。内容：
- 组织：类型、占用的 Component、mod 状态、mod 名字；
- 每个组织的 Job：类型、工位房间、在职市民、mod 状态、mod 名字；
- 组织定时器。

整体方案见 `SAVE_PLAN.md`，人口部分见 `Source/Core/populace/populace_save.md`。

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
- **可以缺省**：`required=false`，没有这一段时重新生成社会。如果人口是按存档恢复的，就不再跑 `GenerateEmploymentHistory`，因为经历和同事关系里已经有这些内容了。

## 依赖关系

- 依赖：`society/*`、`populace/*`、`story/script.h`（`SetValue`）、`common/save_refs.h`、`common/save.h`。
- 被谁依赖：
  - `common/save.cpp`；
  - `Forever/Framework/ForeverFrameworkActor.cpp`（`EnsureSocietyGenerated`）；
  - `Forever/Mod/ForeverSaveSubsystem.cpp`（`Forever.SaveRoundTrip`）。

## 待办 / 后续

- 无。组织和 Job 的 Script 状态由 `story/script_save` 恢复（键 `org:<i>`、`job:<i>:<k>`）。
