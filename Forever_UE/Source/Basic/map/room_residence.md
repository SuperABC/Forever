# room_residence.h / room_residence.cpp

## 职责

`ResidenceRoom`(`room_residence`)：Room域默认内容：各类型的属性和设计见room_*.md。

Room域的默认实现。`SetProperty()`里按类型声明房间的功能属性(`isResidence`/`isWorkspace`/`isStorage`/`isManufacture`及对应容量、房租、门规格)，没声明的属性保持`RoomMod`基类默认值。

## 关键设计

- **`ResidenceRoom`**：每间住宅room=1个名额，多人合住靠`Map::Checkin()`的名额池逻辑，不靠这个capacity强制。
- **`ResidenceRoom`登记`doorSpecs["home"]`**(读卡器门，网格/动画蓝图路径直接写在`SetProperty()`里)：高密度住宅四种布局(wing/side/twin/nshape)的所有 single/row 门洞标签统一是`home`；corridor 的门洞没有标签，仍是缺口。其余房间类型仍用 `doorSpecs["room"]`。
- 带`isWorkspace`的房间(商店/仓库/工坊)，Organization按容纳量一半定员。
- 三种工坊房间由`IndustryBuilding`等概率随机选一种；生产类型是`ManufactureMod`的id，Industry按它为房间生成`Manufacture`。

## 家具布局(`ResidenceRoom::Layout`)

所有住宅都是这一个类型，靠**规模分档 + 随机**做出不同户型。数据来自`RoomLayoutInfo`(房间宽/深/层高、门窗洞)，结果经`AddFurniture`登记，规则见`Dependence/map/room_mod.md`。家具网格和尺寸表：`/Game/Asset/Room/Mesh/<名字>`，尺寸来自生成器导出的`room_furniture_catalog.h`(`kRoomFurnitureCatalog`，名字/进深/宽/高，米，**由`Tools/Furniture/make_furniture.py --header`生成，不要手改**)。

- **整体放大 `ROOM_FURNITURE_SCALE`(1.5)**：游戏多是头顶第三视角，家具要比真实尺寸大才不显得挤。做法是`Layout()`里把房间尺寸/门窗洞按1/S缩小后用原尺寸家具和间距布局，结果再放大S倍(位置、z、`FurnitureSpec`缩放都乘S，网格资产本身不变)，所以家具、走道、门前清空区、吊柜高度同比放大，疏密不变。**下面所有分档/长度数字都是缩小后的空间里的值**(真实尺寸要乘S)：比如"<40㎡"对应真实<90㎡，真实边<3.75m或<18㎡不装；小户型(真实<60㎡)会缺厨房/床，大户型卧室数也相应变少。要调松紧改这个宏即可。
- **中心留空**：家具中心不得落进房间中央的"留空区"(每边内缩 `clamp(短边*ROOM_CORE_INSET_RATIO, ROOM_CORE_INSET_MIN, ROOM_CORE_INSET_MAX)`，缩小后的空间里)，所以家具都沿四周摆；地毯、吊柜和茶几/餐椅/书桌椅这类必须挨着主件的配套件(`TryPlace`的`accessory`)除外。餐桌长边贴一面真墙，墙和桌子之间留一圈椅子位置。电视柜对面墙放不下就退回侧边。
- **朝向**：局部yaw 0度=家具正面朝向+v，`Room::LocalYawToWorld`把它换算成世界yaw(`FacingYawDegrees`必须和`LocalToBuildingLocal`的v轴方向一致，CoreTest save套件`CheckRoomYawConvention`校验；曾经W/E写反导致W/E朝向房间的家具前后全反)。家具网格约定背面在-X、正面在+X(床头板/椅背在-X)，导入后核对过。
- **户型分档**(按面积)：卧室数 <40㎡=0(单间，床放客厅区)、<85=1、<150=2、<230=3、其余4；卫生间 <150㎡=1，否则2；>=150㎡加书房；<8㎡或某边<2.5m不装。
- **分带**：进深>=7m且宽>=6m用"双带"(靠门一带=公共区客厅+厨餐，另一带=卧室/卫生间/书房，打乱顺序)，否则单带沿长轴排(公共区靠门)。每个带按`ZoneWeight`/`ZoneMin`切成功能区(最小长度允许压缩到`ROOM_ZONE_SHRINK`才丢区，保住小户型的厨卫)。
- **功能区**：客厅(沙发+地毯+茶几+电视柜、玄关鞋柜在门边、绿植/落地灯/书架/扶手椅按概率)、厨房(靠墙一排冰箱/台面/水槽/灶台+吊柜，可再来对面一排，剩余空间摆餐桌椅)、卧室(床靠墙+床头柜+床尾地毯+衣柜，可选书桌椅/梳妆台/绿植)、卫生间(马桶/洗手台优先，其次浴缸或淋浴、洗衣机)、书房(书桌椅+书架)。
- **靠墙放法**：`WallPlace`在若干条边、每条边多个沿墙位置依次试，窗/门/已摆家具挡住就换位置换边；卫浴/衣柜/书桌/梳妆台/床在双带模式下只贴**真墙**(分区之间没有隔墙，贴虚拟边会悬在房间中间，宁可不放)，单带/单间才退一步用虚拟边。`TryPlace`统一检查：在区域内、不压已摆家具、不压门前清空区(`ROOM_DOOR_CLEAR_*`)、高过窗台(`ROOM_WINDOW_SILL_TOL`以上)的不贴窗(`ROOM_WINDOW_CLEAR`)；吊柜只查边界、不悬在门洞上方(`doorZones`)、吊柜之间不重叠(`overheadOccupied`，厨房吊柜比灶台宽，撞上就沿墙错开0.25/0.5m再试，还不行就丢)，地毯只查边界。门前清空区`ROOM_DOOR_CLEAR_DEPTH`1.6/`_SIDE`0.6(缩小后空间，真实2.4m/0.9m，给开门扫过的范围留够)。
- **网格必须居中**：布局把家具当"原点居中的矩形"，`make_furniture.py`的`build_catalog`末尾对每件`recenter()`(X/Y包围盒居中)，否则不对称的件(马桶水箱在背面、水龙头在台面一侧)靠墙时会有一部分穿出墙外。
- **随机性**：整体镜像(门窗镜像后布局，再把结果镜回来，保证和真实门窗一致)、功能区顺序、每类家具的款式变体(`Variant`)、沿墙位置、可选件概率。全走`GetRandom`(mod流，`--seed`可复现)。摆不下就丢件，不报错。
- **可调项**：文件顶部的`#define`(`ROOM_MARGIN`/`ROOM_DOOR_CLEAR_*`/`ROOM_WINDOW_*`/`ROOM_WALL_CABINET_Z`/`ROOM_TWO_BAND_*`/`ROOM_ZONE_SHRINK`)，以及`ZoneWeight`/`ZoneMin`和各区概率。
- **验证**：`Source/Test/Core/suite_mods.cpp`的`CheckRoomLayouts`(12种尺寸×8次)断言：在房间内、非吊柜/地毯互不重叠、吊柜之间互不重叠、不堵门、yaw只有0/±90/180、>=20㎡至少5件。环境变量`FURNITURE_DUMP=<路径>.json`时把`room_residence`的布局导出，用`Tools/Furniture/make_preview_data.py`生成俯视预览页看。
- **局限**：没有隔墙(开放式摆放)；家具无碰撞；市民不感知家具；预览页只是俯视方块，真实观感要进游戏看。

## 依赖关系

- 依赖：`Source/Dependence/map/room_mod.h`(`RoomMod`基类)、`room_furniture_catalog.h`(生成的尺寸表)、`common/random.h`。

## 待办/后续阶段

- 无。
