# bus_stop_name.h / bus_stop_name.cpp

## 职责

`BusStopName(lineIndex, stopIndex)`：给编辑器"路网路线"里手动摆的公交站生成站名(`BusStop_L<线下标>_S<站下标>`)。
`.cpp`只是为了验证头文件自包含，没有内容。

## 关键设计

- 线下标/站下标是`TransitData::roadLines`里的全局下标。
- `station_bus`(摆站)和`route_bus`(连线)必须用同一个名字——Core靠站名把线路和站点对上，所以放在这个两边都包含的共用头里，
  不各写一份。

## 依赖关系

- 依赖：只用标准库。
- 被谁依赖：`station_bus.cpp`、`route_bus.cpp`。
