# job_mod.h / job_mod.cpp

## 职责

`JobMod`：一份具体工作类型(比如店员)的行为定义。"哪几天上班/几点上下班"直接写进具体子类的`DailyPlan`里，不再有独立的Calendar概念。

## 关键设计

- **`DailyPlan`/`ExecNode`调度完全是C++直接构造`Change*`，和milestone/JSON没有任何关系**：不做"按节点名找milestone触发"的合并，milestone只能通过它自己原本的Event广播+`Script::MatchEvent`触发器匹配流程被触发，两者运行时完全独立，见`Core/society/job.md`。
- **`plans`/`changes`是mod实例自己的成员字段**：`DailyPlan`/`ExecNode`是具体子类重写的虚方法，直接写自己的成员字段(分配/写入/析构全程在这个mod实例所在的同一侧，Core侧只读，不`delete`里面的`Change*`)，和`BuildingMod::floors/singles/rows`同一个跨DLL安全模式，见`Core/society/job.md`"跨DLL数据传递约束"。
- **`post`(`PostHandle`)**：向Core发起查询的句柄，用途是查occupant家/工位的具体房间地址(`Room::GetAddress()`格式)——`NPCNavigateChange::destination`不能写"home"/"workplace"这种描述性文本，必须是能被`Map::LocateRoom(address)`直接解析回`Room*`的具体地址字符串。
- **`occupantName`**：当前持有这份工作的市民姓名，由`Job`在调用`ExecNode`前同步进来；Dependence层看不到`Citizen`这个Core类型，只能传字符串。
- **Script配置**：`scriptModName`(默认空)+`milestoneNames`(不含路径/扩展名的bare文件名，Core通过`Config::GetScriptPath()`解析)，两者都空则实例没有`Script`；这两个字段和`DailyPlan`/`ExecNode`完全无关，纯粹给这个Job独占的`Script`提供milestone内容。不能用返回`vector<string>`的方法按值把mod分配的容器交给Core遍历(被明确禁止的跨DLL模式)。
- **`SetProperty()`两段式**：具体子类在这里填Script字段，不在构造函数里赋值。

## 依赖关系

- 依赖：`common/mod_state.h`、`common/utility.h`、`common/handle.h`、`Change`(前向声明)。
- 被谁依赖：`job_factory.h`创建它；`Core/society/job.h`消费它；`OrganizationMod::DesignJobsForRoom`决定配哪种Job；具体实现在`Source/Basic/society/job_basic.*`。

## 待办/后续阶段

- 无。
