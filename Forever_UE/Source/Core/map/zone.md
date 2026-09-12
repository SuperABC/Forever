# zone.h / zone.cpp（`building.h`/`.cpp`同构，见文末）

## 职责

`Zone`是"已经落地的一个具体Zone实例"的Core包装类，模式照抄`terrain.h`：持有一个`ZoneMod*`
（由`ZoneFactory`创建/销毁），转发`GetType`/`GetName`。和`Terrain`不同的是**继承`Quad`**——
`Zone`占据的矩形直接就是它自己（`SetPosition`/`GetPosX`等都是`Quad`现成的方法），不额外
持有一个`footprint`字段。

`Zone`这次**不实现**老工程"Zone内部再持有一批`Building`"的递归布局——只是一个
类型+矩形的占位对象，落地时`Map::InitZones()`会调用继承来的`SetPosition`把它摆到
`Lot::RequestPlacement`裁剪出来的位置上，然后就结束了，见`Source/Core/map/map.md`
"InitZones"一节。

## 关键设计

- **"扫描用"实例和"落地用"实例是两个不同的`ZoneMod`对象**——`Map::InitZones()`先给每个
  注册的zone类型建一个临时实例调用`Distribute(lots)`（这个实例可能带`id`/`name`这类只应该
  在真正创建一个实例时才递增的状态，见`ZoneBasic`），处理完显式占位请求后销毁；每一条成功
  的占位请求再单独`new`一个`Zone`（也就是新建一个全新的`ZoneMod`实例）表示"这一个真正落地
  的Zone"。两者不能共用同一个mod实例，否则"扫描"和"落地"会互相污染各自的状态。
- **`parentLot`只是一个方便查询的反向引用**，`Zone`不负责这个指针的生命周期（`Lot`本身也
  不知道有哪些`Zone`落在自己身上——这次没有像老工程`Block::AddZone`那样维护双向映射，单向
  引用已经够用，`Map`直接拿着`vector<Zone*>`用）。
- **`GetRotation()`直接转发`parentLot->GetRotation()`，`Zone`/`Building`自己不存这个字段**
  （第十三轮迁移，简化了一版早前的实现）——`Quad`本身没有旋转，最初照抄`Lot`"在`Quad`基础上
  自己加一个旋转角度"的做法给`Zone`/`Building`也单独存了一份`rotation`（PIE验证发现斜向道路
  旁边裁出来的Zone/Building如果不带旋转，扁cube会显示成轴对齐、和实际地块朝向对不上，需要
  旋转数据本身没有错），但既然`freeLots`池里所有子块都继承同一个顶层`Lot`的`rotation`
  （`Lot::SplitWithPath`产出的两段都用同一个`this->rotation`构造），而`Zone`/`Building`本来
  就已经通过`parentLot`拿着这个顶层`Lot*`（见下），再自己存一份`rotation`纯粹是冗余拷贝——
  `SetParentLot(lot)`之后`GetRotation()`直接转发`parentLot->GetRotation()`就是同一个值，不用
  额外的`SetRotation`调用，`Map::InitZones()`/`InitBuildings()`落地时也就不需要再单独调一次
  `SetRotation`了。唯一的约束是`GetRotation()`必须在`SetParentLot`之后调用才有意义，构造完/
  `SetParentLot`之前调用返回0（`parentLot`还是空指针）。

## 依赖关系

- 依赖：`map/zone_mod.h`、`map/zone_factory.h`、`map/geometry.h`（`Quad`/`Lot`）。
- 被谁依赖：`Source/Core/map/map.h`（`Map::zones`成员，`InitZones()`产出）、
  `Source/Forever/Framework/ForeverZoneFrameworkComponent.h/.cpp`
  （`GenerateZones(Map*)`读`Map::GetZones()`画扁cube）。

## 待办/后续阶段

- 阶段4：Zone内部再对自己剩余的矩形跑一次Building分配流程（用户明确推迟，等设计好细节
  再补，届时`Zone`大概率需要新增一个`vector<Building*>`成员）。

---

# building.h / building.cpp

和`Zone`结构完全一样（同样继承`Quad`，同样"扫描实例"/"落地实例"分离），唯一区别：落地那
一刻要调用`mod->RandomAcreage()`采样一次面积（`RandomAcreage()`每次调用都是新的随机数，只
应该在落地那一刻调一次，不要在别处重复调），面积最终体现在`SetVertices`/`SetPosition`定出
来的矩形尺寸上，不需要单独存一份。`Map::InitBuildings()`落地流程（含显式占位+权重CDF两条
路径）见`Source/Core/map/map.md`。
