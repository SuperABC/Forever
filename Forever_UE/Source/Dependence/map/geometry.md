# geometry.h / geometry.cpp

原样移植自旧工程`E:\Projects\Forever_UE\Source\Dependence\map\geometry.h/.cpp`，未做任何
修改（无windows/UE类型依赖，纯数学）。属于阶段4-0共享基础设施——不感知任何具体domain数据，
但被map域几乎所有概念（Terrain/Zone/Block/Room/Building/Roadnet）用作底层几何/图结构原语，
因此和`story`域的脚本引擎一起提前到阶段4-0迁移。

## 职责

提供导航图节点/连接、矩形空间递归分割两组能力：

- **`Node`/`Connection`/`Intersection`/`Road`**：导航图的点和边。`Connection`支持贝塞尔曲线
  控制点（`AddControls`），`GetPoint(f)`/`GetTangent(f, ...)`按**弧长比例**（不是Bezier多项式
  参数）取点和切线，即`f=0.5`对应曲线物理中点，供道路/人行道渲染与寻路采样使用。
- **`Quad`/`Lot`/`QuadBoundary`**：矩形空间及其递归二分算法`DivideSpace`——给定一组待摆放的
  子矩形（如一栋楼要切出的若干房间），按面积比递归对半分割父矩形，同时维护切分过程中产生/
  失效的导航节点与连接（切割线变成新走廊，原来贯穿的边被打断）。`Lot`在`Quad`基础上加了旋转
  角度和地块类型，`SetPosition(n1, n2, n3[, n4], margin)`可以直接用连续几个角点+内缩边距反推
  出矩形的中心/尺寸/旋转，这是旧工程从"三个或四个已知角点"生成建筑/房间轮廓的标准做法。

## 关键设计

- **`Connection`按值语义管理其端点/控制点**——构造函数、拷贝构造、赋值运算符里都会
  `new Node(...)`拷贝一份，析构时对应`delete`，不共享指针；这是因为`Node`本身很轻量（几个
  float+id），按值复制比引入引用计数更简单，唯一的例外是`Node::count`静态自增id计数器，拷贝
  构造/赋值时会同步推高`count`（保证之后新建的`Node`不会撞见已存在的旧id）。
- **`DivideSpace`用`Space`（geometry.cpp内部类，不出现在头文件）表示"两个子矩形合并后、尚未
  真正定位的中间节点"**——多于2个元素时先按面积从小到大两两打包成`Space`（有点像哈夫曼树的
  构造过程），最终展开成一棵二叉切分树，展开顺序决定了切一刀是横切还是竖切（谁的边界更长就
  沿哪个方向切）。
- **弧长参数化用惰性缓存**——`Connection::GetPoint`/`GetTangent`如果有控制点（真正的Bezier
  曲线，不是直线），会在**首次调用时**采样128个点建立弧长查找表（`arcLengthCache`），后续
  调用直接二分查表，`AddControls`会清空缓存强制重新采样。

## 依赖关系

- 依赖：`common/utility.h`（`OBJECT_HOLDER`标记宏、`GetRandom`——`Quad::SplitInto`用它随机
  决定两个子矩形谁在"下方"）、`common/error.h`（`THROW_EXCEPTION`）。
- 被谁依赖：阶段4迁移map域的`Terrain`/`Zone`/`Block`/`Room`/`Building`/`Roadnet`时，这些
  Core层类会持有/操作`Node`/`Connection`/`Quad`/`Lot`来表示自己的几何形状和导航图。

## 待办/后续阶段

- 阶段4：目前没有任何代码实例化这些类，等Map域（`Terrain`/`Zone`/`Block`起）迁移时才会真正
  被使用和验证。
