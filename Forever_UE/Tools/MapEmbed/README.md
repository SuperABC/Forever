# MapEmbed

把 `Tools/MapEditor` 导出的 `.terrain` / `.roadnet` 编码进一个 TerrainMod / RoadnetMod 的 C++ 源码。生成的 mod 不读文件、没有 `--path` 参数，在游戏里造出的地形/路网和 `terrain_file --path=<文件>` / `roadnet_file --path=<文件>` 完全相同。用途：把某张手工地图做成一个固定的 mod 随 DLL 发布(比如新手教学用 `small.terrain`/`small.roadnet`)。

## 用法
```
python Forever_UE/Tools/MapEmbed/embed_map.py terrain <in.terrain> --id terrain_small --class SmallTerrain --name 小地图地形 --out <目录>
python Forever_UE/Tools/MapEmbed/embed_map.py roadnet <in.roadnet> --id roadnet_small --class SmallRoadnet --name 小地图路网 --out <目录>
```
在 `--out` 目录里生成 `<id>.h`、`<id>.cpp`、`<id>.md`、`<id>_data.inc`，并打印要在所在 DLL 里加的注册代码(`GetModTerrains`/`RegisterModTerrains` 或 Roadnet 那一对，写法同 `Source/Basic/basic.cpp`)。`.cpp` 要加进该 DLL 的 `.vcxproj`，`config.json` 里把 `terrain_file --path=…` 换成 `<id>`(路网同理)。只依赖 Python 3 标准库。

## 在用的生成物
- `Forever_Mod/Tutorial/Cpp/Tutorial/terrain_small.*`、`roadnet_small.*`：新手教学的地形/路网，源文件 `Forever_UE/Resource/Mod/Basic/small.terrain`/`small.roadnet`。重新生成：
  `python Forever_UE/Tools/MapEmbed/embed_map.py terrain Forever_UE/Resource/Mod/Basic/small.terrain --id terrain_small --class SmallTerrain --name 小地图地形 --out Forever_Mod/Tutorial/Cpp/Tutorial`(roadnet 同理：`roadnet …small.roadnet --id roadnet_small --class SmallRoadnet --name 小地图路网`)，再编 `Forever_Mod/Tutorial/Cpp/Tutorial.sln`。

## 原理
- **不另写解析代码**：生成的 `.h/.cpp` 是 `Source/Basic/map/terrain_file.*` / `roadnet_file.*` 的拷贝，只做这几处替换：类名、`GetId`/`GetType` 返回的 id、`GetName` 的显示名、构造函数里的 `--path` 解析(换成固定的说明串，只用于日志)，以及模板里 `// [map_embed:load begin]` 到 `// [map_embed:load end]` 之间"按路径读文件"的一段(换成从内嵌数组 `kEmbeddedData` 取数据)。找不到任何一处替换点工具就报错退出，说明 Basic 里的写法变了，要同步改 `embed_map.py`。
- **数据**：`.terrain` 原样内嵌(魔数 `FTRN` 校验)；`.roadnet` 只保留 mod 会读的 `output` 段并压紧成一行 JSON(`source` 段是编辑器自己的编辑模型，mod 不读)，Python 读不了的宽松 JSON 就原样内嵌。数组写成十进制字节初始化列表(MSVC 字符串字面量有 64KB 上限，不能用字符串)。`small.terrain`(655KB)生成约 2MB 的 `.inc`，Release 编译约 10 秒。
- **生成的代码勿手改**：`.terrain`/`.roadnet` 改了，或 `terrain_file`/`roadnet_file` 改了(修 bug、加字段)，都要重新跑一遍工具，否则内嵌 mod 停在旧逻辑上。

## 验证(2026-11-26)
用 `small.terrain`/`small.roadnet` 生成后编进一个临时 DLL：ModTest 45 项全过；CoreTest 用同一份 config 只把 `terrain_file`/`roadnet_file` 换成内嵌 mod，`--child world --seed 7 --size 256` 两边生成世界，指纹里各域计数、行人/车行导航图、地形段、建筑/人口/交通/脚本等存档段全部一致；不同的只有随 mod id 变的东西(按 id 派生的 mod 随机种子、manifest、路网段里存的 mod id/类型/显示名，长度差正好 5 字节)。
