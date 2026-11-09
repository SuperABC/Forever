# Yuanshen(building_yuanshen / script_yuanshen)

## 职责
`Yuanshen.dll`:一个BuildingMod(`YuanshenBuilding`,id `building_yuanshen`)加一个空壳ScriptMod(`YuanshenScript`,id `script_yuanshen`),给`Resource/ys.script`用。文件:`building_yuanshen.h`、`script_yuanshen.h`、`mod_yuanshen.cpp`(导出函数)。

## 关键设计
- **放置**:`Assign`只在井字路网中心lot(`AREA_OFFICIAL_HIGH`)里用`Lot::FindAdaptivePlacement`按剩余自由空间自适应找一块6x6贴路位置,4个方向谁先找到用谁,放不下就静默不放;不参与权重随机填充(`GetPower`恒0,面积min/max/random都是36)。命中的是自由子块(`targetLot`)而非lot本身时必须设`request.adaptiveParent`,Core才会把残余空间搬回lot(做法同`TrainStationBuilding::Assign`)。余量全挪到`marginStart`,贴到边的终点角,避免居中占掉整块空地。
- **`Layout`**:`direction<0`时从有真实边界路的方向里随机挑一个并回写(同`MoharBuilding`);楼体占满整个矩形(`footprint={0.5,0.5,1,1}`),无地下室,层高均0.4,墙材质用组件默认值。
- **层数14**由`ys.script`要放进来的30个角色决定:1楼4个(房间00001~00004),之后每层2个。改层数或房间数要同步改脚本里的`npc_teleport`目的地。
- 模板:1楼`yuanshen_fg+`(5个single+2个row),其余层`yuanshen_fu+-`(2个single+2个row),各有一口电梯井(`elevators`下标0),一部电梯`AssignElevatorCabin(0,0,layers-1,...,"elevator_basic")`从1楼跑到顶层。房间/组件全用`room_empty`/`component_empty`。
- 挂示例里程碑脚本`entity_event`(`milestoneNames`):进出这栋楼的事件只发给它,见`Forever_UE/Resource/Story/entity_event.script`。
- **`YuanshenScript`是空壳**:不override`WrapScript`(默认原样透传),`ys.script`里用`record_object`(kind=`building_yuanshen`,policy=first)拿到楼的地址,再用`npc_teleport`传送角色,不写C++剧情。它存在的唯一目的是让`script_yuanshen`作为主线mod被依赖到`building_yuanshen`。
- 命名:`GetName()`里现拼`yuanshen<id>`返回(`id(count++)`;`count`是头文件里的`static inline`,不在cpp里定义);`GetInstanceCounter`/`SetInstanceCounter`让存档保存/恢复这个计数器,见`Dependence/common/mod_state.md`。
- `GetModDependencies`:`building_yuanshen`依赖`room_empty`/`component_empty`/`script_empty`;`script_yuanshen`依赖`building_yuanshen`。creator忽略参数字符串。

## 依赖关系
- 依赖:`Dependence`的`building_mod`/`building_factory`/`script_mod`/`script_factory`(只链`Dependence.lib`)。
- 被依赖:`Resource/ys.script`、`config_ys.json`、`Forever_UE/Resource/Config/config.json`的`building_mods`/`script_mods`。

## 待办/后续阶段
- 楼内模板没有`pedestrianNavigation`/`vehicleNavigation`块,居民走不进楼,见`../../Resource/README.md`。
