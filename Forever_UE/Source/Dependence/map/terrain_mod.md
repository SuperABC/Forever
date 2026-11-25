# terrain_mod.h / terrain_mod.cpp

## 职责

`TerrainMod`定义地形Mod必须实现的完整业务接口。

## 关键设计

- **`GetPriority()`决定生成顺序**——`Map::InitTerrains()`按这个值降序排列所有已注册地形，
  依次调用`DistributeTerrain`。数值越高越先跑，后跑的地形理论上可以覆盖先跑的（但具体覆盖
  策略由每个Mod自己在`DistributeTerrain`内部决定，接口本身不强制）。已知取值：Ocean=1.0f，
  Mountain=0.9f。
- **`SetupTexture()`和`DistributeTerrain()`分两步**——`SetupTexture`只声明外观(地图界面颜色`SetMapColor`、地表材质`SetMaterial`)，`DistributeTerrain`才是真正改地图的算法。
  这样`Map`可以先统一调用一轮`SetupTexture`收集所有地形的外观(`Map::GetTerrainAppearances()`)，再按优先级顺序跑`DistributeTerrain`，两件事互不依赖执行顺序。
  `SetupTexture`是纯虚函数，但**材质不是必填**：没调`SetMaterial`的地形(`materialSpec.defined`为false)由渲染层用默认地表材质兜底(UE侧`TerrainMaterialBuilder`的`FallbackTerrainMaterial`，和默认地形`plain`同一套贴图)。
- **地表材质(`SetMaterial`，2026-11-24)**：材质声明活在各地形mod的C++里(不再有`diffusePath/normalPath/roughnessPath`，也没有JSON配置)，新增地形mod=在自己的`SetupTexture`里写一行。
  一个地形最多2个"层"`TerrainMaterialLayer{name, diffuse, normal, orm|roughness, tileMeters, triplanar}`(路径是UE资产路径，如`/Game/Asset/Terrain/Plain/PlainDiffuse.PlainDiffuse`。
  基类不提供拼路径的辅助函数：资产包的目录结构是具体mod的事，路径由各mod自己写全)。两种形式：
  - `SetMaterial(only)`：单层；
  - `SetMaterial(a, b, selector, range0, range1)`：两层，`TerrainMaterialSelector`决定怎么混——`Slope`按坡度`1-法线z`在range0..range1间从a(平缓)渐变到b(陡峭)；`WaterDepth`按水深(米，正=水下，由UE侧顶点色给出)从a(干)渐变到b(水下)；`None`只用a。
  层按`name`去重：不同地形可以共用同名层(如沙滩和海洋共用`wet_sand`)，贴图只加载一份。`normal`必填，`orm`(Megascans约定R=AO,G=粗糙度,B=金属度)和`roughness`(单独灰度图)二选一。
  整个游戏最多8种地形(`MaxTerrainTypes`)、层贴图须是正方形2的幂且≥1024，约束在UE侧`TerrainMaterialBuilder`检查并报错。
  **层贴图必须先过一遍`Tools/TerrainMaterial/prepare_terrain_textures.py`**(设成1024、不流送、diffuse BC7/normal BC5/orm或roughness BC7；运行时直接拷cook后的压缩块进贴图数组，格式不符该层报Error并渲染成灰)。
  - 打包：各工程`pak.bat`在cook前自动跑，主工程处理`/Game/3rdParty/Megascans/Surfaces`和`/Game/Asset/Terrain/Plain`，mod工程处理`/<插件名>/TerrainLayers`——**mod的层贴图要放进插件的`Content/TerrainLayers/`，名字后缀`_B/_N/_ORM`(或`Diffuse/Normal/Roughness`)**，放别处不会被处理。
  - 编辑器/PIE：不经过`pak.bat`，但`TerrainMaterialBuilder`加载层贴图时发现属性不对会**在内存里自动改成同样的属性**(不存盘、不标脏)并打`LogTerrainMaterial` Warning(带改前的属性)，首次重新压缩要几秒，之后进DDC。源图不是方形2的幂或<1024改不了，报Error。想让资产文件本身也改好(或打包前确认)，手动跑一次脚本，命令见`Tools/TerrainMaterial/README.md`。
- **地图界面颜色**：`SetupTexture()`里可调`SetMapColor(r,g,b)`(0~255)指定这个地形在地图界面(`MapWidget`)里画什么颜色，数据经`TerrainAppearance::mapColor`带出；不调用则界面按地形名哈希一个颜色。无mod的默认地形`plain`由界面固定画草地绿。
- **`DistributeTerrain`用6个`std::function`回调读写地图，不直接依赖`Map`/`Element`类型**——
  这是Dependence层一贯的做法（Mod DLL不知道也不需要知道`Map`的具体实现），调用方（`Map`）
  把自己的`GetTerrain`/`SetTerrain`/`GetHeight`/`SetHeight`/`SetWater`/`SetFineHeights`包成闭包传进去。
- **水面(`setWater(x,y,hasWater,height)`)独立于地形类型(`setTerrain`)，每个格子单独设置**——不同格子可以给不同水位(高度单位是地图元素)，用来构造倾斜水面(河流沿下坡方向)。哪个格子要水、水位多少完全由mod在算法里决定，`OceanTerrain`/`RiverTerrain`的用法见`terrain_*.md`。
- **细化高度(`setFineHeights`)按需逐格设置，默认没有**——粗高度`height`是格点`(x,y)`上的
  一个采样，10m一格，UE侧双线性插值在格子边界只有C0连续，起伏大的地方(河岸)会出现明显
  折痕。mod可以给个别element传一个`TERRAIN_FINE_SIZE`×`TERRAIN_FINE_SIZE`(17×17)的高度
  数组，覆盖`[x,x+1]×[y,y+1]`，含右/上边界那一列/行——选17×17不选16×16，是因为这样每块
  自成一体，求值不用读邻居(邻居很可能没有细化数组)，而且UE最细一级LOD每格正好16段，各级LOD
  顶点都落在这个网格的下标上，直接点采样即可。约定：四个角等于对应格点粗高度；相邻两块
  细化数组的共享边应该一致(不一致也不会裂缝，每个点只由floor落到的那一格求值)；必须先
  `setHeight`再`setFineHeights`，因为`Map::SetHeight`会清掉共用该格点的4块细化数组，防止
  后跑的mod改了粗高度之后细化数据过期。数据只传`const float*`，Core自己分配内存拷贝，不会
  跨DLL释放mod分配的内存。目前只有`RiverTerrain`的河岸用到，见`terrain_*.md`。
- **`materialSpec`/`hasMapColor`等是公开数据成员，不是私有+getter**——`Terrain`包装类直接读`mod->materialSpec`/`mod->mapColor*`返回，不通过虚函数转发。
  它们是非虚的数据+非虚辅助函数，所以不改`TerrainMod`虚表/签名，旧mod DLL不用跟着改接口(但`Dependence`头变了，外部mod要重编，见记忆里的"外部mod构建"提醒)。

## 依赖关系

- 依赖：`common/mod_state.h`（纯C++，不依赖Core/UE）。
- 被谁依赖：`Source/Core/map/terrain.h`（`Terrain`包装类持有`TerrainMod*`）、
  `Source/Basic/map/terrain_*.h`（`OceanTerrain`/`MountainTerrain`）、
  `Forever_Mod/Empty`的`EmptyTerrain`demo mod。

- 带存档钩子(`SaveState`/`LoadState`/`GetStateVersion`/`GetInstanceCounter`/`SetInstanceCounter`)，默认不存任何东西，见`common/mod_state.md`。

## 待办/后续阶段

- 无。
