# station_train.h / station_train.cpp

## 职责

`TrainStation`(`station_train`)：station域的默认实现之一。

## 关键设计

- 端口坐标/朝向的换算见`station_common.md`(`NearRoadPoint`/`AlongAxisHeadingDegrees`)。
- **端口模型**(`station_mod.md`)：每条轨道 t(t=0/1)有 `inlet t`、`outlet t`、`stop t`，下标=轨道号。`inlet`/`outlet` 在沿路比例 0.3/0.7(轨道 0)、0.7/0.3(轨道 1)，`stop` 在 0.5(进出口中点)；深度 0.55/0.65 两档，让两条轨道空间上分开。同一条轨道的三个端口朝向相同，三条连线退化成直线。`Route`/`TrainRoute::LayoutRoute` 按"轨道 t = 端口下标 t"取，不再依赖固定的4接口顺序。
- **站内连线**(每条轨道三条，`StraightConnection`)：inlet→stop、stop→outlet 带 `mesh=kTrainTrackMesh`、`meshUnit=kTrainTrackUnit`(0.25，在 `station_train.h`)；inlet→outlet(直通，不停站穿过时走它)mesh 为空，避免和前两条重叠铺两遍。
- **火车站：楼体占满整个Building区域**，轨道端口落在楼体范围内：楼体是地下1层`preset_station_train_bg+`(4条楼梯通上来)+地面1层`preset_station_train_fg-`(大厅+左右两间`room_train_station`)，轨道在地面层之下/之中穿过。端口世界坐标换算不受楼体 footprint 偏移影响(见`Core/traffic/station.md`)。
- **靠右行驶**：`FACE_NORTH`/`FACE_EAST` 时两条轨道的深度对调(`swapDepths`)，使任何朝向下沿航向走时另一条轨道都在左手边(局部坐标+X东+Y南，右手边=(-hy,hx))。`TrainRoute` 据此让顺时针环靠内侧、逆时针环靠外侧，各站一致。

## 依赖关系

- 依赖：`Dependence/traffic/station_mod.h`、`Dependence/map/general_geometry.h`
  (`FACE_DIRECTION`/`PointParams`/`Road`)。
- 被谁依赖：`Basic/map/building_*.cpp`的`TrainStationBuilding`/`AirportBuilding`
  (`stationMod`字段分别指向`"station_train"`/`"station_air"`，依赖表里是站点依赖楼：没有楼就不会生成站点)；
  `TrainRoute::LayoutRoute`/`AirRoute::LayoutRoute`依赖"轨道/跑道 t 的端口下标=t"；`Basic/basic.cpp`注册这三个类型进
  `config.json`的`"station_mods"`。

## 待办/后续阶段

- 火车站楼的新布局(地下层+地面层、轨道接口落在楼体内)还没进游戏看过效果，见`map/building_*.md`。
- `TrainRoute::LayoutRoute`已支持多站(见`route_train.md`)，无头验证过1/2/3/4/7/10站；没进游戏看过。
- 端口/连线几何由无头 harness 检查过(靠右 0 错、导航无悬空)，**未 PIE**。
