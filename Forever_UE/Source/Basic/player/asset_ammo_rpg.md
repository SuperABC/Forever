# asset_ammo_rpg.h / asset_ammo_rpg.cpp

## 职责

`RpgAmmoAsset`(`asset_ammo_rpg`)：火箭弹备弹资产，`WeaponMod::ammoType`引用它，换弹时从背包按这个type扣减(一次扣一个弹匣=1发)。

## 关键设计

- `mobility=Containee`；一发火箭弹比枪弹大得多：重量2、体积1(其它备弹是0.01~0.02)。开局背包里10发(`ForeverCharacter::PossessedBy`)。
- 这个资产只是背包里的"数量"，飞出去的火箭弹是脚本`spawn_actor`放出的独立蓝图Actor，不是这个资产的实例。
- `GetName()`必须全局唯一：`GetType()`+id现拼，计数器存档见`Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`player/asset_mod.h`。
- 被谁依赖：`basic.cpp`注册(依赖表里武器->备弹资产)。
