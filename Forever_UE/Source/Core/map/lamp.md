# lamp.h / lamp.cpp

## 职责

`Lamps`是全图路灯的统一存储：把宿主(目前只有路网)提交的`LampRequests`(单盏/一排/一片，定义在
`Source/Dependence/map/feature_spec.h`)按宿主参考系换算后展开成一盏盏`LampInstance`(位置、世界朝向、蓝图下标)。
由`Map`持有(`Map::GetLamps()`)，UE层`UForeverLampFrameworkComponent`读出来逐盏生成`ALampElement`蓝图Actor。

## 关键设计

- **完全仿`Vegetation`**——展开规则(`ExpandLine`沿线段含两端均匀取点、`num==1`取中点；`ExpandArea`在矩形内均匀
  随机再绕中心旋转)、`PlaceFilter`过滤、`TreeFrame`宿主参考系都是同一套(直接复用`vegetation.h`的`TreeFrame`)，
  灯不是Mesh而是Actor蓝图，所以登记的是蓝图路径(`RegisterBlueprint`去重，`blueprintIndex`指向`GetBlueprintPaths()`)。
- **朝向由请求指定，不做随机旋转/缩放**——请求里的`rotationDegrees`是宿主局部系下的度数，展开时转成弧度并叠加宿主旋转
  (`TreeFrame::rotation`)，得到`LampInstance::yaw`(弧度，世界朝向，数学角0=+X逆时针，UE层直接转成`FRotator`的yaw)。
- **过滤沿用`Map::CanPlaceTree`**：越界、有水面、有hatch的格子不放灯，过滤掉的直接跳过。
- **`Map::InitLamps()`紧跟`InitVegetation()`**(开口已齐全)：`roadnet->LayoutLamps()`(内部调
  `RoadnetMod::LayoutLamps(hostRoads)`再拷贝`mod->lamps`)后用路网的整图参考系展开。园区/建筑宿主目前没接，要接时照
  `InitVegetation`的写法给`ZoneMod`/`BuildingMod`加`LayoutLamps`/`lamps`即可，`ExpandRequests`已经与宿主无关。
- **不存高度**——Z由UE层按`Map::SampleHeight`取。`id`从0递增。
- **存档**：`world.map.lamps`段(蓝图路径表、全部`LampInstance`、`nextId`)，读档直接还原，不重新布置，见`map_save.md`。

## 依赖关系

- 依赖：`map/feature_spec.h`(Dependence)、`map/vegetation.h`(`TreeFrame`)、`common/utility.h`(`GetRandom`)。
- 被谁依赖：`map.h`(`Map::lamps`)、`map_save.cpp`、`Source/Forever/Framework/ForeverLampFrameworkComponent`。

## 待办/后续阶段

- 园区/建筑宿主的`LayoutLamps`。
- 现在是生成阶段一次性生成全部Actor(近/远只切换真实光源和Decal，见`ForeverLampFrameworkComponent.md`)，灯很多时再按距离流式生成/回收Actor。
