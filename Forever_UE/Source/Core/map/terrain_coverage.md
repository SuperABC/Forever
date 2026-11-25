# terrain_coverage.h / terrain_coverage.cpp

## 职责

把"每个 element 一种地形"的离散地图烘焙成**平滑的覆盖度图**，给UE侧地形材质做地形间混合。纯 C++17，只依赖标准库，不碰`Map`和UE类型。


## 关键设计

- **输入**：每个 element 的地形种类下标`ids[y*W+x]`(0..typeCount-1，越界抛`std::invalid_argument`)；**输出**`TerrainCoverage`：每种地形一张覆盖度，4种地形打包成一个RGBA8切片(`sliceCount=ceil(typeCount/4)`)，每个切片带完整mip链(到1x1)。
- **做法**：每个 element 展开成`texelsPerElement²`个texel的one-hot掩码(默认2 texel/element)，再做可分离高斯模糊(`sigma`以element为单位，默认1，核半径`3σ`，出界取边缘格)。
  模糊把"一格一格"的阶梯边界磨成圆滑曲线，**包括45°斜线边界**(量化前各地形覆盖度之和恒为1，量化后误差≤3/255)。材质再用噪声扭曲采样位置并"锐化"，边界才不会显得太糊或太规整。
- **字节顺序**：输出是RGBA顺序(通道c=地形种类 slice*4+c)；UE纹理是`PF_B8G8R8A8`，`TerrainMaterialBuilder`上传前必须把每个texel的R/B字节交换，否则着色器的`.x`和`.z`互换、0号和2号地形串台(2026-11-24踩过：平原与山地材质互换)。
- **不处理construction**：建筑占的格子渲染上和其它格一样，地形种类就是`Element::terrain`。
- **`DownsampleRgba8`**：2×2盒式降采样，`srgb=true`时在线性空间平均(颜色贴图)，false直接平均(覆盖图/法线)；`FullMipCount`算mip层数。覆盖图的mip用它生成(层贴图数组的mip由引擎cook时生成，不用它)。
- 1024×1024地图2 texel/element、8种地形(2个切片)：2048²×4B×2含mip≈43MB；烘焙耗时见`suite_terrain`的perf用例。

## 依赖关系

- 依赖：只用标准库。
- 被谁依赖：`Source/Forever/Framework/TerrainMaterialBuilder`；`Source/Test/Core/suite_terrain.cpp`。

## 测试

`CoreTest --suite terrain`：尺寸/切片数、覆盖度和为255(误差≤3)、超出typeCount的通道为0、单一地形全图255、`sigma=0`退化成硬边、镜像对称、直边单调下降、
**45°阶梯边界模糊后沿对角线的覆盖度几乎恒定(波纹≤8，不模糊时>100)且50%等值线仍在真实对角线上**、mip链到1x1且末级=全图均值、`buildMips=false`、降采样(线性/sRGB/奇数尺寸)、非法输入、大图耗时。

## 待办/后续阶段

- 无。
