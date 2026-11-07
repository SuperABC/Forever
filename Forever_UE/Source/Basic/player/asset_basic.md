# asset_basic.h / asset_basic.cpp

## 职责

物品/背包系统的默认资产，共14个`AssetMod`：
- `ContainerAsset`(`asset_container`)：背包容器，`backpack=true`、`volume=50`，能背在后背。
- `WheatAsset`/`BeefAsset`/`BurgerAsset`(`asset_wheat`/`asset_beef`/`asset_burger`)：食物，`Containee`。
- `PistolAmmoAsset`/`RifleAmmoAsset`/`ShotgunAmmoAsset`/`SniperAmmoAsset`(`asset_ammo_*`)：子弹，没有对应`Product`。
- `PistolWeaponAsset`/`RifleWeaponAsset`/`ShotgunWeaponAsset`/`SniperWeaponAsset`(`asset_pistol`等)：武器资产，`weapon=true`。
- `GrenadeAsset`/`SmokeGrenadeAsset`(`asset_grenade`/`asset_smoke_grenade`)：投掷物，`throwable=true`。

## 关键设计

- 食物资产的id前缀虽然不同(`asset_`)，但`productType`指向`Industry`的`product_*`(见`industry/product_basic.md`)：`Asset`只存这个type字符串，不持有`Product`实例，见`Core/player/asset.md`。只有`BurgerAsset`可使用(`usable=true`、`satiety=0.5`，饱食度上限1、超出截断；用一次`count`-1，到0销毁)，`productType="product_burger"`让商店按零售价卖。
- 武器资产的`weaponType`指向`WeaponMod` id(`weapon_pistol`等，见`weapon_basic.md`)：在肩膀上激活时装备对应武器(激活只走肩膀；`weapon=true`的资产左右手拒绝)，桥接机制见`Core/player/player.h`的`ActivateShoulderWeapon`。武器资产的`milestoneNames={"asset_test"}`，对应`Resource/Story/asset_test.script`的测试脚本(捡起/测试获得/测试扣除/测试丢弃/测试扣除过量选项，`get_asset`/`lose_asset`里程碑`debug_print`)。
- 备弹资产重量/体积很小(0.01~0.02)；武器资产重量3~4.5、体积8~10；`WeaponMod::ammoType`引用备弹资产id。
- 投掷物是`Object`机动性：能拿在手上、放进容器，不能挂肩膀。网格用`/Game/Asset/Meshes/Sphere`(直径100cm)，`meshScale=0.1`缩到10cm。
- `GetName()`必须全局唯一：`GetType()`+id现拼，计数器存档见`Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`player/asset_mod.h`。
- 被谁依赖：`basic.cpp`注册(依赖表里武器资产->武器->备弹资产)；`Core/player/asset.h`消费。

## 待办/后续阶段

- 投掷物落地后的效果(爆炸/烟雾)没做，现在只是被扔出去后消失，见`Core/player/asset.md`。
