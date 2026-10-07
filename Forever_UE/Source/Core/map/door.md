# door.h / .cpp

## 职责

`Door`：一扇真正放了门扇的门的运行期实体——只有`DoorSpec.mesh`非空才会被
`Map::CreateDoor`创建，纯门洞（没标签/查不到spec/mesh为空）永远不会有对应的`Door`
实例。外观/开关方式全部来自创建时传入的`DoorSpec`快照，门本身没有需要多态的行为，
所以不做`DoorMod`/`DoorFactory`这套mod机制（和`BuildingMod::Layout()`/
`UForeverWeaponComponent`当年的取舍同一个理由）。

`ResolveDoorSpec`：建筑门/房间门的放置判定——按tag查`doorSpecs`表，查不到/tag为空/
mesh为空都返回nullptr（只是门洞）。园区门不走这个函数，直接在`Zone::BuildDoors`里判断
`gate.door.mesh`是否为空。

## 关键设计

- **id/交互名/随机朝向都由`Map::CreateDoor`算好再传进构造函数，`Door`自己不重新计算**
  ——这三者都依赖`Map`自己维护的状态（全局门计数器、按"所属对象+spec.name"分组的序号
  计数器、随机数生成），`Door`构造函数只是单纯持有这些已经算好的值，职责边界清楚：
  `Map`管"这是第几扇门/叫什么/随不随机"，`Door`管"这扇门的状态和行为"。
- **Script创建照抄`Vehicle::Vehicle`的模式，但多设一个`"label"`**：`interactName`非空
  才会创建Script（`spec.name`为空的纯自动门恒为`nullptr`），创建后`SetValue("name",
  interactName)`+`SetValue("label", spec.name)`——`Vehicle`当年只设了`"name"`，这次
  多加`"label"`是因为门需要区分"交互名"（完整的、带编号的唯一标识，剧情脚本点名/
  MeetOption显示用）和"门类别标签"（`spec.name`本身，比如"商店大门"，不带编号），这是
  Door专属的新增需求，不是Vehicle当年漏做。
- **`Allow()`按名字字符串存，不按`Citizen*`指针存**——`Map::ApplyChange`处理
  `AllowDoorChange`时手上只有一个`citizen`名字字符串，`Map`不持有`Populace`的引用/
  指针（`Populace::Init`只是`Map::Checkin(const Populace&)`的一次性参数，不长期持有），
  没有办法把名字反查成真正的`Citizen*`。所以`Door::Allow(const std::string&
  citizenName)`存名字，`CanPass(const Citizen* who, ...)`里用`who->GetName()`和存的
  名字比对，不是指针比对——`who`本身是调用方（Forever层`UForeverDoorComponent`，手上有
  `ACitizenElement::GetCitizen()`拿到的真实指针）传进来的真指针，只是"允许名单"这一份
  数据存的是字符串。
- **`CanPass`的Owner分支判定顺序**：`stated==true`（所属对象公有）→ true；
  `isPlayer==true`→ true（这次没有真正的玩家门禁判定，占位按Open处理）；
  `who==nullptr`（调用方判断不出身份，比如空车/公交）→ false；
  `owner==who`（指针相等，真正的房主/楼主/园区主）→ true；否则查`Allow()`登记过的
  名单。`Locked`/`Open`两种状态在最前面直接短路，不走这一整套判定。
- **`DOOR_ACCESS_TYPE`数值顺序和`DoorSpec::access`的int字面值(0/1/2)保持一致**——
  `Door`构造函数直接`static_cast<DOOR_ACCESS_TYPE>(spec.access)`，不需要额外的
  字符串/枚举转换表；`Map::ApplyChange`处理`SetDoorAccessChange`时才需要把脚本里的
  字符串("open"/"owner"/"locked")转成这个枚举，转换逻辑放在`map.cpp`里（就近原则，
  这是唯一需要字符串转枚举的地方）。
- **`IsFlippedSide()`只在`leaves==1 && spec.randomSide`为true时才有意义**——双扇门
  两扇对称镜像，没有"选哪一边"的歧义，恒为`false`；`spec.randomSide`为`false`的单扇门
  也恒为`false`（没开随机化开关），Forever层`UForeverDoorComponent`按这个值决定具体
  渲染/动画细节，见`Forever/Element/DoorComponent.md`。

## 依赖关系

- 依赖：`Source/Dependence/map/door_spec.h`（`DoorSpec`纯数据）、
  `Source/Core/map/zone.h`/`building.h`/`room.h`（`GetOwner`/`GetStated`）、
  `Source/Core/populace/citizen.h`（`CanPass`里的`Citizen::GetName()`）、
  `Source/Core/story/script.h`/`script_factory.h`（Script创建）、
  `Source/Core/common/config.h`（`Config::GetScriptPath`）。
- 被谁依赖：`Source/Core/map/map.h`（`CreateDoor`/`FindDoorByName`/`GetAllDoors`/
  `ApplyChange`四个Change分支）、`Source/Core/map/zone.h`/`building.h`/`room.h`
  （各自的`doorEntities`持有`Door*`的生命周期）、
  `Source/Forever/Element/DoorComponent.h`（UE层读几何/外观/`IsFlippedSide()`驱动
  实际渲染和开关动画）。

## 待办/后续阶段

- `CanPass`的`isPlayer`分支这次恒按Open处理——没有真正的"玩家门禁"判定（比如玩家
  也应该能被`Allow()`单独授权/拒绝），等真的需要玩家专属门禁规则再回来改。
- `FindPedestrianPath`不按门禁绕路——`Locked`/`Owner`的门会挡住市民寻路，这次所有门
  默认`Open`，不会触发这个问题，按门禁过滤寻路留作后续工作，见`door_system_plan.md`
  风险清单。
