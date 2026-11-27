# ForeverAssetFrameworkComponent

## 职责

地上的物品在场景里的实体(`AAssetElement`)：登记、**按玩家距离流式生成/销毁**，以及物品相关Change的执行。
这个组件不持有`Asset`(Core层类型)的所有权，真正所有权在`Room::assets`(房间里的)或`Player::worldAssets`(室外的)。

## 流式加载(2026-11-26)

- **登记表**：每件"在地上"的物品一条`FGroundEntry`(物品指针、名字、最后已知位置朝向、实体弱引用、生成时间)，
  `groundEntries`数组 + `entryIndexByAsset`/`assetByName`两张索引。`Asset*`是Core的普通C++类型，不能标`UPROPERTY`；
  实体用`TWeakObjectPtr`，被引擎销毁(掉出世界等)后自动失效，下次进入范围按最后已知位置重新生成。
- **`RegisterGroundAssets()`**：开局和读档各调一次(`AForeverFrameworkActor`的"构建载具/恢复载具与玩家"阶段)，
  扫所有房间的`Room::assets`和`Player::worldAssets`，只登记不生成。房间mod布局时放的物品(床、仓库里的汉堡)、
  读档恢复的室内外物品都从这里进表——读档后室内物品看不见的问题就是这样解决的。
- **归属**：登记项记着物品归哪个房间(`FGroundEntry::room`，`Room::assets`里的)，`nullptr`=`Player::worldAssets`(室外的、扔出去的)。
  `SpawnWorldAsset`按"是不是在玩家当前房间的`Room::assets`里"判断(在房间里丢下/拖出来的都进当前房间)。
- **离开房间就不再属于房间**(`DetachFromRoom`，判断在`ABuildingElement`)：两种情况——可移动物品的实体出了它所在房间的触发盒(`OnRoomOverlapEnd`，物品网格开了重叠事件；物品自己的选项触发盒、物品流式销毁、房间盒随近处LOD销毁补发的EndOverlap都不算，最后一种靠"物品原点还在盒子里"排除)；或者房间盒刚建好时，这个房间在Core里持有的物品此刻位置已经不在盒子里(盒子没建时被推出去的，`DetachAssetsOutside`，位置用`GetCurrentLocation`：有实体取实体位置，没有取Core摆位，布局后还没生成过的不判断)。转的时候`Room::RemoveAsset`→`Player::AddWorldAsset`(有实体先把这一刻的实际位置写回Core)，登记项`room`置空、选项总能弹、进背包"附近"列表。被推进隔壁房间也一样变成室外物品，不会转进那个房间。室外已有同名物品时留在原房间。
- **`TickComponent`**(世界生成完之前不动，因为楼层地板要等那时才有碰撞)，以玩家Pawn为参考点：
  - **玩家换房间**(轮询`Player::currentRoom`，它由房间触发盒设置；读档后无条件重做一次)→`OnPlayerRoomChanged`：
    离开的房间的固定物品撤掉实体，进入的房间里**有脚本的**固定物品生成实体(只有弹选项的触发盒，第一次生成时补发`game_start`)；
    所有可移动物品的实体重设`SetOptionsAllowed`——房间里的物品只在玩家就在那个房间里时弹选项，触发盒伸过墙也不会在隔壁房间弹。
  - **固定物品不按距离流式**：显示和阻挡跟房间家具一起由`ABuildingElement::BuildRoomFurniture`建进ISM，随建筑近处LOD和楼层预算加载/拆掉；
    没有脚本的固定物品从来没有实体。
  - **可移动物品按水平距离流式**：小于`assetSpawnDistance`(4地图单位=40米)且没有实体就生成，大于`assetDespawnDistance`(8=80米)
    且有实体就`WriteBackToCore`(把实际位置朝向写回`Asset`的地面摆位)再销毁。每帧最多`maxAssetOpsPerTick`(4)个生成+销毁，
    预算用完就停，下一帧从停下的下标(`streamCursor`)接着扫(一张256的地图大约4000条，扫一遍很便宜)。
    `minAssetLifetime`(2秒)内不销毁，这是Chaos物理代理的安全要求。
- **预热(`WarmUpAssets`，加载界面"预热物品"阶段，在`RegisterGroundAssets`之后)**：收集网格路径——每种已启用物品mod的(`Asset::CollectRegisteredMeshes`，每种临时建一个mod实例，命名计数器跳一个号、不恢复)、房间里和室外现有物品的、房间家具的；物品网格用`AAssetElement::LoadAssetMesh`加载并存进`warmedMeshes`(UPROPERTY，防GC)，家具和固定物品的网格走建筑组件的`ResolveMesh`进它的缓存；编辑器里`FStaticMeshCompilingManager::FinishCompilation`等网格编译完；每种物品网格(含Cube占位)用`AAssetElement::SpawnWarmup`在高空临时生成一个实体、同一帧销毁(同`WarmVehicleClasses`)。
- **`SpawnWorldAsset(asset, location, rotation)`**：把物品放到地上=登记 + 把落点写进Core + 立刻生成实体。丢弃、投掷、
  背包面板拖出来都走它(都发生在玩家身边)。**`DestroyWorldAsset(asset)`**：物品离开地面=取消登记 + 销毁实体(没有实体就只取消登记)。
  **任何会delete一件"在地上"的`Asset`的地方都必须先调`DestroyWorldAsset`**，否则登记表里留着悬空指针，Tick里会解引用它。
- **`SpawnElementFor`**：先`StartAssetScript`(补发`game_start`，脚本一般在这时`add_option`)，再重新找登记项(脚本可能把物品弄走)，
  然后`SpawnActorDeferred`+`Init`+`FinishSpawning`。固定物品的生成高度加1cm(同家具)；`restOnBottom`的可移动物品按网格包围盒
  把原点抬到"底面在给定高度上方2cm"，并把实际位置写回Core、清掉`restOnBottom`。
- **`SyncWorldAssetsToCore`**：存档前把所有有实体的物品(室内室外)的位置写回Core；没有实体的保留上一次写回的位置。
- `FindElementByName`只找得到当前有实体的物品。`spawn_effect`/`sound_effect`/`area_damage`的位置解析(`ResolvePosition`)
  找不到实体时退回登记表里的最后已知位置(比如手雷到点时玩家已经走远)。
- 固定物品(`Asset::IsFixture()`)：实体是静止的(见`AssetElement.md`)，从不写回位置；`pick_asset`/`destroy_asset`对它无效
  (`DetachGroundAsset`当它不在地上)。

## 关键设计

- `AAssetElement`默认用项目通用的Cube占位网格(`/Game/Asset/Common/Mesh/Cube.Cube`，和
  `ABuildingElement`同一份资产)；资产的`AssetMod::meshPath`非空时改用它指定的网格和`meshScale`缩放
  (比如投掷物的球体)。带物理模拟+重力(Movable + 碰撞预设能被玩家推动)，不是纯摆设的Static网格。
- `SpawnWorldAsset`用`SpawnActorDeferred`：先`Init`换好网格、把`meshScale`写进Transform再
  `FinishSpawning`，物理体才会按最终的网格和缩放创建(Actor的根就是网格组件，缩放必须走Transform，
  `FinishSpawning`会用传入的Transform覆盖根组件的缩放)。
- `SpawnThrownAsset`：投掷。同一个Actor，额外`Launch`给初速度(开CCD、忽略投掷者)。Asset先记进
  `Player::worldAssets`(房间外的物品，所有权归Player，不进`Room::assets`，落点在室内也一样)再生成Actor；
  同名已存在则整个失败，不留没有归属的Actor。扔出去之后不会自动消失(落地后的效果由以后的物品自己设计)。
- `CollectNearby(center, radiusCm, out)`：背包"附近"列表用。球形重叠查询(`ECC_PhysicsBody`对象类型，因为`AAssetElement`网格用`PhysicsActor`预设)走引擎物理宽相位，代价正比于附近物品数而不是`worldAssets`总量；只收归`Player::worldAssets`持有的(房间里的物品归`Room`，不进该列表)，位置取Actor实时位置，按距离从近到远排序。

## 依赖关系

- 依赖：`Source/Forever/Element/AssetElement.h`(`SpawnActor`的目标类型)、Core `player/player.h`/`player/asset.h`/`map/room.h`、`ForeverStoryFrameworkComponent`(`StartAssetScript`/`NotifyAssetEvent`/`NotifyAssetResult`)。
- 被谁依赖：`Source/Forever/UI/InventoryWidget.h`(Drop/Pick交互)、`AForeverFrameworkActor::ApplyChange`(四个资产Change)、存档/读档流程。

## 脚本驱动的背包Change

`ApplyPickAsset/ApplyGrantAsset/ApplyDeductAsset/ApplyDropAsset`(由`AForeverFrameworkActor::ApplyChange`转发)。pick：在房间根/`worldAssets`里找到资产并摘下→`Player::StoreAsset`→成功`DestroyWorldAsset`+`get_asset(success)`，失败放回原处+`get_asset(fail)`。grant：`CreateAsset`+`SetCount`+`StoreAsset`。deduct/drop：`TakeOwned`，drop再`SpawnWorldAsset`到玩家身前。`FindElementByName(name)`按资产名找场景里的`AAssetElement`(带脚本的资产`SpawnWorldAsset`时先补发game_start再`Init`烘焙选项)。

## spawn_effect / destroy_asset（2026-11-17）

`ApplySpawnEffect`：`effect`留空直接返回(没有默认特效)；按`asset`字段(缺省=`context.self`的`name`变量)`FindElementByName`取Actor位置，`AForeverExplosionEffect::Spawn(world, 位置, UpVector, scale, 蓝图路径)`；找不到Actor打Warning。
`ApplyDestroyAsset`：登记`pendingDestroy`(存名字不存指针)并立刻`SetActorHiddenInGame`+关碰撞；`FlushPendingDestroy`(`AForeverFrameworkActor::Tick`在`FireExpiredTimers`之后调用，此刻没有脚本在执行)按名字`DetachGroundAsset`→`DestroyWorldAsset`→`Player::DestroyAsset`。延迟的原因：同一个里程碑里后面的Change还要用被销毁资产的脚本(`context.self`)。

## 堆叠合并与QueueDelete（2026-11-17）

`ApplyPickAsset`/`ApplyGrantAsset`遇到`StoreResult::Merged`(并进了背包里同类堆叠，见`Player::StoreAsset`)：先发`get_asset(success)`，再`QueueDelete`延迟销毁这件已无用的资产(不能立刻delete：执行pick的脚本可能就是它自己的)，`FlushPendingDestroy`里统一delete。

## ApplySoundEffect（2026-11-18）

`sound_effect`的UE层：`ResolvePosition`求位置(规则同`ApplySpawnEffect`)，`sound`按UE资产路径`LoadObject<USoundBase>`(不含`.`补对象名)，`UGameplayStatics::PlaySoundAtLocation(volume, pitch)`。加载不到/没有位置只打Warning。
