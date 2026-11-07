# terrain_basic.h / terrain_basic.cpp

## 职责

`OceanTerrain`/`MountainTerrain`/`RiverTerrain`是Terrain域三个默认地形实现。`"plain"`不是Mod，是`Element::terrain`的硬编码默认值(见`map.md`)，这三个类只负责在plain上覆盖出ocean/mountain/river。三者共用文件顶部的`SEA_LEVEL`(-0.1)和8方向邻居偏移表`neighborOffsets`。

执行顺序由`GetPriority()`决定：Mountain `0.95` -> Ocean `0.9` -> River `0.5`。

## OceanTerrain

贴图复用plain地面三张贴图——"海洋"的视觉完全来自独立的水面mesh/材质。

**为什么海排在山之后**：海岸线雕刻时**已经是`terrain_mountain`的格子跳过**(`isOcean`保持false，对后面的海岸线BFS种子判定等同普通陆地，不需要特判)，让海绕开山体。如果海先跑，山体边缘自己的侧向衰减曲线和紧贴山脚雕出的海岸地势是两套独立算的数字，接缝处会出现台阶状硬悬崖。注意这不是严丝合缝的高度拼接：海岸线BFS种子高度固定0，不读相邻山体边缘高度，剩下的只是山体衰减到近0和海岸0之间很小的自然差值。

`DistributeTerrain`两步：
1. **海岸线**：随机选1-4条地图边(`coastMask`，至少一面)，每条边带随机游走的深度轮廓(`coastDepth`基准+`coastSlope`每行小幅摆动，限制在`size/64`~`size/8`之间回弹)，范围内格子标记为`terrain_ocean`。
2. **深度**：以与陆地相邻的海洋格为海岸线种子(高度0)做BFS向内陆扩散，下降方向沿"离开地图中心"；第0/1圈强制0，从第2圈起才变深，上限`OCEAN_MAX_DEPTH`。写回高度时同循环调`setWater(x,y,true,SEA_LEVEL)`(per-cell水面机制见`terrain_mod.md`"水面")。

常量：`OCEAN_DEPTH_RATE=0.05`、`OCEAN_SLOPE_JITTER=0.01`、`OCEAN_MAX_DEPTH=60`。

## MountainTerrain

贴图是Rock三张，唯一和plain视觉不同的地形。

- 山脊数：`densityScale=width*height/(512*512)`(整数除法)，`>1`时`4+GetRandom(densityScale*2)`条，否则0。**地图必须严格大于512x512才会生成山**；地图尺寸同时必须保持2的整数次幂(LOD覆盖范围计算依赖，见`ForeverFrameworkActor.cpp`的`kDefaultMapWidth`注释)。
- 重试预算全局共享：`Counter counter(200)`在循环外只创建一次，候选点不合格(不在plain上或离地图中心太近，`min(w,h)/4`)时`attempt--`重试，直到200次耗尽。
- 每条山脊：①从随机中心点向两端随机游走成多段线；②沿脊线从随机峰值格(20-49)向两端衰减，递减速率是有界随机游走(允许偶尔变负)，**某一端高度递减到0就在该端停止**，更远的脊格不再生成也不作BFS种子(否则速率走到负数会从0重新爬起，在主山外长出孤立小山包)；③侧向BFS扩散(带9宫格连续性修正抑制突变)+5x5均值平滑；④只写回`plain`/`terrain_mountain`格子，高度取现有值和新值的较大者(多山脊叠加不互相压低)。
- 常量：`ALONG_*`(沿脊线衰减)、`LATERAL_*`(侧向坡度)、`RIDGE_*`(游走)，见文件顶部宏。

## RiverTerrain

贴图复用plain的diffuse，`normalPath`/`roughnessPath`留空走引擎fallback；河道靠地形凹陷+水面mesh表现。

### 起点与山体内段(D8)

1. 摊平`heights`/`isMountain`，山体格子按高度降序排序，初始流量1.0。
2. 按序遍历：在8邻居里找**坡度**(高度差÷距离，对角距离√2)最大的邻居——不能直接比高度，否则对角邻居系统性占优，流向全是45度斜线、转弯全是直角。最陡邻居是更低的山体格子则把`accum`加给它并记`flowTarget`；是非山体或局部坑则`flowTarget=-1`(流量到此为止)，但`exitTarget`仍记下"想流去哪个非山体格子"供出山段续用。
3. 起点判定：同一降序顺序遍历，`accum >= RIVER_FLOW_THRESHOLD`(30)且未标记、**不挨着已有河流格子**(侧面蹭上去不批准)、离所有已有起点距离`> RIVER_ORIGIN_MIN_SPACING`(200，即2000m)才批准为新起点，然后沿`flowTarget`链路一路标`isRiver`，直到出山/局部坑/撞到已标记的河。格子变成河流只有这条下坡蔓延一条路。
4. 出山口防过陡：从山脚往源头逆推，相邻两格高度差`> RIVER_MAX_STEP_DROP`(0.2)就截断，靠源头那段整体撤销，起点改到截断点。保留下来的山体段`riverDistance`从截断点重新从0起算，`ownChainSet`只含保留段。

### 出山后的蔓延(全有或全无)

每条出山链路立刻接着跑`continueRiverOutsideMountain`，只有最终流到**地图边界、海洋、或并入已有河**才保留，其余失败原因整条撤销(山体内段+出山段，起点也从`riverOrigins`移除)：撞到山、陷入局部坑、步数耗尽、流经地图中心区域(`CenterBlocked`，宽高各`3/8~5/8`，留给路网中心lot；山体内段在origin循环里单独扫一遍)。
- 初始流向：往回倒最多4格指向出山格，避免角度只能是0/45/90/135。
- 连续坐标+固定步距`RIVER_SAMPLE_STEP`(0.5)采样落在哪个格子；还在同一格则不触发判定。
- 流向角`theta`+角速度`angularVel`(标量)，每步角速度加`±RIVER_ANGULAR_VEL_JITTER`(0.015)并夹到`RIVER_ANGULAR_VEL_MAX`(0.02)。角速度是无均值回归的纯随机游走，上限不能大：过大(0.4)会16步转满一圈到处画圆。
- 采样格比上一格高就在8邻居里找最低格，`theta`直接指向它、角速度清零；连最低的都不更低就是局部坑，失败。
- **避让**：每步沿东南西北四轴探测，海洋看`RIVER_COAST_AVOID_DISTANCE`(10格)、不属于自己链路的河看`RIVER_NEIGHBOR_AVOID_DISTANCE`(4格)，取最近一处，`theta += 带符号夹角 * RIVER_COAST_STEER_COEF`(1/16)，夹角逐步缩小，平滑逼近。
- **碰河/碰海按8邻居判定**，不需要正好踩上。碰河判定必须排除自己链路已标记的格子(`ownChainSet`，含山体内段)，否则刚出山就把自己当成"别的河"终止。
- **入海接驳**：判定到海后`convertTouchingOceanToRiver`把触发判定的海洋格并进河流，再`extendIntoOcean`顺流向多推`RIVER_MOUTH_EXTEND_CELLS`(4)格，因为海洋贴岸一圈是高度0的"沙滩"，只接一两格河床公式切不过沙滩。
- **最小长度**：链路自己新标记的中心线格子数`< RIVER_MIN_LENGTH`(100)整条撤销(含并入别的河的短支流，被并入的那条不受影响)。

### 加宽、河岸与提交

- **加宽**(提交前一次，所有起点/碰撞/截断判定都基于宽度1的中心线，提前加宽会让碰撞检测被自己的宽度碰到)：每个中心线格子按`riverDistance`(相对自己链路起点流经的格数，山体内段和出山段接续计数，起点用过陡截断后保留的山体段长度)算半径`RIVER_BASE_WIDEN_RADIUS(1.5)+riverDistance/RIVER_WIDTH_GROW_PER_CELLS(200)*0.5`，欧几里得距离判定避免方块/八边形棱角。
- **河岸平滑**：到核心河道的欧几里得距离场(搜到`RIVER_BANK_SEARCH_RADIUS`)，`d<=RIVER_BLUR_RADIUS`(2)的格子**并入`isRiver`**(否则水面mesh范围停在加宽边缘、边界只外挪一格)，降深`RIVER_HEIGHT_DROP * bankFalloff(d)`，`bankFalloff`是线性后再过smoothstep。`R<d<R+1`一圈(`isBankOnly`)只按同一曲线降高度、不并入河流、不改地形、不加水面，避免粗高度出现台阶，路网/zone看到的河流范围不变。
- **提交**：每个河流格子`setTerrain(terrain_river)`、`setHeight(原高度 - RIVER_HEIGHT_DROP*falloff)`、`setWater(原高度 + SEA_LEVEL)`(和海同一公式，河口原高度接近0，天然对上海面；水位不随falloff打折，水面才连续)，并给8个非河/非海邻居铺同一水位，挡住凹陷边缘露出的水面侧面。**原本是`terrain_ocean`的格子(加宽/blur是纯几何扩散会卷进海洋格)水位必须固定`SEA_LEVEL`**，不能套"原高度+SEA_LEVEL"——海底深度会算出更低水位，入海口水面凹陷+海岸沙滩相对凸起，视觉上河海断开。河口海洋格的高度仍走统一下降公式，河床才能切过沙滩连到海床。
- **不要再做"入海口单独加宽5x5/isMouth/水位强制SEA_LEVEL/全局兜底扫描"**：试过多轮都没解决断开问题，根因是上一条的水位公式，已全部撤销，现在入海口除接驳和延伸外没有任何特殊处理。
- **河岸细化高度(17x17)**：粗高度10m一格，双线性插值在格子边界只有C0，水岸线会是折线。所有`setHeight`之后，对4x4模板(`x-1..x+2`)里有格子距离`<R+1.5`且不全是核心河道的格子调`setFineHeights`，值为`原高度双线性(p) - RIVER_HEIGHT_DROP*bankFalloff(d(p))`，`d(p)`是对距离场做Catmull-Rom双三次插值(`max(0,·)`)：距离场近似线性所以等值线光滑，核心河道内d恒0、河床平，整数格点上权重退化使四角等于粗高度(日志`max fine corner error`应≈0)。原高度用双线性：无细化数组的邻格共享边下降量为0，两边算法一致才无折痕。**必须在`setHeight`之后调用**(`SetHeight`会清掉共用格点的细化数组，见`terrain_mod.md`)。

## 依赖关系

- 依赖：`map/terrain_mod.h`、`common/utility.h`；River依赖Mountain+Ocean先跑完(`basic.cpp`依赖表声明`terrain_river`依赖`terrain_mountain`)。
- 被谁依赖：`Source/Basic/basic.cpp`(`GetModTerrains`/`RegisterModTerrains`)；路网(`roadnet_*`)读地形分类；`ForeverTerrainFrameworkComponent::BuildOceanMesh()`按"有没有水"铺水面，河流格自动纳入。

## 待办/后续阶段

- `RIVER_FLOW_THRESHOLD`等数值是起始值，按效果继续调。
