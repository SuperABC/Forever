# player_save.h / player_save.cpp

## 职责

玩家域的存档读写，类名 `PlayerSaveCodec`，写成二进制段 `world.player`。内容：
- 游戏时钟；
- 跨天判断用的 `day`；
- 时间流速；
- 生命值；
- 5 个身体槽位里的物品：左手、右手、背包、左肩、右肩；
- 房产里的物品；
- 当前所在房间；
- 散落在各个房间里的物品（`Room::assets`）。物品属于玩家域，所以存在这一段，不存在地图段；
- 房间外的物品（`Player::worldAssets`，比如被扔出去的投掷物）：每个记位置朝向（6 个 float，UE 世界坐标）和整棵物品树。**不存速度**，读档后 UE 侧按位置朝向重新生成 Actor，从静止开始模拟。这一块在段的末尾，更早的存档没有它，读到末尾就当没有。

## 关键设计

- **物品整棵树存**。每个物品记：类型（即 AssetMod 注册 id）、名字、数量、mod 状态，以及它装着的东西（按名字排序，递归）。
- **物品重建**：
  - 读档时用原来的构造函数重建物品。重量、体积这些属性来自 mod 的 `SetProperty`，和新开局一样。
  - 装着的东西直接放回容器，不走 `AddContent` 的空间检查：存档里本来就装得下，这样 mod 改了尺寸也不会丢东西。
  - 类型已经不可用（mod 没了）的物品，连同它装着的东西一起丢弃，并打警告。
- **初始装备只在开局发一次**：`AForeverCharacter::PossessedBy` 只在玩家槽位全空时才发初始装备，所以读档恢复出来的物品不会被覆盖。
- **可以缺省**：`required=false`，没有这一段时按新开局生成玩家。

## 依赖关系

- 依赖：`player/player.h`、`player/asset.h`（两者都把这个类声明成 friend）、`common/save_refs.h`。
- 被谁依赖：
  - `common/save.cpp`；
  - `Forever/Framework/ForeverFrameworkActor.cpp`（`EnsurePlayerGenerated`）；
  - `Forever/Mod/ForeverSaveSubsystem.cpp`（`Forever.SaveRoundTrip`）。

## 相关的 UE 侧状态

存在 `ue` 段，由 `ForeverSaveSubsystem` 写、`AForeverFrameworkActor::RestoreControlledPawn` 读：
- 玩家操控对象的位置和朝向、视角朝向；
- 激活的肩膀武器 `weaponSlot` 和弹匣余量 `magazineAmmo`。读档后用 `AForeverCharacter::RestoreWeaponState` 改回存档时的状态。
