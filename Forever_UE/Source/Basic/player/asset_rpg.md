# asset_rpg.h / asset_rpg.cpp

## 职责

`RpgWeaponAsset`(`asset_rpg`)：火箭筒的背包资产。`weapon=true`、`weaponType="weapon_rpg"`，在肩膀上激活时装备`weapon_rpg`(桥接机制见`Core/player/player.h`的`ActivateShoulderWeapon`)。

## 关键设计

- `mobility=Object`、重量7、体积12(比狙击枪更重更大)；`milestoneNames={"asset_test"}`(同其它武器资产)。
- 模型来自`WeaponMod`(`weapon_rpg`)，资产本身没有`meshPath`。
- `GetName()`必须全局唯一：`GetType()`+id现拼，计数器存档见`Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`player/asset_mod.h`。
- 被谁依赖：`basic.cpp`注册(依赖表里武器资产->武器)；`ForeverCharacter::PossessedBy`开局放进背包。
