# green_mod.h / green_mod.cpp

## 职责

`GreenMod`是绿化区(Green)的mod基类：Core把一块多边形(`GreenShape`)交给mod，mod在里面布置**市民活动圆**、**树**和**自定义装饰物**。绿化区本身(多边形、有没有连路)由路网文件指定，mod只决定里面放什么。

## 关键设计

- **Core→mod 只读输入 `GreenShape`**：`xy`(顶点，有符号面积>0，可凹)、包围盒、`linked`(有没有边连到真实道路)。`Layout(shape)`基类实现把它们**拷进mod自己的成员**(在mod所在DLL里执行，容器分配器不会错位，见memory cross_dll_allocator_crash)，清空上次输出后调子类的虚函数`LayoutGreen()`；子类不覆盖`Layout`。Core只调虚函数，绝不直接改mod的容器。
- **输出是值类型**：`circles`(`GreenCircle{x,y,r}`，世界坐标地图单位)、`decorations`(`DecorationSpec`，位置相对**包围盒**：`(0,0)`=西北角，ratio乘包围盒宽高再加offset；不旋转)、`trees`(`TreeRequests`，参考系同装饰物)。Core拷走后丢弃多边形外的装饰物/树点，所以mod可以按包围盒随便撒。
- **圆的校验在Core**(见`Core/map/green.md`)：圆心必须在多边形内；连了路的绿化区里，每个圆按"圆心到各连路边的**线段最短距离**(垂足在段外取端点)"从近到远试，取第一条使"圆心→线段最近点"整条连线都在多边形内的边，都不行就丢弃该圆。mod不用自己管连通性，可以多给候选圆。
- **工具(非虚，编进各自DLL)**：
  - `FindLargestCircle(cx,cy,r)`：多边形内**避开已`AddCircle`的圆**之后还能放下的最大圆(polylabel式单元格优先队列，精度0.01=10cm，剩余半径<0.01返回false)。**不会自动加进circles**，调用方决定最终半径(可缩小)再`AddCircle`——这样连续调用可以得到互不重叠的多个圆。
  - `RandomPoint(x,y)`：多边形内均匀随机一点，首次调用耳切三角化并缓存，之后O(log t)、无拒绝采样。
  - `GetPolygonArea()`、`AddCircle/AddDecoration/AddTreeSingle/Line/Area`。
  - 内部随机统一用`GetRandom`，Core在mod实例的`RandomScope(GetInstanceStream(mod))`里调`Layout`，所以同种子重现。
- **子类还要提供两个static查询**(基类不声明，经`RegisterGreen`登记成函数指针)：`static float GetPowerFree()`、`static float GetPowerLinked()`，权重0=这类绿化区不选它。
- **命名**：`GetName`=类型+静态计数(无空格，地址按空白切)；`GetInstanceCounter/SetInstanceCounter`随存档保存。读档时mod是空实例，Core从存档恢复圆/装饰物快照，**不重跑Layout**。

## 依赖关系

- 依赖：`common/mod_test.h`、`common/mod_state.h`、`map/feature_spec.h`(TreeRequests/DecorationSpec/PointParams)、`map/general_geometry.h`、`map/polygon_util.h`。
- 被谁依赖：`green_factory.h`、`Core/map/green.h`、`Basic/map/green_park.*`。
