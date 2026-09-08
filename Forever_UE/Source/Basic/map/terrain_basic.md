# terrain_basic.h / terrain_basic.cpp

## 职责

`OceanTerrain`/`MountainTerrain`是Terrain域两个真实的默认地形内容，从`Source/Basic/README.md`
共用文档独立出来（该文档自己约定的"业务接口补齐后独立成文"规则）。对照老工程
`E:\Projects\Forever_UE\Source\Basic\map\terrain_basic.h/.cpp`原样迁移，包括全部魔法数字。
老工程这里没有一个叫`TerrainBasic`的类——阶段3那个占位类名本来就是要被这两个真实类替换掉的。

## OceanTerrain

`GetPriority()=1.0f`（最高，最先生成），`diffusePath="/Game/Asset/Textures/Terrain/
PlainDiffuse.PlainDiffuse"`（和plain地面用同一张贴图——"海洋"的视觉效果完全来自独立的水面
mesh/材质，不是靠海底贴图区分），`waterHeight={true,-0.1f}`（水面固定在-0.1地图单位）。

`DistributeTerrain`两步走：

1. **海岸线**：随机决定1-4条地图边（`coastMask`，至少保证一面），每条边独立走一次带随机游走
   的深度轮廓（`coastDepth`基准+`coastSlope`每行小幅摆动，越界回弹），标记该范围内的格子为
   `"ocean"`类型。
2. **深度**：以陆地相邻的海洋格为海岸线种子（高度0），做BFS向内陆扩散，下降方向沿"离开地图
   中心"的方向；贴岸第0/1圈强制拉平为0，从第2圈起才真正变深，深度上限`OCEAN_MAX_DEPTH`。

常量：`OCEAN_DEPTH_RATE=0.05f`、`OCEAN_SLOPE_JITTER=0.01f`、`OCEAN_MAX_DEPTH=60.0f`。

## MountainTerrain

`GetPriority()=0.9f`（次高，海洋之后），`diffusePath="/Game/Asset/Textures/Terrain/
RockDiffuse.RockDiffuse"`（唯一和plain视觉不同的地形——这是mountain区别于plain/ocean的
真正体现）。

`DistributeTerrain`按密度公式`densityScale=width*height/(512*512)`（整数除法）决定山脊数量
（`densityScale>1`时`4+GetRandom(densityScale*2)`条，否则0条——**注意是严格大于1，不是
大于等于1**：512x512地图算出的`densityScale`刚好是1，一座山都不会生成，这是`AForeverFrameworkActor`
默认地图尺寸最初选512x512时踩到的实际bug，PIE验证时发现"有海没山"才定位到。改成1024x1024后
`densityScale=4`，稳定能生成4-11座山；顺带地图尺寸必须保持2的整数次幂，理由见
`Source/Forever/Framework/ForeverFrameworkActor.cpp`里`kDefaultMapWidth`旁的注释
（LOD覆盖范围计算也依赖这一点，不是只有山的数量在意）。每条山脊：

1. 从随机中心点（要求落在`"plain"`且离地图中心足够远）向两端随机游走生成弯曲多段线。
2. 沿脊线从随机峰值格（20-49高度）向两端衰减。
3. 侧向BFS扩散（带9宫格连续性修正抑制突变）+ 5x5均值平滑。
4. 只写回`"plain"`/`"mountain"`格子（不覆盖ocean/construction），高度取现有值和新值的较大者
   （允许多座山脊叠加，不互相压低）。

常量：`ALONG_BASE_RATE=0.30f`、`ALONG_RATE_JITTER=0.10f`、`ALONG_RATE_MIN=-0.15f`、
`ALONG_RATE_MAX=1.00f`、`LATERAL_BASE_RATE=0.35f`、`LATERAL_INIT_JITTER=0.15f`、
`LATERAL_PROP_JITTER=0.06f`、`RIDGE_STEP_LEN=1.0f`、`RIDGE_TURN_JITTER=0.15f`、
`RIDGE_SUB_STEPS=4`，和`OceanTerrain`共用同一张8方向邻居偏移表(`neighborOffsets`)。

## 关键设计

- **两个类都依赖阶段4-0已经迁移好的`common/utility.h`（`GetRandom`/`Counter`/`debugf`）**，
  不需要额外迁移任何支撑代码。
- **`MountainTerrain`的重试预算是全局共享的**——`Counter counter(200)`在整个`mountainCount`
  循环外面只创建一次，不是每座山各自200次；某次候选点不合格时`attempt--`重试，直到200次
  预算耗尽为止，这是对照老工程精确保留的行为，不是简化。
- **`"plain"`不是这里注册的Mod**——它是`Element::terrain`的硬编码默认值（见`map.md`），两个
  地形类只负责"从plain上面覆盖出ocean/mountain"，不负责生成plain本身。

## 依赖关系

- 依赖：`map/terrain_mod.h`、`common/utility.h`。
- 被谁依赖：`Source/Basic/basic.cpp`（`GetModTerrains`/`RegisterModTerrains`导出函数）。
