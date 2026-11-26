# 待处理的 Review 条目

来自 2026-11-25 的全项目 review。当时列出的其它条目都已经修掉，或者你决定不处理；修法、规则和注意事项都写在各自的代码文档和 `CONVENTIONS.md` 里，这里只留还没处理的三条。

- 编号沿用初版：A = Core 感知具体 mod，G = 现有测试失败。
- 行号对应 2026-11-26 的工作区。
- **已确认** = 读过对应代码、行号核对过；**推测** = 有依据但没有实际复现或回退验证。

---

## A. Core 感知具体 mod

### - [ ] A1 每日需求的服务类型写死 `"food"`/`"video"`（已确认）
- 位置：[economy.cpp:34-38](Forever_UE/Source/Core/society/economy.cpp:34)、284、311-313
- 问题：`kMealServiceType="food"`、`kVideoServiceType="video"`，以及视频需求的财富阈值 `kVideoWealthLow/High`（2000 元/5000 元）都是 Core 常量。"市民需要什么服务、需要多少"是具体 mod（餐厅/视频公司）的业务。
- 影响：换一套组织 mod（没有 food/video 服务）后，Core 仍然只会产生这两种需求；新增一种服务类型必须改 Core。
- 建议：需求由 mod 声明。代码里的注释已经写了"先写死，以后由 scheduler 自定义"——比如让 `SchedulerMod` 或一个新的需求声明接口返回"(服务类型, 份数, 预算规则)"列表，Core 只做撮合。

### - [ ] A2 `AssignVehicles` 写死载具种类 `"car"`/`"truck"`（已确认）
- 位置：[traffic.cpp:567-568](Forever_UE/Source/Core/traffic/traffic.cpp:567)、587-588；成年年龄常量在 35 行
- 问题：成年市民 20% 概率分一辆 `"car"`，组织随机 1-5 次分 `"truck"`。种类字符串是 Basic 的 `vehicle_sedan`/`vehicle_truck` 注册的，Core 不该知道。这是 Core 里仅剩的两个载具种类名。
- 附带：成年年龄用自己的 `kAdultAge = 18`，而经济/选举用的是可调政策 `GovernmentPolicy::adultAge`，两处会不一致。
- 建议：改成由 mod 声明"谁拥有哪类车、概率多少"（例如 `OrganizationMod`/`SchedulerMod` 上的字段，或车辆 mod 注册时带"适用所有者"），Core 只负责找车位和建资产。

---

## G. 现有测试失败

CoreTest quick（`--suite all --quick`）现在是 7489 项通过、8 项失败，失败的全在 polit 套件。最近一次完整报告在 `Forever_UE/Saved/TestReports/review_20261126_a3/core`。

### - [ ] G2 polit 套件 8 项失败（推测与小地图配置有关）
- 只出现在 seed 12：两个党派创建时都没拿到会议室，后面的党代会、会议室冲突测试跟着失败。
- 11-24 的旧报告里 polit 是全过的。中间隔了提交 `7731158`（改了 `config.json`：小地图）。我没有回退验证，是推测。
- 值得看的点：地图上没有会议室房间时党派就没有会议室，这本身是个脆弱点（会议室只出现在 `ResidenceHighBuilding` 的一种布局里）。
