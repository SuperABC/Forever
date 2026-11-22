# Water/WaterWave（统一波浪表达式）

水面高度 `H(x, y, t, iterations)`（厘米，零均值）只有一份源码：`WaterWave.inc`（Shadertoy MdXyzX / afl_ext 的 `getwaves`，MIT，移植版）。

- C++：`WaterWave.h` 的 `ForeverWater::WaveHeight(x, y, t, iterations=12)` `#include` 该文件作为函数体。游泳/浮力（`ForeverCharacter::QueryWater`）用它。
- Shader：`Tools/WaterMaterial/build_water_assets.py` 把 `.inc` 文本内联进 `M_WaterSurface`（WPO/法线）和 `PP_Underwater`（水面高度）的 Custom 节点。
- 两边各自实时算同一表达式，不传算好的数据。

## 规则
- **改波型只改 `WaterWave.inc`，然后重跑 `build_water_assets.py`**（编辑器关闭，命令见 `Tools/WaterMaterial/README.md`），再重编 C++。
- `.inc` 只能用 C++/HLSL 公共子集：标量 float、`sin/cos/exp/sqrt/ceil`、`for/if/break`，注释不要非 ASCII。
- 方向步进用 `1232.399963 mod 2π = 0.8956…`，避免 GPU 大参数 `sin/cos` 误差导致与 C++ 对不上。
- `iterations` 为 float：最后一个 octave 按小数部分渐入，octave 数随距离连续变化（材质里近 12 → 远 1），相邻 LOD 同一位置高度一致。
- 参数（`WAVE_SCALE`/`AMPLITUDE`/`DRAG_MULT`/`timeMultiplier`）都在 `.inc` 顶部；未经 PIE 调参。

## 当前状态
`WaterWave.inc`末尾已改成`return 0.0f`（波浪关闭，水面平的）；原表达式留在注释里。水面mesh改回单张图一个网格+引擎插件材质，波浪没有消费者，只有`QueryWater`和`PP_Underwater`还在调用（结果为0）。
