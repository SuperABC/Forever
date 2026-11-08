# household.h / household.cpp

## 职责
户：住在同一个房间里、有夫妻或父母子女关系的一组人。同房间没有亲属关系的人算不同户。
户**直接替代**原来的`Room::tenants`：Room保存里面住的户，户保存成员。

## 关键设计
- 成员入住时由`Room::AddResident`归户：与本房间某户的任一成员有夫妻/父母子女关系就加入那一户；
  同时与多户相连则把这些户合并；都没有关系就新建一户。亲属关系可以经第三人间接相连（比如两个成年人
  的孩子先后入住）。
- 目前不涉及户内人员变动（出生、死亡、迁入迁出）：成员不会被移出，离城和死亡的人仍留在`members`里，`GetAdults`只返回在场（`IsActive`）的成年人，经济结算（房租、饭钱、补助）因此自动跳过他们；整户离城见`society/emigration.md`。
- `Citizen::GetHousehold()`反查所属户。
- 存档：户的成员写在人口段里（和原来的tenants一样），Room持有户，读档时按存档重建。
- 房租按房间里的户数均分，每户再由成年成员均摊，见`Settlement::PayRents`。

## 依赖关系
- 依赖：`Citizen`（亲属关系、年龄）。
- 被谁依赖：`Room`（持有）、`Map::SettleCitizen`、`PopulaceSaveCodec`、`Settlement`。
