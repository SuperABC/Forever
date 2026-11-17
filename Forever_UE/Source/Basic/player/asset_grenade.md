# asset_grenade.h / asset_grenade.cpp

## 职责

`GrenadeAsset`(`asset_grenade`)：asset域的默认实现之一。

物品/背包系统的默认资产，共14个`AssetMod`：

## 关键设计

- 武器资产的`weaponType`指向`WeaponMod` id(`weapon_pistol`等，见`weapon_*.md`)：在肩膀上激活时装备对应武器(激活只走肩膀；`weapon=true`的资产左右手拒绝)，桥接机制见`Core/player/player.h`的`ActivateShoulderWeapon`。武器资产的`milestoneNames={"asset_test"}`，对应`Resource/Story/asset_test.script`的测试脚本(捡起/测试获得/测试扣除/测试丢弃/测试扣除过量选项，`get_asset`/`lose_asset`里程碑`debug_print`)。
- 备弹资产重量/体积很小(0.01~0.02)；武器资产重量3~4.5、体积8~10；`WeaponMod::ammoType`引用备弹资产id。
- 投掷物是`Object`机动性：能拿在手上、放进容器，不能挂肩膀。破片雷网格用`/Game/3rdParty/Pineapple_grenade/StaticMeshes/pineapple_grenade`，`meshScale=1`先按模型原始尺寸，大小待PIE里看了再调；烟雾弹是引擎球体小球，不共用。
- `GetName()`必须全局唯一：`GetType()`+id现拼，计数器存档见`Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`player/asset_mod.h`。
- 被谁依赖：`basic.cpp`注册(依赖表里武器资产->武器->备弹资产)；`Core/player/asset.h`消费。

## 待办/后续阶段

- 手雷`milestoneNames={"asset_grenade"}`：`Resource/Story/asset_grenade.script`，`throw_asset`→3现实秒引信(`create_timer` mode `real`)→`spawn_effect`(蓝图`/Test/Effect/BP_GrenadeExplosion`，没有就无特效并打Warning，`effect`留空同样无特效)+`destroy_asset`。手雷脚本对`/Test/...`是软依赖。
- 爆炸没有伤害/冲击力，只有视觉。
