# ForeverAssetFrameworkComponent

## 职责

世界掉落物(`AAssetElement`)的生成/销毁登记——背包UI的Drop/Pick动作分别调用
`SpawnWorldAsset`/`DestroyWorldAsset`，见`Source/Forever/UI/InventoryWidget.h`。这个
组件不持有`Asset`(Core层类型)的所有权，真正所有权在`Room::assets`或某个容器的`contents`
里；这里只登记"哪个`Asset*`对应世界里哪个`AAssetElement*`"，方便Pick时反查到Actor销毁。

## 关键设计

- `worldAssetActors`(`TMap<Asset*, AAssetElement*>`)不能标`UPROPERTY`——`Asset*`是Core层
  的普通C++类型，不是`UObject`/`USTRUCT`，UHT反射不认。生命周期不受影响：`SpawnActor`
  出来的`AAssetElement`由World自己的Actor列表持有，不依赖这张表的引用来防止GC，和
  `ForeverWeaponComponent::currentWeapon`(`WeaponMod*`)同一个道理。
- `SpawnWorldAsset`只负责`SpawnActor`+`Init`+登记进表；`DestroyWorldAsset`只负责查表
  `Destroy()`+从表里移除，都不管`Asset`本身的生命周期——调用方/`Room`负责`Asset`的创建
  和释放。
- `AAssetElement`默认用项目通用的Cube占位网格(`/Game/Asset/Meshes/Cube.Cube`，和
  `ABuildingElement`同一份资产)；资产的`AssetMod::meshPath`非空时改用它指定的网格和`meshScale`缩放
  (比如投掷物的球体)。带物理模拟+重力(Movable + 碰撞预设能被玩家推动)，不是纯摆设的Static网格。
- `SpawnWorldAsset`用`SpawnActorDeferred`：先`Init`换好网格、把`meshScale`写进Transform再
  `FinishSpawning`，物理体才会按最终的网格和缩放创建(Actor的根就是网格组件，缩放必须走Transform，
  `FinishSpawning`会用传入的Transform覆盖根组件的缩放)。
- `SpawnThrownAsset`：投掷。同一个Actor，额外`Launch`给初速度(开CCD、忽略投掷者)。Asset先记进
  `Player::worldAssets`(房间外的物品，所有权归Player，不进`Room::assets`，落点在室内也一样)再生成Actor；
  同名已存在则整个失败，不留没有归属的Actor。扔出去之后不会自动消失(落地后的效果由以后的物品自己设计)。
- `SyncWorldAssetsToCore`/`RestoreWorldAssets`：存档前把场景里这些Actor的实际位置朝向写回
  `Player::worldAssets`(`ForeverSaveSubsystem`的保存流程里，和市民位置同步一起)；读档后按记的位置朝向
  重新生成Actor，**速度不存**，从静止开始模拟(`AForeverFrameworkActor`读档分支里`RestoreControlledPawn`之后调用)。
  只有有Actor的物品才同步，没有Actor的保留上一次记的位置。注意：读档时地形碰撞还没就绪的话，物品可能掉穿，
  需要PIE确认。

## 依赖关系

- 依赖：`Source/Forever/Element/AssetElement.h`(`SpawnActor`的目标类型)。
- 被谁依赖：`Source/Forever/UI/InventoryWidget.h`(Drop/Pick交互)。

## 待办/后续阶段

- 背包界面的丢弃：房间里进`Room::assets`，室外进`Player::worldAssets`；拾取仍只认房间(室外的物品捡不起来，要做的话需要房间栏显示附近的室外物品)；除投掷物外的资产仍统一用Cube占位；其余资产的真实网格/图标留作后续美术资产到位后，在各自的
  `AssetMod`里填`meshPath`即可。
