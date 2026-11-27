# household.h / household.cpp

## 职责
户：住在同一个房间里、有夫妻或父母子女关系的一组人。同房间没有亲属关系的人算不同户。
住户关系只有这一层：Room保存里面住的户，户保存成员(`Room`没有单独的租客列表)。

## 关键设计
- 成员入住时由`Room::AddResident`归户：与本房间某户的任一成员有夫妻/父母子女关系就加入那一户；
  同时与多户相连则把这些户合并；都没有关系就新建一户。亲属关系可以经第三人间接相连（比如两个成年人
  的孩子先后入住）。
- 成员会被移出（2026-11-27）：搬家、自然死亡、离城都调`Room::RemoveResident`，把人从户里移出并`SetHousehold(nullptr)`，户空了就删掉、房子变回空房（`Map::PickVacantResidence`/迁入只认没有任何户的住宅）。剩下的人哪怕已经不再彼此连通（比如中间那个人搬走了）也不拆户。出生、婚后同居仍走`AddResident`归户，见`demography.md`。被打死的人仍留在`members`里（不走这套善后），`GetAdults`只返回在场（`IsActive`）的成年人，经济结算因此跳过他们。
- `Citizen::GetHousehold()`反查所属户。
- 存档：户的成员写在人口段里，Room持有户，读档时按存档重建。
- 房租按房间里的户数均分，每户再由成年成员均摊，见`Settlement::PayRents`。

## 依赖关系
- 依赖：`Citizen`（亲属关系、年龄）。
- 被谁依赖：`Room`（持有）、`Map::SettleCitizen`、`PopulaceSaveCodec`、`Settlement`。
