# weapon_pistol.h / weapon_pistol.cpp

## 职责

`PistolWeapon`(`weapon_pistol`)：weapon域的默认实现之一。

## 关键设计

- 字段含义见`Dependence/player/weapon_mod.md`。四把枪的数值差异：

| 武器 | 伤害 | 射速(秒/发) | 全自动 | 射程(cm) | 散射(度) | 弹匣 | 换弹(秒) |
|---|---|---|---|---|---|---|---|
| 手枪 | 20 | 0.2 | 否 | 5000 | 0 | 8 | 2 |
| 步枪 | 40 | 0.1 | 是 | 10000 | 0 | 30 | 1.5 |
| 霰弹枪 | 90 | 0.9 | 否 | 2500 | 2.5 | 6 | 2.5 |
| 狙击枪 | 150 | 1.5 | 否 | 30000 | 0 | 5 | 3 |

- **霰弹枪**一次开火只有一条射线：`WeaponMod`还没有"一次射出多颗弹丸"的字段，用单发高伤害+小散射近似。

- **后坐力**逐枪调：手枪轻(俯仰1.2~2、左右+-1.2)，步枪连发时准心明显上爬(0.8~1.5、+-0.8)，霰弹枪单发大(3~4.5、+-1.5)，狙击枪单发很大、左右漂得少(4~6、+-0.8)，仿PUBG手感，具体数值留给PIE里试。

- **模型**用`Weapons_Free`资源包的Actor Blueprint(`firstPersonMeshPath`以`_C`结尾)；资源包的模型比真实尺寸大得多，用`modelScale`缩放(手枪0.25、步枪/霰弹枪0.4、狙击枪0.6)。`attachOffset*`/`backOffset*`/`muzzleOffset*`是在Core基础位置上的微调；霰弹枪和狙击枪的3D变换先沿用步枪的数值，后面按模型实际大小再调。

- **`ammoType`**对应`Basic/player`里的备弹资产(`asset_ammo_pistol`/`rifle`/`shotgun`/`sniper`)，换弹时从背包按这个type扣减。

## 依赖关系

- 依赖：`player/weapon_mod.h`。
- 被谁依赖：`basic.cpp`注册；`Core/player/weapon.h`消费；`Source/Forever/Player/ForeverWeaponComponent`读数值。

## 待办/后续阶段

- 弹道/后坐力/模型变换数值留给PIE里调；霰弹枪的多弹丸机制没做。
