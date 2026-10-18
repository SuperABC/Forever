# save.h / save.cpp

## 职责

存档/读档的 Core 侧编排：manifest、段版本表、mod 状态的 JSON 读写实现、加载前校验、把存档的 mod 集合应用到
当前 Config、恢复 mod 命名计数器。UE 侧的文件、压缩、对话框在 `Source/Forever/Mod/ForeverSaveSubsystem`。
整体方案和分阶段计划见仓库根目录 `SAVE_PLAN.md`。

## 当前进度（P0–P6，全部域都已存档）

存档里有这些段：
- `manifest`；
- 地图 4 段：`world.map.terrain/roadnet/structures/vegetation`，见 `Source/Core/map/map_save.md`；
- `world.populace`，见 `Source/Core/populace/populace_save.md`；
- `world.society`，见 `Source/Core/society/society_save.md`；
- `world.traffic`，见 `Source/Core/traffic/traffic_save.md`；
- `world.scripts`，即全部 Script 的状态，见 `Source/Core/story/script_save.md`；
- `world.player`，见 `Source/Core/player/player_save.md`；
- `ue`，即 UE 侧状态（JSON），由 `ForeverSaveSubsystem` 写：玩家操控的市民和车辆、位置朝向、视角、激活的肩膀武器、弹匣余量。

`BuildArchive`/`BuildManifest` 接收 `SaveWorld`（map/populace/society/traffic/story/player 指针，没生成的传 nullptr，对应的段就不写）。布局模板名取自各建筑的 `GetFloorSpecs` 快照。

读档时全部域按存档恢复，并跳过 game_start 广播。

段版本表里的 `required` 字段：
- `required=true` 的段缺失时，`ValidateArchive` 直接拒绝。比如只有 manifest 的 P0 旧存档。
- 人口、社会、交通、脚本状态、玩家、ue 段都是 `required=false`：较早阶段的存档没有这些段，读档时对应的域重新生成，没有脚本状态段时 game_start 照常广播。这就是计划里"未序列化的域临时回退到重新生成"的开关，现在作为旧存档的兼容方式保留，新存档里所有段都有，不会触发回退。

## 跨域引用

`common/save_refs.h`（`SaveRefs` 命名空间）统一各域段之间的引用编码，只用各类的公开接口：
- Room/Component：（所属 Building 名字, 在 `GetRooms()`/`GetComponents()` 里的下标）；
- Time：年月日时分秒毫秒逐个写。

同一个文件里还有几个读写工具：
- `LiveIds`/`LookupId`：mod 实例到注册 id 的映射；
- `WriteModState`/`ReadModState`：mod 状态，即状态版本 + JSON 文本。

## 关键设计

- **manifest**（`SaveManifest`）：
  - `mods`：按 `(config.json 的 "<concept>_mods" key, modId)` 记，带参数字符串、`stateVersion`、`counter`。
    同一个 id 可能在不同 concept 里重名，所以必须带 concept。当前取"保存时启用的全部 mod"，比"真正用到的"略宽，
    后续阶段可以收窄。
  - `storyScripts` / `mainStory`：Story screen 的脚本只存名字（不含路径/扩展名），读档时按 `Config::GetScriptPath`
    重新解析成本机路径。
  - `scripts` / `layouts`：资源检查用的脚本名、`.layout` 模板名（模板名来自存活建筑 mod 的 `floors`）。
  - `gameVersion`：只用于显示和排查。
- **存档里不出现文件路径**：脚本、布局、mod 都只存名字和 id；`dll_paths/layout_paths/resource_paths` 不进存档。
- **段版本表**（`GetSaveSectionVersions`）：每个段一行 `{名字, current, minimum}`。**改了某个段的数据布局就必须
  升这一段的 current，并在这里补一个从旧版本升上来的迁移函数**；不认识的段、比 current 新、比 minimum 旧的段
  都在 `ValidateArchive` 里拒绝。
- **21 个 concept 的统一操作表**（`GetConceptOps`，模板 + 成员函数指针生成）：枚举存活实例（计数器/状态版本）、
  临时建实例取状态版本、恢复计数器、检查是否已注册。
- **加载前校验分两步**：
  1. `ValidateArchive`（不改状态）：段版本 → manifest → 每个 `(concept, id)` 都有已发现的 DLL 提供 → 每个 mod
     声明的依赖也在存档的 mod 集合里 → 脚本/布局资源都能在配置的目录里找到。
  2. `ApplyManifestConfig` 把存档的 mod 集合/剧情脚本设成当前 Config（只改内存，不写回 config.json）并
     `ReloadModArgs`，之后 `ValidateModsRegistered`（Registry 只在第一次 `Get()` 加载 DLL，游戏内读档需要启动时
     没加载的 DLL 会在这里报出来，提示重启）和 `ValidateModStateVersions`（只对 `stateVersion>0` 的 mod 临时建
     实例比较）。任何一步失败调用方用返回的快照 `RestoreConfig` 恢复。
- **命名计数器恢复**：`RestoreInstanceCounters` 由 `AForeverFrameworkActor::EnsureMapGenerated` 在地图按存档恢复之后、
  其余域重新生成之前调用，重新生成的域新建的 mod 实例会从存档时的编号接着往下编。
- **JsonStateWriter/Reader**：Writer 用"层级栈 + 闭合时挂回父节点"的方式构建 JSON（不持有指向容器内部的指针）；
  Reader 只做 const 访问。二进制数据用 base64 存成字符串。

## 依赖关系

- 依赖：`Dependence/common/{save_archive,mod_state,json}.h`、`common/config.h`、`common/registry.h`。
- 被谁依赖：`Source/Forever/Mod/ForeverSaveSubsystem`。
