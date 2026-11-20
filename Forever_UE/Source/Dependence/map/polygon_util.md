# polygon_util.h / polygon_util.cpp

## 职责

2D多边形小工具，`GreenMod`基类和`Core/map/green.cpp`共用(原来这类函数散落在map.cpp/pier.cpp的匿名命名空间里，没法跨文件/跨DLL共用)。多边形用扁平数组`xy={x0,y0,x1,y1,...}`，首尾不重复，隐含最后一点连回第一点。**凹多边形可用，输入不能自交。** 全部是纯函数，只读入参，不碰全局状态，所以Core和mod各自编进自己的DLL也没有跨DLL问题。

## 关键设计

- **绕向约定**：`PolygonSignedArea`(shoelace)>0 表示"规整后"的多边形，`EnsurePositiveArea`把负的翻转过来。Core读入路网绿化时先规整；"逆时针"只是让"边的哪一侧是内部"有确定定义，**不要求凸**。
- **点在多边形内**：射线法，**边界上(1mm容差)算内**，所以贴边的圆心/装饰物不会被丢掉。
- **`SegmentInsidePolygon`**(圆心→接路点连线必须整条在多边形内)：线段与每条边求交点参数，把线段切成若干小段，**每段的中点**都要在多边形内；共线重叠的边取端点投影。凹角处的连线会被正确判出界。
- **`TriangulatePolygon`**(耳切)：`GreenMod::RandomPoint`用它做面积加权均匀采样(首次调用缓存三角形+面积前缀和，之后每次O(log t)、没有拒绝采样)。找不到耳朵时丢掉近似共线的顶点继续；彻底退化返回false，调用方有包围盒拒绝采样兜底。
- **`SignedDistanceToPolygon`**：内部为正的到边界距离，`GreenMod::FindLargestCircle`的单元格优先队列(polylabel式)用它：距离函数是1-Lipschitz，已有圆的"到圆边缘距离"取min后仍是1-Lipschitz，所以剪枝照样成立。

## 依赖关系

- 无外部依赖(只用`<vector>`/`<cmath>`)。
- 被谁依赖：`green_mod.cpp`(FindLargestCircle/RandomPoint)、`Core/map/green.cpp`(圆/装饰物过滤)、`Core/map/map.cpp`(InitGreens、LocateGreen)。

## 待办/后续阶段

- 无。
