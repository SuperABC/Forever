# TerrainBlendTest

脱离 UE 的独立验证程序，验证 `NearTerrainBlend` 近处地形混合算法（原 `terrain_blend.*`，现在合并在
`Source/Core/map/terrain.h/.cpp` 里）。算法本身只依赖标准库，自带最小 PNG 写出器，不需要任何图像库；
但 `terrain.cpp` 里同时有 `Terrain` 类，所以编译时要带上 Dependence 的头文件和它用到的三个源文件
（`terrain_factory.cpp`、`terrain_mod.cpp`、`error.cpp`）。

## 编译运行

```
g++ -std=c++17 -O2 -Wall -Wextra -I ../../Source/Core -I ../../Source/Dependence -I ../../Source/Dependence/common main.cpp ../../Source/Core/map/terrain.cpp ../../Source/Dependence/map/terrain_factory.cpp ../../Source/Dependence/map/terrain_mod.cpp ../../Source/Dependence/common/error.cpp -o terrain_blend_test
./terrain_blend_test <输出目录> [--radius 1.5] [--tpe 16] [--seed 1] [--chunks 64] [--bound 20]
```

- MSVC（x64 Native Tools 命令行）：`main.cpp` 用了 `__builtin_popcount`，需要加
  `/FIintrin.h /D__builtin_popcount=__popcnt`，并且用 `/MD` 和 Dependence 保持一致：
  `cl /std:c++17 /O2 /MD /EHsc /utf-8 /FIintrin.h /D__builtin_popcount=__popcnt /I ../../Source/Core /I ../../Source/Dependence /I ../../Source/Dependence/common main.cpp ../../Source/Core/map/terrain.cpp ../../Source/Dependence/map/terrain_factory.cpp ../../Source/Dependence/map/terrain_mod.cpp ../../Source/Dependence/common/error.cpp`
- 不给 `--radius` 时依次跑 `1.5` 和 `2.0` 两组。
- `--chunks` 必须是 8 的倍数；`--chunks 8` 只跑 128×128 个 element，几秒就能出结果，适合改代码时快速试。
- 默认的完整实验（64×64 个 chunk，每个 16×16 个 element）每组半径几分钟。
- 有任何断言失败时返回非零退出码；全部输出同时写入 `<输出目录>/results.txt`。

## 输出的图

图像上方是地图 +Y。颜色：plain 绿、ocean 蓝、mountain 灰，压力地图里另有黄、红、紫、青。

| 文件 | 内容 |
|---|---|
| `<map>_before.png` | 混合前，一个 element 一个像素 |
| `<map>_construction_mask.png` | construction 的位置（白色） |
| `<map>_r<R>_after.png` | 混合后，一个 element 一个像素，取该 element 内所有 texel 混合颜色的平均 |
| `<map>_r<R>_diff.png` | `after` 与 `before` 的差异热力图，看过渡带宽度 |
| `<map>_crop_before.png` | 交界最密处 4×4 个 chunk（64×64 个 element）的混合前，放大到 1024×1024 |
| `<map>_r<R>_crop_after.png` | 同一位置的混合后，texel 分辨率 |
| `<map>_r<R>_crop_after_grid.png` | 同上，叠加 chunk 边界线（每 256 像素一条），用来确认没有接缝 |
| `<map>_r<R>_crop_weights.png` | 按地形 id 的权重：R = mountain，G = plain，B = ocean |
| `<map>_r<R>_crop_slots.png` | 按槽位的权重：R、G、B = 槽位 0、1、2 |

`<map>` 是 `main`（噪声生成的山、海、平原，按游戏规则标出 construction）或 `stress`（Voronoi 大块加
乱码块、棋盘格、单格孤岛、细线，专门制造同一点多于四种地形的情况）。

## 检查项

1. **解析用例**：同质区域、一条直边（单调、对称、边界上 50/50、与 double 实现对照、测量过渡带宽度）、
   单个孤立 element、9 种地形挤在 3×3 里的路口（触发前四大截断）。
2. **连续性的细化测试**：texel 越密，相邻 texel 的权重差应该按比例缩小；有跳变就不会缩小。
3. **整图**：每个 texel 权重和为 255 且无重复 id；逐 chunk 拼接与 128×128 大块一次计算逐字节相同；
   快速路径开/关逐字节相同；与慢速参考实现、与独立的 double 实现一致；construction 内部纯净；
   主地图上相邻 texel 权重差不超过 `--bound`。
4. 统计与耗时：各路径 texel 占比、前四大截断占比、单个 chunk 重建耗时。
