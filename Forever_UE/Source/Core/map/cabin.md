# cabin.h / cabin.cpp

## 职责

一台电梯（轿厢）。每条 `ElevatorCabinSpec`（`BuildingMod::AssignElevatorCabin`）对应一台。Cabin 持有：
- 自己的 Script；
- 交互选项；
- 当前所在楼层；
- 行程状态。

状态全部在 Core，UE 层只按它摆放轿厢、推进行程，所以 LOD 重建、远景、存档都不会丢状态。

## 关键设计

- **楼层**：用人类直觉层号，0 是 1 楼，负数是地下。高度都是地图单位，不含 UE 的 `BUILDING_HEIGHT_EPSILON`。
- **初始楼层**：1 楼，夹到这台电梯的范围内。比如只负责 3~5 楼的电梯，初始停在 3 楼。
- **交互名**：`"<building名> 电梯<n>"`，n 按这栋楼已建的电梯数加 1，全局唯一。
  - `Map::FindCabinByName` 按交互名查电梯。
  - `add_option`、`remove_option`、`launch_elevator`、`option_dialog` 都按交互名找到电梯。
- **Script**：spec 里 `milestoneNames` 非空时才创建，和 `DoorSpec` 同一个约定。创建时写入这些脚本变量：
  - `self.name`：交互名；
  - `self.building`：建筑名；
  - `self.min`/`self.max`：层号范围。
- **一口井多台电梯**：
  - 同一个 `shaftIndex` 可以声明多台电梯，各管一段楼层，比如 1~2 楼一台、3~5 楼一台。
  - 楼层范围和已建电梯重叠的，后一条在 `Building::BuildCabins` 里跳过并警告。
- **行程**：
  - `Launch(level)`：记下出发高度 `fromZ`（当前高度）、目标楼层，从 0 开始计时。运行中再次 Launch 就是改目标，从当前高度重新出发；已经停在这一层时什么都不做。
  - UE 推进行程：`ABuildingElement::UpdateCabins` 每帧调用 `Advance` 累加时间，用"匀速 + 两端缓入缓出"的曲线（`ComputeTripZ`）算出当前高度并写回 `SetCurrentZ`，到站时调 `Arrive` 更新 `currentLevel`。
  - 这一步不管 LOD 都会执行，所以远景下电梯照样会到站。
  - 速度和缓动时长用 `UForeverBuildingFrameworkComponent` 的 `cabinCruiseSpeed`/`cabinEaseSeconds`。
- **持有关系**：电梯归 `Building::cabinEntities`，Building 析构时释放；`Map::cabinsByInteractName` 只是索引。
- **创建入口**：`Building::BuildCabins(Map&)` 逐条调用 `Map::CreateCabin`。新开局时由 `Map::InitBuildings` 在 `BuildDoors` 之后调用；读档时由 `MapSaveCodec` 直接按存档重建。

## 交互

- **选项**：脚本在 game_start 里给自己加"前往x层"选项，见 `Resource/Story/elevator_basic.script`。地下用 B，地上从 1 楼起编。
- **点击选项**：触发 `option_dialog`，执行 `launch_elevator`，`command` 填 `$$local.option`，即被点的选项文字。
- **command 的解析规则**（`Map::ApplyChange`）：`B`/`b` 后面跟数字 n 表示 −n（地下 n 层）；否则取第一个数字 n，表示 n−1（n 楼）；超出范围就夹到范围内。
- **触发盒**：UE 层（`ABuildingElement`）在两处放触发盒，只在近处 LOD 存在：
  - 每层电梯正面开口外（`Elevator::GetDirection()` 那一侧）：宽为开口宽×0.8、高为层高×0.8（`kElevatorMarkerScale`，居中），深 1 米（`kElevatorFrontDepth`），贴着开口平面；
  - 轿厢内：宽和高取轿厢的 0.8（`kElevatorInnerScale`，底贴轿厢地板），沿开口方向深度取轿厢进深的 0.5（`kElevatorInnerDepthScale`）、贴着轿厢门平面（和门口触发盒之间不留缝），玩家走到轿厢最里面就不弹选项；跟着轿厢移动。
  - 只认玩家本人。门口和轿厢内可能同时压到，按计数只在第一个进入时弹出选项、全部离开才收起。
  - 选项运行中变化时，`AForeverFrameworkActor::RefreshInteractOptions` 按交互名找到电梯所在楼的 `ABuildingElement`（`FindForBuilding`），调用 `RefreshCabinOptions` 重新烘焙；玩家正压着触发盒时整组换掉。

## 存档

- `world.map.structures` 里每栋楼的电梯都会写：所在楼层、当前高度、行程状态和交互选项。交互选项只在存档带脚本状态段时恢复，和门一样。
- 电梯 Script 的状态由 `script_save` 存，键是 `cabin:<交互名>`。

## 依赖关系

- 依赖：`story/script`（Script）、`common/config`（`GetScriptPath`）。
- 被谁依赖：
  - `map/building`（持有）、`map/map`（创建/索引/ApplyChange）、`map/map_save`；
  - `story/script_save`；
  - `Forever/Element/BuildingElement`（摆放、推进行程、触发盒）；
  - `Forever/Framework/ForeverFrameworkActor`（`RefreshInteractOptions`）；
  - `Forever/Framework/ForeverStoryFrameworkComponent`（广播和选项点击）。

## 待办 / 后续

- 轿厢网格的碰撞：玩家要能站在轿厢上跟着移动，需要在 PIE 里确认。
