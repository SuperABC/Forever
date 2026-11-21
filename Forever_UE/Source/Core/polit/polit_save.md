# polit_save.h / polit_save.cpp

## 职责
政治域的存档读写：`PolitSaveCodec::Save/Load`，段名`world.polit`(`SAVE_SECTION_POLIT`)，版本见`GetSaveSectionVersions`。
不升版本、不写迁移(旧档缺这一段读不回，按项目约定接受)；读档失败时`ForeverFrameworkActor`退回重新生成。

## 段内容(二进制，按顺序)
`lastMonthKey`；政策值(按目录：id+值)；党派列表，每党：mod id、mod state、`meetingAddress`、`terms`、`ruling`、党首(市民下标)、
党员(市民下标列表)、当月党代会进度(`monthKey`/`day`/start/end/`started`/`ended`/`skipLogged`)、权力分配((权力id, 担任者下标))。

## 关键设计
- 市民用`Populace::GetCitizens()`的下标引用，所以必须在Populace按同一份存档恢复之后读。
- 会议室只存地址，读档时`Map::LocateRoom`重新解析，**不再调`ChooseMeetingRoom`**。
- 政策值通过`PolicyCatalog::Apply`写回(同样夹[min,max])。
- `Polit::Load`成功时已经`Bind`好。

## 依赖关系
- 被谁依赖：`save.cpp`(BuildArchive/Load)、`ForeverFrameworkActor`(读档)、`ForeverSaveSubsystem`(往返校验)、`suite_save.cpp`(CheckPolitRoundTrip)。
