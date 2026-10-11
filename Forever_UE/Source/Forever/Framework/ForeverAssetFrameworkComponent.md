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
- `AAssetElement`当前一律用项目通用的Cube占位网格(`/Game/Asset/Meshes/Cube.Cube`，和
  `ABuildingElement`同一份资产)，不区分具体资产类型，带物理模拟+重力(Movable + 碰撞预设
  能被玩家推动)，不是纯摆设的Static网格。

## 依赖关系

- 依赖：`Source/Forever/Element/AssetElement.h`(`SpawnActor`的目标类型)。
- 被谁依赖：`Source/Forever/UI/InventoryWidget.h`(Drop/Pick交互)。

## 待办/后续阶段

- `AAssetElement`的可视化目前统一用Cube占位，不区分具体资产类型；按资产类型换真实网格/
  图标留作后续美术资产到位后再做。
