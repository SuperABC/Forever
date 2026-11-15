# vehicle_asset.h / vehicle_asset.cpp

## 职责

`VehicleAsset`：载具资产（`AssetMobility::Vehicle`），`Asset`的Core内置子类，**没有`AssetMod`**——载具之间的差异
已经由`VehicleMod`承担，不需要每个载具mod再配一个资产mod。指向一辆`Vehicle`（不拥有它，`Vehicle`归`Traffic`），
名字取车名；记录所有者（`ownerCitizen`或`ownerOrganization`，都空=政府，目前没有）。

## 持有与挂接

- **`Traffic`持有**（`Traffic::vehicleAssets`，`DestroyVehicle`/`CreateVehicle`覆盖同名车时同步删除并从所有者列表摘除）。
- 分配规则：市民可有`car`，组织只有`truck`，见`traffic.md`"AssignVehicles"。机场的飞机不挂资产（没有机场组织，等有了再改）。
- 存档：随traffic段，见`traffic_save.md`。
