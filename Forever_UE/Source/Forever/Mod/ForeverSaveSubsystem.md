# ForeverSaveSubsystem.h / .cpp

## 职责

存档/读档的 UE 侧入口（`UGameInstanceSubsystem`）：文件读写、外层头、zlib 压缩、文件对话框、加载前校验和报错，
以及把通过校验的读档请求交给 World 关卡。Core 侧的 manifest、校验、版本表见 `Source/Core/common/save.md`，
存档系统总说明见 `Source/Core/common/save.md`。

## 关键设计

- **文件格式**：外层头 = `"FEVR"` + `SAVE_FILE_VERSION`(u32) + 未压缩长度(u64)，后面是 `FCompression`(zlib) 压缩的
  `SaveArchive`。扩展名 `.fsav`，对话框过滤器 `Forever Save (*.fsav)|*.fsav`。读的时候先查 magic 和文件版本
  （比当前新、比最低可读旧都拒绝），再解压、解析分段容器。
- **`RequestLoad` 的顺序**：读文件 → `SaveGame::ValidateArchive`（不改状态）→ `ApplyManifestConfig` 把存档的 mod
  集合设成当前配置 → `EnsureModsRegistered` → `ValidateModsRegistered` + `ValidateModStateVersions`。任何一步失败
  都恢复原配置、返回 false 和中文/英文的错误列表；全部通过才登记 `pendingLoad` 并 `OpenLevel(World)`。菜单和
  游戏内加载都走这一个函数。
- **做成 GameInstance 子系统**：读档要跨一次 `OpenLevel`，`pendingLoad` 必须活过关卡切换；
  `AForeverFrameworkActor::FetchActiveLoad` 开局时 `ConsumePendingLoad()` 取走（GameMode 找出生点时就会触发，比
  `BeginPlay` 早，见 `ForeverFrameworkActor.md`）。
- **保存**：从当前关卡里的 `AForeverFrameworkActor` 取 Map/Populace/Society/Traffic/Story 组成 `SaveWorld` 传给
  `SaveGame::BuildArchive`，再写 UE 段(JSON：玩家操控的市民 `controlledCitizen`、车辆 `controlledVehicle`、位置朝向
  `pawnX/Y/Z/pawnYaw`、视角 `controlPitch/controlYaw`、`weaponSlot`、`magazineAmmo`)；
  保存前先等后台寻路线程停下(`WaitForPendingPathfinding`)，再把场景里市民Actor的位置写回Core(`SyncPositionsToCore`，正在走路的市民按已经到达目的地写进存档，存完用`RestoreAfterSave`还原)；
  剧情对话进行中、谜题进行中拒绝保存(这些中间状态不进存档)；不在游戏关卡里（比如开始菜单）时拒绝保存。保存时把地图统计
  （`MapSaveCodec::GetLastReport`）和每个段的大小打进日志。
- **测试用控制台命令**：`Forever.Save [路径]`、`Forever.Load [路径]`，不给路径就弹对话框。正式入口是暂停界面
  （`UI/PausePanelWidget`，Tab）的保存/加载按钮和开始菜单的`LoadGameButton`（`UI/ConfigMenuWidget`）。
- **往返一致性检查**：控制台命令 `Forever.SaveRoundTrip`——当前世界写段 → 读回成新的 Map/Populace/Society/Traffic/Story(含脚本状态)/Player → 再写段，
  逐段比较字节(跳过 manifest)，结果打在日志和屏幕上（见 `Source/Core/map/map_save.md`"往返一致性"）。
- **当前进度**：全部域按存档恢复（P0–P6 完成）。

## 依赖关系

- 依赖：`Source/Core/common/save.h`、`Mod/ForeverDialogUtils.h`、`Mod/ForeverModSubsystem.h`；UE 的
  `Misc/Compression.h`、`Misc/FileHelper.h`。
- 被谁依赖：`Framework/ForeverFrameworkActor.cpp`（消费读档请求）；`UI/PausePanelWidget`（暂停界面）、`UI/ConfigMenuWidget`（开始菜单加载按钮）。
