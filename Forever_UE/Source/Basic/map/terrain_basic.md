# terrain_basic.h / terrain_basic.cpp

## 职责

`OceanTerrain`/`MountainTerrain`/`RiverTerrain`是Terrain域三个默认地形实现。

## OceanTerrain

`GetPriority()=0.9f`(低于`MountainTerrain`的0.95，山先跑、海后跑)，
`diffusePath="/Game/Asset/Textures/Terrain/PlainDiffuse.PlainDiffuse"`(和plain地面同一张
贴图——"海洋"的视觉效果完全来自独立的水面mesh/材质，不靠海底贴图区分)。

**为什么海洋要排在山之后，不是最先跑**：原来是海先跑(`1.0f`)、山后跑(`0.9f`)。山体
生成时虽然已经有保护——拒绝"中心点落在海洋格"的山(`centerX/centerY`落在`"terrain_ocean"`上直接
跳过这次尝试)，写回阶段也会跳过已经是`"terrain_ocean"`的格子——但这套保护只在"海先生成好"的
前提下才有意义；真正的问题出在反过来看这件事：山体边缘自己有侧向衰减(平滑降到0，见
`MountainTerrain`一节)，但海岸线紧贴着山脚雕刻出来的那一截地势跟山体衰减曲线完全是
两套独立算出来的数字，两边对不上，接缝处出现台阶状的硬悬崖。调整成**山先跑、海后跑**，
同时在海岸线雕刻那一步(下面第1步)加一条保护：**已经是`"terrain_mountain"`的格子不雕刻**(每个
方向的雕刻循环里都有`if (getTerrain(x,y)=="terrain_mountain") continue;`)，让海绕开已经摆好的
山体，不去覆盖它。注意这不是严丝合缝的高度拼接——海岸线BFS的种子高度仍然固定是0(不会
反过来去读相邻山体边缘实际的高度)，只是不再用"整格强行改写成海拔深度"的方式去覆盖一座
还杵在那里的山，裂开的硬悬崖没有了，剩下的落差只是山体自身衰减到接近0边缘和海岸线0之间
本来就很小的自然差值，不是人为雕刻出来的台阶。**这个跳过只影响"要不要把这格标记成海"，
不影响
海岸线BFS怎么判断谁是种子**——第2步"是不是海岸线"的判定只看`isOcean[][]`这个布尔数组
(某格是不是海)，不管陆地那边究竟是plain还是mountain，跳过山体之后它在`isOcean`里还是
`false`，跟任何普通陆地格子一样会被当成"海岸线的陆地邻居"，不需要为山专门加特判逻辑。

`DistributeTerrain`两步走：

1. **海岸线**：随机决定1-4条地图边(`coastMask`，至少保证一面)，每条边独立走一次带随机
   游走的深度轮廓(`coastDepth`基准+`coastSlope`每行小幅摆动，越界回弹)，标记该范围内的
   格子为`"terrain_ocean"`类型——**已经是`"terrain_mountain"`的格子跳过，不雕刻**(见上面"为什么海洋要
   排在山之后")。
2. **深度**：以陆地相邻的海洋格为海岸线种子(高度0)，做BFS向内陆扩散，下降方向沿"离开地图
   中心"的方向；贴岸第0/1圈强制拉平为0，从第2圈起才真正变深，深度上限`OCEAN_MAX_DEPTH`。
   写回高度的同一个循环里顺手给每个海洋格调`setWater(x,y,true,-0.1f)`——水面固定在-0.1
   地图单位，和改之前视觉上完全一样，只是换成了per-cell的`setWater`机制（以前是
   `SetupTexture()`填一次`waterHeight`、整次`DistributeTerrain`调用全部格子共用，见
   `terrain_mod.md`"水面"一节），现在只是暂时还没利用"每格可以不同"这个能力。

常量：`OCEAN_DEPTH_RATE=0.05f`、`OCEAN_SLOPE_JITTER=0.01f`、`OCEAN_MAX_DEPTH=60.0f`。

## MountainTerrain

`GetPriority()=0.95f`(最高，最先生成，早于`OceanTerrain`——原因见`OceanTerrain`一节
"为什么海洋要排在山之后")，`diffusePath="/Game/Asset/Textures/Terrain/
RockDiffuse.RockDiffuse"`(唯一和plain视觉不同的地形)。

`DistributeTerrain`按密度公式`densityScale=width*height/(512*512)`(整数除法)决定山脊数量
(`densityScale>1`时`4+GetRandom(densityScale*2)`条，否则0条)。**地图尺寸必须严格大于
512x512才会生成山**(`densityScale`在512x512刚好算出1，取不到`>1`)；地图尺寸同时必须保持
2的整数次幂，理由见`Source/Forever/Framework/ForeverFrameworkActor.cpp`里`kDefaultMapWidth`
旁的注释(LOD覆盖范围计算也依赖这一点)。每条山脊：

1. 从随机中心点(要求落在`"plain"`且离地图中心足够远)向两端随机游走生成弯曲多段线。
2. 沿脊线从随机峰值格(20-49高度)向两端衰减。
3. 侧向BFS扩散(带9宫格连续性修正抑制突变)+5x5均值平滑。
4. 只写回`"plain"`/`"terrain_mountain"`格子(不覆盖ocean/construction)，高度取现有值和新值的较大者
   (允许多座山脊叠加，不互相压低)。

常量：`ALONG_BASE_RATE=0.30f`、`ALONG_RATE_JITTER=0.10f`、`ALONG_RATE_MIN=-0.15f`、
`ALONG_RATE_MAX=1.00f`、`LATERAL_BASE_RATE=0.35f`、`LATERAL_INIT_JITTER=0.15f`、
`LATERAL_PROP_JITTER=0.06f`、`RIDGE_STEP_LEN=1.0f`、`RIDGE_TURN_JITTER=0.15f`、
`RIDGE_SUB_STEPS=4`，和`OceanTerrain`共用同一张8方向邻居偏移表(`neighborOffsets`)。

## RiverTerrain（山体内D8流量 + 流出山体后在任意地形蔓延，调试显示）

`GetPriority()=0.5f`(低于`MountainTerrain`的0.95和`OceanTerrain`的0.9——`DistributeTerrain`
跑到这里时要能读到Mountain+Ocean写完之后的最终地形分类)，`diffusePath="/Game/Asset/Textures/Terrain/
PlainDiffuse.PlainDiffuse"`(跟`OceanTerrain`一样复用平地贴图——调试阶段曾经用纯色
`Blue`贴图确认河道位置，位置确认没问题之后换成平地贴图；河道本身靠地形凹陷
(`RIVER_HEIGHT_DROP`)+上面盖的水面mesh已经能表现出来，不需要再用地表材质本身的颜色
额外强调；`normalPath`/`roughnessPath`留空，走现有fallback填平法线+中性粗糙度)。

`DistributeTerrain`是标准D8单流向流量累积算法，**范围限制在`"terrain_mountain"`格子内**（第一
阶段只做山体部分，流到非山体格子就停止）：

1. 摊平全图`heights`/`isMountain`两个数组（`y*width+x`寻址），收集所有`isMountain`为真
   的下标按高度降序排序。
2. 所有山体格子初始流量1.0（`accum`数组）。
3. 按排好的顺序遍历每个山体格子：在8个邻居（和`OceanTerrain`/`MountainTerrain`共用的
   `neighborOffsets`）里找**坡度**(高度差÷距离，对角邻居距离`sqrt(2)`、轴向距离`1`)
   最大的一个——**坡度修复**：不能直接比原始高度，对角邻居天然比轴向邻居远`sqrt(2)`
   倍，同样的坡度下对角邻居的高度差会更大，直接比高度会让流向系统性地只往对角方向走、
   转弯全是90度直角，按坡度比才能让轴向/对角公平竞争。如果最陡的邻居比自己更低且也是
   山体，把自己的`accum`加到它上面（它必然排在自己后面处理，不会漏加），同时记下这个
   格子自己的D8下坡目标`flowTarget`；如果最陡的邻居不是山体，或者没有邻居比自己更低
   （局部坑），`flowTarget`记`-1`，就停在这里，不往外送——这就是"流到非山体就停止"。
4. **不是"谁过线就标记成河流"，而是"起点+下坡蔓延"**：按同一个降序顺序遍历每个山体
   格子，`accum`达到`RIVER_FLOW_THRESHOLD`（当前30.0f，调试起始值）且还没被标记成河流
   时，判断能不能批准成一个新的"河流起点"——①挨着任何已有河流格子就跳过（说明贴着某条
   已有河流的侧面，不是它的下坡延伸方向，不批准也不标记，避免侧面一整排格子都过线互相
   蹭成一大片）；②跟所有已有河流起点的距离都要超过`RIVER_ORIGIN_MIN_SPACING`（当前
   100.0f=1000m，1 element=10m）。两条都过了才批准成新起点，然后沿`flowTarget`链路一路标记成
   `"terrain_river"`，直到流出山体/局部坑（`flowTarget=-1`）或者撞上已经标记过的河流（提前
   停止）——沿途不用再检查`accum`，下坡链路上流量只会越来越大，起点过线整条链路自然
   都过线。格子变成河流的唯一途径就是这条下坡蔓延，没有"挨着河流就自动并入"这种规则。

河流格子复用`Element::terrain`这个已有字符串字段——和`OceanTerrain`/`MountainTerrain`写
`"terrain_ocean"`/`"terrain_mountain"`完全同一个机制，`"terrain_river"`作为一种新地形类型，会自动在`Map::
InitTerrains()`的贴图数组槡位分配里拿到自己的槡位，不需要改任何渲染代码。

**第三阶段：真正改地形——降河床高度+倾斜水面**：最终提交河流格子那个循环（`isRiver[idx]`
为真才会走到的那个`for`）里，每个格子除了`setTerrain(x,y,"terrain_river")`，还顺手：
1. `setHeight(x,y, getHeight(x,y) - RIVER_HEIGHT_DROP * dropFalloff[idx])`——河床比
   原地形低一截(`RIVER_HEIGHT_DROP=0.5f`)，核心河道`dropFalloff=1`按满额下降，河岸
   平滑圈（见下面"河岸高度平滑"一节，并入`isRiver`的那圈）按距离衰减少降一些。
2. `setWater(x,y,true, getHeight(x,y)修改前的原始值 + SEA_LEVEL)`——水位不是"河床高度
   (已经扣掉RIVER_HEIGHT_DROP)+一个小偏移"，是跟`OceanTerrain`完全同一个公式："原地形
   高度+`SEA_LEVEL`"(`-0.1f`，两边共用的同一个命名常量，之前`OceanTerrain`写的字面量
   `-0.1f`也换成了这个)。河口附近原地形(`plain`)高度本来就接近0，算出来的水位天然就是
   `SEA_LEVEL`，正好和海面对上，不需要再夹一下；原地形越往上游/越高，水位跟着一起抬，
   倾斜还是会自然出来——相邻格子原地形高度本来就不一样，统一加一个常数偏移不影响相对
   高低，水面照样跟着地形坡度走。`setWater`是`terrain_mod.md`"水面"一节说的那个per-cell
   独立回调，`ForeverTerrainFrameworkComponent::BuildOceanMesh()`的格子筛选条件本来就是
   "有没有水"而不是"是不是ocean类型"，河流格子设了水面之后自动被这个既有函数纳入同一张
   水面mesh，不需要改Forever的渲染代码。
3. 河床比周围矮一截，只给河流格子自己铺水面会在凹陷边缘露出水面mesh的侧面——顺手给
   这个格子的8个邻居也铺上同一片水位(跳过已经是`"terrain_river"`/`"terrain_ocean"`的邻居，不覆盖它们
   各自更准确的水位；河流邻居轮到自己那个格子时会用自己算出来的水位覆盖回来，和这一步
   的先后顺序无关)。

**出山口附近防止河流过陡**：`propagateDownhill`现在从山脚(`mountainPath`末尾)往山上
(下标减小方向)逆推检查，只要连续两格之间(更靠源头那个减去更靠山脚那个)高度差超过
`RIVER_MAX_STEP_DROP`(1.0f)，就在那里截断——靠源头那一段(含原来的起点)整体撤销
(`isRiver`重新置`false`)，河流起点(`riverOrigins`里对应那一项)改成截断点，只保留靠
山脚、坡度更缓的那一段；没有过陡的地方就不截断，起点还是原来的。

**依赖`Blue`贴图实际分辨率**：`Blue.png`不一定是2048×2048，`ForeverTerrainFrameworkComponent::
LoadRawBGRA8`已经改成运行时按源贴图实际尺寸最近邻重采样到所需大小，不要求这张贴图精确
符合2048×2048（这是通用改进，不是河流专属，见该文件的md）。

**第二阶段：流出山体之后在任意地形上继续蔓延，全有或全无**——每条确实"流出山体"的链路
（终点`flowTarget=-1`但`exitTarget>=0`，这个判断见上面第3步），立刻在同一次遍历里接着
跑第二阶段（不再像早期版本那样攒一批`pendingExits`留到最后才跑）——第二阶段判断"是否
已经是河流"查的是内存里的`isRiver`数组，不是`getTerrain`，所以不需要先把山体内的河流
提交进真实地图。只有链路最终流到地图边界或者流到`"terrain_ocean"`才算成功、这条链路(山体内
`propagateDownhill`标记的那段+第二阶段标记的那段)才会在最后一次性`setTerrain`提交进
真实地图；中途撞到`"terrain_mountain"`、或者陷入局部坑（无论是山体内部的坑，还是流出山体之后
的坑）都算失败，这条链路从起点开始整条撤销(对应的格子在`isRiver`里重新置`false`，这个
起点也从`riverOrigins`里移除，不再占间距名额)；撞到别的河（已经判定为"成功"、提交等待
中的另一条河）算并入，不算失败也不算新起点，沿用"起点+下坡蔓延"规则里既有的并入机制。

**流经地图中心留给路网的那块区域也算失败，整条撤销**：地图正中心`宽高方向各3/8~5/8`
那片区域(`inCenterRegion`)是留给井字路网中心lot(`AREA_OFFICIAL_HIGH`，见
`roadnet_basic.cpp`/`roadnet_basic.md`)用的，河流不能占这块地——不管是山体内
(`propagateDownhill`返回的`mountainPath`，逐格扫一遍`inCenterRegion`)还是流出山体之后
(`continueRiverOutsideMountain`主循环每步都查，连同链路起点自身那一次检查)，只要碰到
就返回新增的`RiverEnd::CenterBlocked`，不在`kept`的白名单(`MapEdge`/`Ocean`/
`OtherRiver`)里，走的是跟`Mountain`/`Pit`完全一样的"链路从起点开始整条撤销"流程。山体内
这段不经过`continueRiverOutsideMountain`那套`kept`机制(`propagateDownhill`本身不区分
成功/失败)，所以单独在origin循环里`propagateDownhill`返回之后立刻扫一遍`mountainPath`，
碰到中心区域就直接撤销`mountainTouched`、`continue`到下一个候选起点，不会往下走陡度
截断或者流出山体那一段逻辑。

**长度不足`RIVER_MIN_LENGTH`(100格)的链路整条撤销**：在加宽之前、按宽度1的中心线判断，
每条链路只算它自己新标记的格子数——流出山体的链路=过陡截断后保留的山体段
(`mountainKeptLen`)+流出山体之后的`plainTouched`(含入海口接驳/延伸格子)，在`kept`判定
之后检查，不够就改成`kept=false`走同一套撤销；山体内就并入既有河流的支流(`flowTarget
[finalIdx]>=0`那个分支)用`mountainPath.size()`。所以并入别的河的短支流也会被取消，但它
并入的那条河不受影响(那条河更早处理，已经单独通过了长度检查)。撤销时同样`pop_back`
起点，附近的候选起点之后还能重新发源。日志里的`too short=`是这类撤销的数量。
**"撞到别的河"按8邻居判定，不是非得正好采样到河流格子本身**——当前格子或者它的8个
邻居里只要有一个已经是河流，就算碰到，立刻并入；不然两条河会贴着彼此平行流一段才真正
重合，中间那道缝隙视觉上很突兀。**必须排除"这条链路自己标记过的格子"**——否则刚出山
体第一步，紧挨着的就是山体内`propagateDownhill`那段自己标记的河流格子，立刻被当成
"碰到别的河"终止，河基本走不出山体。`continueRiverOutsideMountain`多接一个
`ownChainSeed`参数（山体内这条链路自己标记过的格子），用`unordered_set`记录"属于自己"
的格子，连同第二阶段自己新标记的也一起算进去，`hasNearbyRiver`检查时排除这些，只有真正
"别的"河才会判定成碰撞。

**"流到海"同样按8邻居判定**：挨着`"terrain_ocean"`格子（哪怕采样点本身还没真的落进海里一格）
就算到了，和"碰到别的河"同一个道理，不用非得正好踩进海里那一格。**判定成功之后要把
触发判定的那个/那些海洋格子也并进河流**（直接`markRiver`，等最后统一提交时会被改成
`"terrain_river"`类型）——不然河流在岸边几格外、还没真正碰到海洋格子本身的地方就断掉了，中间
缺一段看起来像河流凭空消失在半路。

**接上之后还要再往海里多推`RIVER_MOUTH_EXTEND_CELLS`(4)格**：海洋贴着陆地那一圈其实
是"沙滩"——`OceanTerrain`里海岸线+往内一圈(ring≤1)高度固定是0，只比固定水位`SEA_LEVEL`
(-0.1)高一点点。只接上刚检测到的这一两格海洋格子，河床高度公式(`原高度-RIVER_HEIGHT_
DROP*falloff`)可能还没真正算到沙滩后面更深、更低的海域，沙滩这一截挡在中间，看起来
还是没接上。`convertTouchingOceanToRiver`接上缺口之后，紧接着调`extendIntoOcean`，
顺着原来的流向(判定在链路一开始就触发用`initDirX/initDirY`，判定在主循环半路触发用
当前这一步的`dirX/dirY`)继续往前推4格距离(按`RIVER_SAMPLE_STEP`的粒度采样，不是直接
跳4整格)，沿途新采到的格子一律`markRiver`——不做哪怕是最低高度邻居的脱困检查，单纯
往海里多插一段，让这几格也走一遍河床高度公式，强制切过沙滩连到更深的海里。这几格
原来大概率就是真海洋格子，提交时`wasOcean`判断会让它们水位走固定`SEA_LEVEL`那条路，
高度走通用下降公式——不会复用"入海口水位要对齐"那次修的例外逻辑，天然兼容。

**离海岸线太近就按比例往海的方向拐，不让河擦着海岸线走**：`continueRiverOutsideMountain`主循环
每一步在积分角速度之后，从当前格子沿东/南/西/北四个轴向各往外探测，只看`RIVER_COAST_AVOID_
DISTANCE`(10格=100m)以内、取最近的一处`"terrain_ocean"`；找到就算出"海所在轴向的角度"与当前流向
`theta`的带符号夹角`diff`(归一化到(-π,π])，`theta += diff * RIVER_COAST_STEER_COEF`(1/16)。
`diff`每步都按更新后的`theta`重算，所以转的量逐步缩小(夹角80°：这步转5°，下步转75°×1/16…)，
是平滑逼近而不是一次掰过去。河背离那片海流时(夹角超过90°)也照常按比例拐向海。
**别的河同理**：同样四个轴向探测`isRiver`里不属于自己这条链路(`ownChainSet`之外)的格子，距离只有`RIVER_NEIGHBOR_AVOID_DISTANCE`(4格=40m)，向最近的一处(海或河取最近)拐，比例系数相同，最终靠近到8邻居内仍走原来的"并入别的河"。这一步在`hasNearbyOcean`之前，河最终靠近到8邻居范围内仍然走原来的"流到海"收尾。

**加宽河流，越往下游越宽**：以上所有起点/碰撞/截断判定都是基于宽度1的中心线做的
（提前加宽会让`hasNearbyRiver`这类基于`isRiver`的碰撞检测在中心线还没走开多远时就被
自己的宽度碰到），所以在最终提交之前单独加一次膨胀。宽度不再是全图统一的固定值——
`riverDistance[idx]`记录这个格子相对它所在这条链路**自己的起点**流经了多少格(山体内
`propagateDownhill`和流出山体后`continueRiverOutsideMountain`接续计数，不是从0重开，
`continueRiverOutsideMountain`多接一个`startDistance`参数，传进去的是"过陡截断之后
真正保留下来的那段山体路径长度"，不是截断前的原始长度)，每个中心线格子按自己的
`riverDistance`算一个专属半径：
`radius = RIVER_BASE_WIDEN_RADIUS(1.5) + riverDistance/RIVER_WIDTH_GROW_PER_CELLS(200)
* 0.5`，起点处半径1.5刚好覆盖3x3的8个邻居(对角距离√2≈1.414<1.5)，对应原来固定宽度3
的效果；每流经200格，半径多加0.5——对称膨胀一次两侧都变宽，所以半径+0.5相当于总宽度
+1，先试这个比例看效果。半径判定用欧几里得距离（`sqrt(dx²+dy²)<=radius`），不是
8邻居格数，和河岸blur那次改法同一个思路，避免方块/八边形的棱角。之后那个"给河流格子
自己的8邻居铺水面"的逻辑接在加宽后面跑，水面会在加宽后的河道基础上再往外扩一圈，
不用改。

**入海口"加宽5x5"这条路已经放弃**：之前尝试过在河流判定"碰到海"的那个点单独再加宽
成5x5(`widenMouth`/`RIVER_MOUTH_WIDEN_RADIUS`)、给这块区域单独记`isMouth`标记、水位
强制等于`SEA_LEVEL`、河床强制以`SEA_LEVEL`为基准往下降，甚至还加了一个"对最终河道
整体再扫一遍、不管链路怎么结束都补一次加宽"的兜底——这些都是在"猜可能是哪里不够宽/
不够低"的基础上做的局部修补，试了几轮都没解决"最标准的一条河垂直流入海里、入海口
河面和海面断开"这个最基础的场景，说明问题根本不在"宽度不够"或者"没对齐"，而是更底层
的东西没搞清楚。**已经全部撤销**（`RIVER_MOUTH_WIDEN_RADIUS`/`widenMouth`/`isMouth`/
那个全局兜底扫描都删掉了，`DistributeTerrain`现在除了`convertTouchingOceanToRiver`
接驳触发判定的那一两个海洋格子之外，没有任何额外的"入海口"特殊处理），河床/水位
公式统一回到最朴素的"原地形高度−`RIVER_HEIGHT_DROP*falloff`"/"原地形高度+
`SEA_LEVEL`"，不区分是不是靠近海口。

**诊断：不再猜，直接打印入海口的真实数据**——`DistributeTerrain`末尾原有的
`river_debug.log`追加逻辑里，新增了针对每一条"成功碰到海"(`RiverEnd::Ocean`)链路的
专项记录：提交完(地形/高度/水位都写进真实地图之后)，把这条链路最后一个格子周围9x9
范围内所有格子的`terrain`/`height`/`water`打到日志里(水位没有`getWater`回调读回，
改成在commit循环里顺手记一份`debugWaterHeight`；没被river碰过的`"terrain_ocean"`格子水位
必然是固定的`SEA_LEVEL`，直接按这个常量打，不用猜)。

**真正的根因(靠上面这份诊断数据找到的，不是猜的)：加宽/blur是纯几何扩散，会把一圈
真正的海洋格子也卷进`isRiver`**——这些格子原来的高度是`OceanTerrain`自己算的海底
深度(比如-0.09、-0.27，不是0)，如果水位也套河流那套"原地形高度+`SEA_LEVEL`"公式，
算出来的水位会比海洋自己固定铺的水位(`SEA_LEVEL`=-0.1)更低(深度越深、水位越低，实测
能低到-0.27)——入海口附近的水面因此凹下去一块。旁边没被卷进来的真海洋格子水位还是
固定的-0.1，而海洋自己贴着海岸线那一圈格子(高度正好是0，比它自己的水面-0.1高
0.1，这是`OceanTerrain`一直都有的正常"沙滩"效果，单独看没有问题)相对这片凹陷的水面
就显得"从水里多凸出来了一截"——水面凹陷+沙滩相对凸起这两个现象叠加，视觉上就是河流
和海洋从中间断开了。修复：提交循环里`setTerrain`覆盖之前先判断这个格子原来是不是
`"terrain_ocean"`——是的话水位直接用固定的`SEA_LEVEL`(跟`OceanTerrain`自己铺水面完全一样)，
不走"原地形高度+`SEA_LEVEL`"那套公式；高度还是用河流自己的下降公式，不用管。这是
按"这个格子真实的原始地形类型"判断的，覆盖所有被加宽/blur卷入的真海洋格子，不是
之前那种按坐标范围圈一块"入海口"的特殊区域。

**河岸高度平滑**：河床是硬降`RIVER_HEIGHT_DROP`，紧贴着的非河流格子还是原高度，河道
边界不规则的地方会出现台阶状的锯齿河岸。加宽之后、提交之前算一遍每个格子到河流的距离，
距离越近的falloff越接近1，超过`RIVER_BLUR_RADIUS`(=2)完全不下降（falloff=0）。**这圈
扩散到的格子直接并入`isRiver`**（不是单独调一下高度就完事）——只改高度不改地形/不给
水面的话，水面mesh的覆盖范围还是停在加宽后的河道边缘，边缘往外一格地形已经被削平滑了
一截但没有水盖过去，侧边界只是往外挪了一格没有真正消失。并入之后这些格子会跟核心河道
格子一起走同一个提交循环：`setTerrain`/`setHeight`/`setWater`/邻居水面填补全都适用，
区别只是`setHeight`用`RIVER_HEIGHT_DROP * falloff[idx]`（核心河道`falloff=1`按满额
下降，外圈按比例少降，离河流越远降得越少，做出缓坡代替直角）；水位`setWater`不受
falloff影响，和核心河道用同一个"原地形高度+`SEA_LEVEL`"公式，这个公式本来就按每个
格子的原始高度各自算，天然连续，不需要额外打折。

**距离算法：欧几里得距离+smoothstep缓动，不是BFS格数**——第一版是多源BFS，按8邻居
"走了几步"算距离，每步不管是直走还是斜走都算1步。这样算出来的等距离线其实是个
八边形/方块(切比雪夫距离的形状天生带棱角)，所以边缘看起来是"圆角锯齿"——半径调大只是
让锯齿圈更宽，棱角本身并不会消失，因为问题出在距离的度量方式，不是半径不够。改成直接
算每个格子到最近河流格子的**欧几里得距离**(`sqrt(dx²+dy²)`的真实直线距离)，等距离线
才会是真正的圆弧而不是多边形。为了避免对全图两两格子算距离(O(N²))，改成以每个河流
格子为中心，只扫它周围`(2*RIVER_BLUR_RADIUS+1)`见方的局部范围，对范围内每个格子取
"到这个河流格子的距离"和"已经记录的最小距离"中更小的那个——河流格子数量通常远小于
全图格子数，整体复杂度仍然可控。falloff本身也不再是纯线性，再过一次smoothstep缓动
(`t*t*(3-2t)`)，让下降速度在半径边界处顺滑地从0起步、顺滑地过渡到0，不是一进圈就
按固定速率突然开始下降——视觉上坡面两端都更圆润，不会在刚进入平滑圈的那一条线上出现
斜率突变。

**并入范围和衰减范围对齐：`R<d<R+1`这一圈只降高度**（R=`RIVER_BLUR_RADIUS`）——
smoothstep的`t=1-d/(R+1)`要到`d=R+1`才降到0，但以前只有`d≤R`的格子会降，`d=R`那一格
还降`0.26×RIVER_HEIGHT_DROP`，紧挨着的`d=√5`就是0，粗高度本身有一个台阶。现在
`R<d<R+1`的格子(`isBankOnly`)也按同一条smoothstep降高度(最多约0.16×5m，降完仍高于水位，
是干地)，但**不并入`isRiver`、不改地形类型、不加水面**，路网/zone看到的河流范围不变。
距离搜索半径是`RIVER_BANK_SEARCH_RADIUS`(=R+2)，更远的格子统一记成这个值。

**河岸细化高度(17×17)**：粗高度10m一格，UE双线性插值在格子边界只有C0连续，水岸线(河床
和水面的交线)会是一段段折线。所有`setHeight`跑完之后，对4×4模板(`x-1..x+2`)里有格子
距离`<R+1.5`、又不全是核心河道的格子调用`setFineHeights`，数值是
`原高度双线性(p) - RIVER_HEIGHT_DROP*bankFalloff(d(p))`，其中`d(p)`是对距离场做
Catmull-Rom双三次插值(再`max(0,·)`)：
- 距离场近似线性，Catmull-Rom精确还原线性和常数，所以d的等值线(水岸线)是光滑曲线；核心
  河道里d恒为0，河床保持平。
- 整数格点上插值权重退化成`(0,1,0,0)`，细化数组四角正好等于粗高度(日志里的
  `max fine corner error`应该≈0)。
- 原高度用双线性不用双三次：没有细化数组的邻格在共享边上的下降量是0，两边都是双线性才
  完全一致，不会出现小折痕。相邻两块细化数组的共享边只依赖同一列/行的数据，结果完全相同。
- 必须在`setHeight`之后调用，原因见`terrain_mod.md`(SetHeight会清掉共用格点的细化数组)。

**河口处blur/加宽扩散到的海洋格子要跟河流格子套同一个公式，不能保留原高度**：加宽+BFS
是纯几何扩散，不看原来是什么地形类型——河流流到海附近时，扩散圈有一部分会扫到真正的
海洋格子(不是前面`convertTouchingOceanToRiver`接驳用的那一两个"桥接"格子，是离河道
稍远一点、单纯因为在`RIVER_BLUR_RADIUS`范围内被卷进来的海洋格子)。岸边浅滩的海洋高度
本来就接近0，不比降过的河床低——这部分格子如果保留原高度不下降，就会在河流和海洋
中间留一道比河床高的"坝"，把两边截断，河流看起来流不到海里（曾经按"保留原海洋高度/
固定SEA_LEVEL水位"特殊处理过，结果就是这个问题，已经改回来）。所以这部分格子和核心
河道、普通河岸blur格子完全一视同仁，套同一个`原高度-RIVER_HEIGHT_DROP*falloff[idx]`/
`原高度+SEA_LEVEL`公式，把这段地形也跟着压低，河床和海床才能连起来。

流出山体之后的具体规则：

0. **初始流向不能只看出山前最后一步**：`propagateDownhill`现在连同路径一起返回（按
   经过顺序的格子列表），初始流向不是简单地用"出山前最后一格→出山第一格"这一步算
   （那一步必然是D8的8个离散方向之一，归一化后角度只能是0/45/90/135这几个值）——
   往回倒最多4格（链路本身不够长就用链路起点），从那一格直接指向出山格子算向量，分量
   通常不再是±1，角度自然不再卡在那几个离散值上。
1. **按高度通用处理任意地形，不假定plain**：只看`getHeight`，不管具体地形类型是什么。
2. **连续坐标+固定步距采样，不是Bresenham画线**：维护一个连续坐标`(posX,posY)`，每步
   沿当前流向前进`RIVER_SAMPLE_STEP`（0.5，element单位）距离，四舍五入到最近的
   element；如果落在的还是上次采样停留的那个格子（还没走出去），这次采样不触发任何
   标记/判定，直接进入下一步继续累积距离。
3. **流向用角度表示，偏向速度是角速度(标量)，只保留这一层**：不再叠加偏向加速度
   （之前那套"加速度→速度→流向"的二阶积分太难控制）。`theta`是流向角度(弧度)，
   `angularVel`是角速度(标量，不是向量)；每步直接给`angularVel`加一个带正负号的
   随机小量（`RIVER_ANGULAR_VEL_JITTER`，当前0.015f），夹到`RIVER_ANGULAR_VEL_MAX`
   （当前0.03f弧度/步——这个值不能太大：角速度是没有均值回归的纯随机游走，长时间贴着
   上限是常态不是意外，0.03大约对应200步/100个element/1000米转满一圈，之前0.4那个值
   16步就转满一圈，到处像画圆）积分更新`theta`，`(cosθ,sinθ)`就是当前流向。角度/
   角速度都是标量，不存在"向量模长太小导致方向被一次扰动整个甩转"这种问题。
4. **采样到的格子比上一格高就强制刷新角度**：说明角速度把路径带上坡了，在这个格子的
   8邻居里找**高度最低**的那个（不是坡度最大的——这里单纯是脱困，不需要按距离把对角
   方向归一化那一套），把`theta`直接刷新成指向它的角度、`angularVel`清零（不带着刚才
   走错方向时积累的转速继续转）。如果连这个"最低的"也不比当前格子更低（真正的局部坑，
   8邻居全都更高），直接判定这条链路失败。
5. 有一个安全上限`(width+height)*2/RIVER_SAMPLE_STEP`步，步数耗尽也按失败处理。

## 关键设计

- `MountainTerrain`的重试预算是全局共享的——`Counter counter(200)`在整个`mountainCount`
  循环外面只创建一次，不是每座山各自200次；某次候选点不合格时`attempt--`重试，直到200次
  预算耗尽为止。
- `"plain"`不是这里注册的Mod——它是`Element::terrain`的硬编码默认值(见`map.md`)，两个
  地形类只负责"从plain上面覆盖出ocean/mountain"，不负责生成plain本身。

## 依赖关系

- 依赖：`map/terrain_mod.h`、`common/utility.h`。
- 被谁依赖：`Source/Basic/basic.cpp`(`GetModTerrains`/`RegisterModTerrains`导出函数)。
