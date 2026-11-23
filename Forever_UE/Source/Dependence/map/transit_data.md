# transit_data.h

## 职责

`TransitData`：MapEditor"线路"工具手工设计的独立线路的**纯值类型**数据——实体线路（节点 + 连线）和线路环。`roadnet_file` 从 `.roadnet` 的 `output.transit` 读出，放进 `RoadnetMod::transit`，Core 原样拷贝一份（`Roadnet::GetTransit` / `Map::GetTransit`），route mod 在 `--skip` 时通过 `RouteMod::SetManualTransit` 拿到只读指针。

## 关键设计

- `Node{x,y,z,isExtern}`：地图单位，z=该节点高度（火车=轨面高度，飞机=飞行高度）；`isExtern`=城外连接点（编辑器吸附地图边界建的节点，在边界上；route mod 目前当普通节点用，掉头的半圆会凸出边界外）。
- `Conn{from,to,hasCtrl,cx,cy,kind,route}`：from→to 的二次 Bezier（`hasCtrl=false` 是直线），高度由两端节点 z 决定（扁 S）；`kind` ∈ ground/hug/mouth/tunnel/bridge；`route`=所属 route mod id（如 `route_train`）。无方向，route mod 自己造两个方向的铁轨/航线。
- `Item{preset,stop,node}`：线路环的一项。`preset>=0`=预设建筑（下标是 `.roadnet output.presets` 的原下标，`stop`=是否停靠），否则 `node>=0`=线路节点（只表示"经过"）。
- `Loop{route,items}`：首尾相接的闭环。
- `RoadStop{road,x,y,rightSide,station}` / `RoadLine{route,stops}`（2026-11-23）：编辑器"路网路线"设计的公交线（跑在路网上，不是实体线路）。`road`=`output.roads` 下标（= `Map::GetRoads()` 下标，Roadnet 按序拷贝），`(x,y)`=站点在路中线上的垂足（Core/mod 自己投影出弧长 t，不依赖编辑器 Bezier t），`rightSide`=沿 a→b 的右手边（side0），`station`=摆放这个站的 station mod id（目前只有 `station_bus`）。线是一个环（首尾隐含相接）。`roadLines` 的**全局下标**用于站命名，见 `Basic/traffic/bus_stop_name.h`。
- 全是值类型：route mod **只读**，不要往里写（跨 DLL 堆规则，见 `cross_dll_allocator_crash`）。

## 依赖关系

无（只用 `<string>`/`<vector>`）。被 `roadnet_mod.h`、`route_mod.h` 包含；用法见 `route_mod.md`"手动线路"。
