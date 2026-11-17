# ForeverDamage.h / ForeverDamage.cpp

## 职责

`FForeverDamage`：`single_damage`/`area_damage`两个Change的UE层执行体(静态函数，没有状态)。伤害数值、距离衰减、部位倍率都由**脚本**算好传进来，
这里只做"扣血 + 致死则广播击杀事件并处理死亡"，以及范围伤害的圈选和遮挡判断。Core里没有枪/伤害的专用逻辑，`Citizen::TakeDamage`只管扣血。

## 关键设计

- `DamageCitizen(world, target, damage, source, weapon, part)`：目标是场景里的`ACitizenElement`(已死亡/没有`Citizen`什么都不做)。致死时构造`CitizenDeceaseEvent`
  (reason固定`"player"`，凶手=`source`姓名、武器种类=`weapon`、部位=`part`(空按`body`)、致死伤害、凶手是否可见、死亡地址)交给`BroadcastCitizenDecease`，再`HandleCitizenDeath`。
  这段死亡处理原样来自已删除的`UForeverBulletSubsystem::ApplyCitizenHit`。凶手是否可见：死者和凶手(按`source`姓名找到的、场景里有Actor的市民)中心点连线没被`ECC_Visibility`射线挡住；凶手没实例化=false。
- `DamageArea(world, center, radius, damage, occlusion, power, source, weapon)`：用`OverlapMultiByObjectType(ECC_Pawn)`球查询圈出范围内的`ACitizenElement`(先收成弱指针再结算——致死处理会改场景)，
  逐个算伤害 `damage * (1 - (距离/半径)^power)`，距离≥半径跳过；`occlusion`为真时从中心抬高30cm到市民中心一条`ECC_Visibility`线，被挡住就跳过(目标自己忽略)。**只处理市民**：范围内的玩家当前操控的市民也是`ACitizenElement`，
  会被炸到；`Player`自己的血量(`Player::TakeDamage`)不在这里处理。载具伤害以后另加`DamageVehicle`。
- `ClassifyPart`：命中点在胶囊体上的相对高度，顶部20%=head、40%以下=legs、其余=body(从旧`ApplyCitizenHit`搬来)。`ParseVec3`：解析脚本里`"(x, y, z)"`字符串，`spawn_effect`/`area_damage`的`position`/`normal`共用。
- 为什么用`ECC_Visibility`做遮挡而命中判定用`ECC_Pawn`：命中射线要打到市民胶囊体(Pawn预设忽略Visibility，用Visibility会穿过去)；遮挡射线找的是墙/地形，市民本来就不该挡，用Visibility正好。

## 依赖关系

- 依赖：`Element/CitizenElement`、`Framework/ForeverFrameworkActor`/`ForeverStoryFrameworkComponent`/`ForeverPopulaceFrameworkComponent`、`story/event.h`、`populace/*`。
- 被谁依赖：`UForeverAssetFrameworkComponent::ApplySingleDamage/ApplyAreaDamage/ApplySpawnEffect`、`UForeverWeaponComponent`(`ClassifyPart`)。

## 待办/后续阶段

- "受到伤害"事件(伤害结算后给受害者/攻击者的脚本发事件)还没做；做了之后`single_damage`/`area_damage`的击杀信息字段可能调整。
- 载具受到伤害(`DamageVehicle`、`single_damage`的`target`支持载具)。
- `area_damage`的遮挡只测中心到中心一条线，不做部分遮挡比例；没有`ignore`字段(手雷会炸到自己)。
