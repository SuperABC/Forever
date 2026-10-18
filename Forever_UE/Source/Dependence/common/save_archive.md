# save_archive.h / save_archive.cpp

## 职责

存档"压缩体"内部的分段容器 `SaveArchive`，以及二进制段的读写小工具 `ByteWriter` / `ByteReader`。纯 C++，
不依赖压缩库；只在宿主侧（Core/UE）使用，mod 不直接接触（mod 只通过 `mod_state.h` 的接口读写自己的数据）。
整体方案见仓库根目录 `SAVE_PLAN.md`。

## 文件结构

- **外层头**（UE 的 `UForeverSaveSubsystem` 写）：`magic "FEVR"`(4) + `SAVE_FILE_VERSION`(u32) + 未压缩长度(u64)
  + zlib 压缩体。压缩用 UE 的 `FCompression`，所以外层在 UE 层。
- **压缩体** = `SaveArchive::Serialize()`：段数(u32) + 每段 `名字(u32长度+字节)` + `段版本(i32)`
  + `类型(u8，0=JSON 1=二进制)` + `长度(u64)` + 数据。全部小端序。

## 关键设计

- **版本号分层**（见 `SAVE_PLAN.md` 1.5 节）：这里定义外层的 `SAVE_FILE_VERSION` / `SAVE_FILE_MIN_VERSION`，
  管外壳布局；每个段自带段版本，段版本表在 `Source/Core/common/save.h`。改哪部分格式就只升哪部分的号。
- **小结构用 JSON 段，大数组用二进制段**：manifest、各类记录、mod 快照写 JSON 文本；地图格子、导航图、关系这类
  大数组用 `ByteWriter` 写紧凑二进制，避免 JSON 文本膨胀。
- **解析失败不崩溃**：`Parse` 对截断/错位/未知段类型都返回 false 并给出原因；`ByteReader` 读越界时置 `Failed()`、
  后续读返回 0，调用方读完一个段检查一次 `Failed()` 即可。
- 同名段 `SetSection` 会替换，不会重复。

## 依赖关系

- 依赖：只有标准库。
- 被谁依赖：`Source/Core/common/save.*`、`Source/Forever/Mod/ForeverSaveSubsystem.*`。
