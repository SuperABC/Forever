# zone_residence_low.h / zone_residence_low.cpp

## 职责

`ResidenceLowZone`(`zone_residence_low`)：low密度住宅园区：只在low密度住宅区(AREA_RESIDENCE_LOW)的lot里放，园区里放两栋low密度住宅楼。

## 关键设计

- **三个类完全独立实现，没有共用基类，也没有共用的辅助函数**：各自有自己的`Assign`/`Layout`/`LayoutTrees`、命名计数器和函数内的局部常量；逻辑目前是同一份的拷贝，以后各自改，互不影响。只有急救包/树的网格路径、缩放这些`ZONE_KIT_*`/`ZONE_TREE_*`测试配置`#define`是共用的。
- **"一个本体独占一个mod实例"模型**：static的`Assign()`(决定占哪块lot、哪个方向、边距和进深)和非static的`Layout()`(围墙/大门/内部道路/内部建筑的实际布局)分开。
- **`Assign`**：只看对应密度的住宅区lot(高/中/低对应`AREA_RESIDENCE_HIGH/MIDDLE/LOW`)，**每块lot随机放0/1/2个**园区(`GetRandom(3)`)。目标面积固定`TARGET_ACREAGE`(20000)，换算成近似正方形边长；从WEST开始找第一个有边界路、临街长度够用的方向，第1个园区贴这条边的起点角(不居中)；第2个园区：这条边长度够2倍边长就贴另一端的角，否则换到正对的那条边的起点角，放不下时`RequestPlacement`失败、这个请求被跳过。不再看井字中心lot(`AREA_TEST`)，所以中心lot里没有园区了。请求给的沿路宽度和进深是**最小值**：`RequestPlacement`某一侧切完剩下的地不到3个单位(小路1+最小地块2)就不切，园区直接拉伸到那条边，所以实际占地可能比 `TARGET_ACREAGE` 开方的正方形更大。
- **`Layout`/`LayoutTrees` 按 quad 的实际尺寸排布**(不再假设正方形)：大门所在边长度、两个方向的半长、内部建筑沿路/垂直方向的尺寸、树线长度都从 `quad.GetSizeX/Y()` 取(大门朝西/东时北边长度取 Y)。
- **`Layout`**：大门放在`direction`这条边正中间(滑动双开门)，该边大门以外和其余3条边整段铺围墙(围墙进深`WALL_DEPTH`=0.2、大门宽度占边长`GATE_WIDTH_RATIO`=0.2)；垂直于大门所在边的一条人行内部道路过大门贯穿到对面，把园区分成两半，每半一栋对应密度的住宅楼。
- **`LayoutTrees`**：沿四面栅栏内侧每隔一段种一棵树，大门那一段让出来。坐标按"朝北"写，Core按大门方向旋转。西/东边从北角种到南角，南边各往里收一个间距避开已种的角，北边(大门边)分成大门两侧两段，开口两侧各再让出半个间距。
- **自定义模型/树的测试配置**用`#define`(`ZONE_KIT_*`急救包、`ZONE_TREE_*`树的网格路径/缩放/间距/内缩)，先给默认值，看过效果再调。
- 挂示例脚本`entity_event`(进出事件只发给这个实体自己的`Script`)，见`Resource/Story/entity_event.script`。
- 三个住宅园区的`gapMaterial`都是`Pavement`：园区里建筑quad之外的区域铺人行道地面。
- **内部布局细节**：两栋内部建筑沿内部道路两侧各占一半，沿路方向长`0.6*side`、垂直方向深`0.6*(side/2)`，不贴围墙/内部道路；`roadIndices`记它贴着`internalRoads[0]`的哪一面，两半相反。
- **急救包装饰**(`ZONE_KIT_*`)：大门开口两端各一个，离北边`ZONE_KIT_INSET`。

## 依赖关系

- 依赖：`map/zone_mod.h`、`common/utility.h`(`GetRandom`)、`Lot`/`Quad`/`PlacementEmitFunc`(`general_geometry.h`)。
- 被谁依赖：`basic.cpp`注册(三个园区分别依赖`Residence{High,Middle,Low}Building`)；`Core/map/zone.h`消费；各`config*.json`的`zone_mods`列出这三个id。

## 待办/后续阶段

- `ZONE_*`的网格路径和数值是测试配置，看过效果再调。
