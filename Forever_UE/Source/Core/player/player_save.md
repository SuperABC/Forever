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

- **物品整棵树存**。每个物品记：类型（即 AssetMod 注册 id）、名字、数量、剩余使用次数（`usesLeft`，紧跟数量）、武器弹匣余量（`magazineAmmo`，紧跟`usesLeft`，-1=没用过）、mod 状态，以及它装着的东西（按名字排序，递归）。
- **物品重建**：
  - 读档时用原来的构造函数重建物品。重量、体积这些属性来自 mod 的 `SetProperty`，和新开局一样。
  - 装着的东西直接放回容器，不走 `AddContent` 的空间检查：存档里本来就装得下，这样 mod 改了尺寸也不会丢东西。
  - 类型已经不可用（mod 没了）的物品，连同它装着的东西一起丢弃，并打警告。
- **初始装备只在开局发一次**：装备由剧情脚本在 `game_start` 里用 `grant_asset` 发(UE 的 `PossessedBy` 不发)，读档时不广播 `game_start`，所以读档恢复出来的物品不会被覆盖。

## 依赖关系

- 依赖：`player/player.h`、`player/asset.h`（两者都把这个类声明成 friend）、`common/save_refs.h`。
- 被谁依赖：
  - `common/save.cpp`；
  - `Forever/Framework/ForeverFrameworkActor.cpp`（`EnsurePlayerGenerated`）；
  - `Forever/Mod/ForeverSaveSubsystem.cpp`（`Forever.SaveRoundTrip`）。

## 相关的 UE 侧状态

存在 `ue` 段，由 `ForeverSaveSubsystem` 写、`AForeverFrameworkActor::RestoreControlledPawn` 读：
- 玩家操控对象的位置和朝向、视角朝向；
- 激活的肩膀武器 `weaponSlot` 和弹匣余量 `magazineAmmo`。(读档时的弹匣余量现在以物品身上的`magazineAmmo`为准，此处是冗余的)读档后用 `AForeverCharacter::RestoreWeaponState` 改回存档时的状态。

## 资产的options与scriptStarted
`WriteAsset/ReadAsset`在mod状态之后追加`U32 options数量 + 各Str`、`U8 scriptStarted`；`ReadAsset`读完把脚本里的`name`变量改回存档里的名字。

## 天气
段末尾(worldAssets之后)追加：天气类型U8+剩余游戏毫秒F64，更早的存档没有就当晴天+新抽倒计时。没升版本号。见`weather.md`。

## 音乐音轨(2026-11-27)
天气之后追加：U32轨道数，每条 Str名字/Str asset/U8 loop/U8 paused/F64 position。读到段尾(更早的存档)就当没有音轨。
读回来每条取新的generation，UE第一帧就从存下的进度接着放。没升版本号。见`music.md`。

## 固定物品和地面摆位(2026-11-26)

- `WriteAsset`第一个字节区分普通物品(0)和固定物品(1)。固定物品没有`AssetMod`，把规格整份写出来(type、名字、网格、三轴缩放、
  `scriptModName`、里程碑名)，后面跟选项和`scriptStarted`；`ReadAsset`读到1就用`Asset(desc, name)`重建。不这样做的话，
  读档走`new Asset(&factory, type)`拿不到mod会把它丢掉。脚本变量仍在`world.scripts`段，键`asset:<名字>`。
- 房间里的物品和室外的物品，每件前面都写地面摆位(6个F32 + 1个U8的`restOnBottom`)，读回来`SetGround`。
  室内物品以前不存位置，读档后场景里看不到；现在存了，UE层按它流式生成实体。
- 读地图段不会重建房间物品(读档不调`Layout`，也不调`Map::BuildRoomAssets`)，房间里的物品全靠这一段恢复。
- 没升版本号、没写迁移，旧存档读不了。
