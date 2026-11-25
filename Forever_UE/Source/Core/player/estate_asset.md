# estate_asset.h / estate_asset.cpp

## 职责

`EstateAsset`：不动产资产（`AssetMobility::Estate`），`Asset`的Core内置子类，**没有`AssetMod`**——不动产没有
mod的概念，指向的是已经存在的`Zone`/`Building`/`Room`，本身不拥有它们。名字取被指对象的`GetAddress()`。

## 持有与挂接

- **`Map`持有**（`Map::estateAssets`，析构时delete，`Asset`有虚析构）。
- **`Map::BuildEstateAssets()`建立**：按`Map::Checkin`写下的归属（`owner`/`stated`）扫描，只记"最高层"——
  园区整体归属就只有园区一个资产，不再给下属建筑/房间各建一个；建筑整体归属同理；房间只有在和所在建筑归属不同
  （各自独立分配）时才建。私有归属挂进该`Citizen::GetAssets()`，公有(`stated`)的收进`Map::GetStatedEstateAssets()`，
  由框架交给`Society::SetGovernmentAssets`（政府资产列表）。
- **不存档**：完全由已存的`owner`/`stated`推导，读档在人口/社会恢复后同样调用`BuildEstateAssets`重建。
- 为什么是"事后一次扫描"而不是"指定owner时就建"：`Checkin`发生在`Society`创建之前（政府的列表在`Society`），
  且读档本来就要重建，一次扫描同时覆盖新游戏和读档。

## 注意

- 组织不能拥有不动产（房产`owner`只能是市民，`stated`=政府），所以组织的资产列表里没有`EstateAsset`。
