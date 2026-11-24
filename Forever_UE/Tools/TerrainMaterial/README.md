# TerrainMaterial

生成运行时唯一的地形材质 `/Game/Asset/Terrain/M_Terrain`（所有 LOD、所有地形共用）。设计和运行时数据见 `Source/Forever/Framework/ForeverTerrainFrameworkComponent.md`"地形材质"一节。

## 文件

- `prepare_terrain_textures.py`：把层贴图设成运行时能直接拼进数组的格式(最大1024、不流送、漫反射BC7/法线BC5/遮罩BC7)，并校验源图是方形2的幂且≥1024。已经符合的不会重新保存。
  按资产名后缀识别角色：`_B`/`_D`/`Diffuse` 漫反射，`_N`/`Normal` 法线，`_ORM`/`Roughness` 遮罩。用法见脚本头部，**各工程的 `pak.bat` 在 cook 前自动跑它**：
  `Forever_UE/pak.bat` 处理 `/Game/3rdParty/Megascans/Surfaces` 和 `/Game/Asset/Terrain/Plain`；`Forever_Mod/*/UE/*/pak.bat` 处理 `/<插件名>/TerrainLayers`(目录不存在就跳过)。
  编辑器/PIE 不经过 pak.bat：`TerrainMaterialBuilder` 发现层贴图属性不对会在内存里按同样规则自动改(不存盘，打 `LogTerrainMaterial` Warning)——**规则在 `PrepareTextureInEditor`(TerrainMaterialBuilder.cpp)和本脚本各有一份，改一边要同步另一边**。
  新增地形层贴图：放进这些目录(或在 pak.bat 里加目录)，贴图名按后缀约定。注意 `-script="..."` 里的路径要用正斜杠(`\U` 会被当转义)。

- `terrain_material.hlsl`：材质的全部逻辑，一个 Custom 节点（覆盖图采样+噪声扭曲+锐化、坡度/水深选层、三向投影、距离分档、世界空间法线输出）。
- `build_terrain_material.py`：UE Python 脚本，建/重建 `M_Terrain`：把 hlsl 原样贴进 Custom 节点，建好参数节点(纹理数组/表/噪声/调参向量)和输出连线，并创建 `Content/Asset/Terrain/Placeholder/` 里的占位资产(只用来让材质编译时知道参数类型和 sRGB 与否，运行时被 MID 整个替换)。可重复运行，已存在的资产会清空节点后重建。

## 运行

先关掉编辑器(否则资产被锁)，再：

```powershell
& "C:\Workspace\UE5\Engine\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Workspace\UE5\Projects\Forever\Forever_UE\Forever.uproject" -run=pythonscript -script="C:\Workspace\UE5\Projects\Forever\Forever_UE\Tools\TerrainMaterial\build_terrain_material.py" -unattended -AllowCommandletRendering
```

- `-AllowCommandletRendering` 必须带，否则 shader 不会真编译，`get_statistics` 恒为 0。
- 命令行里 `print` 不显示，脚本用 `unreal.log_warning` 输出(搜日志里的 `[build_terrain_material]`)；有 `PROBLEM:` 行表示某一步失败。
- uproject 和脚本路径要用绝对路径。

改了 `terrain_material.hlsl` 或要加新的参数/输出时重跑即可；**只改地形 mod 里的 `SetMaterial`(贴图、tile、范围)不需要重跑**，那些是运行时数据。

## 材质参数

纹理/表/覆盖图由 `TerrainMaterialBuilder` 运行时通过 MID 的同名参数传入：`TerrainDiffuseArray`、`TerrainNormalArray`、`TerrainMaskArray`、`TerrainCoverageArray`、`TerrainTypeTable`、`TerrainLayerTable`、`TerrainNoise`，标量/向量 `TerrainMapInfo`、`TerrainWaterEncoding`。

调参向量(材质里的默认值，可在 MID/材质实例上直接改)：

| 向量 | 默认 | 含义 |
|---|---|---|
| `TerrainTuning0` | (0.35, 0.12, 0.30, 0) | 采样位置噪声扭曲幅度(element) / 覆盖度噪声幅度 / 锐化带宽 |
| `TerrainTuning1` | (120, 200, 40, 140) | 开始/完全渐变到平均色的距离(米) / 细节法线开始/完全淡出的距离(米) |
| `TerrainTuning2` | (70, 16, 0, 0) | 三向投影终止距离(米) / 噪声周期(element) |

## 已知限制

- 打包版的层贴图必须先过 `prepare_terrain_textures.py`(pak.bat 自动跑)，否则运行时对应层报错、渲染成灰；编辑器里会自动在内存修正。
- 最多 8 种地形，层贴图须是正方形 2 的幂且 ≥1024。
- 画面效果(斜线边界、坡度/水线过渡、距离档切换、帧率)只能 PIE 目测，tile 米数和各范围是初值。
