# roadnet_small

## 职责
小地图路网：把 `small.roadnet` 编码进代码的 roadnet mod，效果与 `"roadnet_file --path=<small.roadnet>"` 相同，但不读文件、没有 `--path` 参数。

## 关键设计
- 由 `Forever_UE/Tools/MapEmbed/embed_map.py` 生成，`roadnet_small.h/.cpp` 是 `Source/Basic/map/roadnet_file.*` 的拷贝(只换类名/id/显示名、读文件段)，数据在 `roadnet_small_data.inc`(79607 字节)。**勿手改**：`small.roadnet` 或 `roadnet_file` 改了就重新生成。
- 生成命令：`python Forever_UE/Tools/MapEmbed/embed_map.py roadnet <small.roadnet路径> --id roadnet_small --class SmallRoadnet --name 小地图路网 --out <本目录>`
- 设计细节(格式、优先级、贴图/tag 等)同 `Source/Basic/map/roadnet_file.md`。

## 依赖关系
只依赖 Dependence。
