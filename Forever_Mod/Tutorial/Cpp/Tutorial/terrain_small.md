# terrain_small

## 职责
小地图地形：把 `small.terrain` 编码进代码的 terrain mod，效果与 `"terrain_file --path=<small.terrain>"` 相同，但不读文件、没有 `--path` 参数。

## 关键设计
- 由 `Forever_UE/Tools/MapEmbed/embed_map.py` 生成，`terrain_small.h/.cpp` 是 `Source/Basic/map/terrain_file.*` 的拷贝(只换类名/id/显示名、读文件段)，数据在 `terrain_small_data.inc`(655444 字节)。**勿手改**：`small.terrain` 或 `terrain_file` 改了就重新生成。
- 生成命令：`python Forever_UE/Tools/MapEmbed/embed_map.py terrain <small.terrain路径> --id terrain_small --class SmallTerrain --name 小地图地形 --out <本目录>`
- 设计细节(格式、优先级、贴图/tag 等)同 `Source/Basic/map/terrain_file.md`。

## 依赖关系
只依赖 Dependence。
