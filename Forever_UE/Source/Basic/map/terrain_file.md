# terrain_file

## 职责
从第三方工具 `Tools/MapEditor` 导出的 `.terrain` 文件里读出整张地图的地形字符串、高度、水位，写进地图。配置示例：`"terrain_file --path=C:/maps/city.terrain"`（放进 `terrain_mods`；`--path value` 写法也行）。

## 关键设计
- **文件格式**（二进制，小端）：魔数 `FTRN`；`u32 width, height`；**调色板**：`u16` 条数，每条 `u16` 长度 + UTF8 字符串（下标0固定 `plain`）；然后 `u16[w*h]` 每个 element 的调色板下标（行优先 `y*w+x`）；`f32[w*h]` 高度；`f32[w*h]` 水位（NaN=无水）。单位同 Core：1 element=10m，高度同单位。不含 fine heights。
- **路径**：TerrainMod 没有 PostHandle，所以只能是绝对路径或相对进程工作目录的路径；路径里可以有空格（取到下一个 ` --` 或结尾）。`--seed` 已被 Core 在构造前剥掉。
- **plain/construction 的高度照文件读**：编辑器笔刷默认不改平原格高度，但勾选"允许改平地"后可以抬高/压低平原（比如让山体到平原的过渡更平缓）。Core 对 3×3 范围内**高度都为 0**（不看地形类型）的区域标记为可建设(construction)，所以被抬高过的平原不会长建筑/地块（编辑器的地块检查也会提示）。**水不受限制**：岸边的平原格也要放水，否则水面和地面之间会露出缝隙。Core 的可建设标记只看高度、不看水，所以 3×3 高度全为 0 的区域里放了水也会被标成可建设（编辑器不对此警告：地面以下有水、岸边地面以上有水都合理）。
- **尺寸不一致**：以游戏地图尺寸为准，裁剪或留 plain，并 `debugf` 警告。文件缺失/损坏：`debugf` 后不动地图。
- **优先级1.0**：最先跑。若同时启用山/海/河，它们只会作用在文件没占用的 plain 格上；用文件地形时建议 `terrain_mods` 里只留 `terrain_file`。
- **贴图**：自己没有声明任何材质(`SetupTexture` 只设地图色；文件里的格子地形串是 `plain`/`terrain_mountain` 等，不会是 `terrain_file`)。`plain` 的材质在 `Map::GetTerrainAppearances()` 里硬编码(沙地)；文件里其它字符串(含 `terrain_mountain`/`terrain_beach` 等)只有对应 mod 也启用时才有自己的材质，否则那些格子渲染成纯灰并在日志里打 `LogTerrainMaterial` 错误。**要让文件里的山/海/河有材质，就把这几个 mod 也启用并加 `--skip`**（只提供贴图、不再随机生成），例如 `terrain_mods`: `["terrain_file --path=…", "terrain_mountain --skip", "terrain_ocean --skip", "terrain_river --skip"]`（河 mod 依赖山 mod，三个一起开）。
- 不写 fine heights，所以没有河岸精细高度。

## 依赖关系
只依赖 Dependence（`map/terrain_mod.h`、`common/utility.h`）。编辑器侧读写见 `Tools/MapEditor/js/io.js`。

## 待办/后续阶段
- 河岸精细高度（17×17）。
