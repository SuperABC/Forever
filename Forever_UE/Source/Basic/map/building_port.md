# building_port.h / building_port.cpp

## 职责

`PortBuilding`(`building_port`)：港口，building域的默认实现之一。**只靠路网预设落地**(`Resource/Mod/Basic/test.roadnet`的`output.presets`里`id:"building_port"`，`Map::PlacePresets`→`InitBuildings`)，`Assign()`是空的，不自己选地块；`GetPower`恒0不参与权重分布。目的只是把船放出来，建筑本身不重要。

## 关键设计

- 地下1层 + 地上1层，楼体占满整个Building区域(默认`footprint`)。**地下1层**(`basements=1`，`floorHeights[0]=2.0`即20米)模板`preset_port_bg`，layout里只有一圈墙(corridor)，**不是房间**(不`AssignRoom`、不分船位)，作用是让墙插到水面以下、一楼不会浮在水面上。**地上1层**模板`preset_port_fg`(`Resource/Layouts/preset_port_fg.layout`)：4个小房(layout的singles 0~3，全部`room_empty`+`component_port`，个数写在`.cpp`的`kSingleCount`，改layout的single数要同步改)，远离路的一侧(水面上)8个船位矩形(layout的`parkings` 0~7)，面积随预设矩形变，船位个数写在`.cpp`的`kBoatSlotCount`，改layout的船位数要同步改它。
- 对 parkings 0~7 各调一次`AssignParking(0, 0, spec)`(`spec`：`spacing=-1`、`categories={"boat"}`、`stocked=true`、`offRoad=true`)：每个矩形一个船位(`-1`)，`offRoad`=船位在水里、不接公路(layout里没有车行连接，车位节点不在车行图里)，`stocked`=开局由`Traffic::InitStockedVehicles`各停一艘`boat`类载具(`vehicle_boat`)，共8艘；船位都归入小房。船没有主人，不走`AssignVehicles`。
- 船位开口朝路(layout `direction 2`)，所以车头(船头)朝向远离路的一侧(海)。
- 没有`stationMod`(没有公交船/航线，暂时不建站点)。
- `config.json`的`building_mods`里要有`"building_port"`才启用；没启用时预设只占位并打警告。

## 依赖关系

- 依赖：`Source/Dependence/map/building_mod.h`；`component_port`、`room_empty`(`basic.cpp`依赖表声明)；`vehicle_boat`(船位的类别`boat`，运行时按类别查，不在依赖表里)。
- 布局坐标(占位值，按真实船在布局编辑器里调)：模板按"路在北边、y=0靠路、y向远离路增大"画，坐标=比例×边长+地图单位偏移；小房 y 0.3~2.3、x居中2单位宽；船位矩形 y 9~12.5、x居中4单位宽，`direction 2`(开口朝路，船头朝海)；只给小房铺地面/天花板。(2026-11-23 用户已把layout改成8个船位+整面地面，坐标以layout文件为准，上面只是最初的占位说明。)layout文件里可以写`//`和`/* */`注释(布局编辑器`stripJsonComments`读取时去掉，2026-11-23)，但**编辑器保存时不会保留注释**，所以注释要写在这份md里。
- 被谁依赖：`basic.cpp`注册；`Resource/Layouts/preset_port_fg.layout`。
