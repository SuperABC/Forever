# terrain_args.h

## 职责
山/海/河三个地形mod共用的参数解析：`ParseDistributeArg(args)` 取 `--distribute=false`（`--distribute false` 也行，`0/no/off` 同义，大小写不敏感），返回是否要跑 `DistributeTerrain`。没写参数或其它值一律 true。

## 关键设计
- **用途**：配合 `terrain_file`。地图由文件给定时，山/海/河不该再随机生成，但它们的贴图只在自己的 mod 启用时才登记——所以配置里写 `"terrain_mountain --distribute=false"` 让它只提供贴图（`SetupTexture`），`DistributeTerrain` 直接返回。
- 只解析 `--distribute`，`--seed` 已被 Core 在构造前剥掉；其它未知参数忽略。
- header-only inline，无状态。

## 依赖关系
无（只用标准库）。使用方：`terrain_mountain/ocean/river.{h,cpp}`。
