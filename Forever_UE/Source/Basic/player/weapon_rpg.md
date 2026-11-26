# weapon_rpg.h / weapon_rpg.cpp

## 职责

`RpgWeapon`(`weapon_rpg`)：RPG-7火箭筒。单发(弹匣1、换弹3秒、不全自动)，**射线本身不造成任何伤害**，只为给脚本提供瞄准点；真正的火箭弹是开火时脚本放出的独立Actor。

## 关键设计

- 字段含义见`Dependence/player/weapon_mod.md`。`fireMode=Hitscan`、`pelletCount=1`、`maxRange=50000`(500米，瞄准点射线最远距离)、`baseSpread=0`、`fireRate=1`、`magazineCapacity=1`、`reloadDuration=3`、`ammoType="asset_ammo_rpg"`，后坐力很大(俯仰5~7、左右+-1)。
- **开火流程全在脚本里**(`Resource/Story/weapon_rpg.script`，`milestoneNames={"weapon_rpg"}`)：UE层只发`weapon_fire`(`fire`枪口位置、`direction`镜头朝向、`hit`瞄准射线交点等)→脚本`spawn_actor`在枪口放`/Game/Asset/Projectile/BP_RPG7Rocket`→火箭弹Actor自己飞行/尾焰/碰撞/爆炸特效(`Forever/Player/ForeverRocketProjectile.md`)→撞击后`projectile_hit`回到本脚本→`area_damage`(半径800cm、中心伤害250，估计起点)。
- **模型**：`firstPersonMeshPath`=`/Game/3rdParty/RPG7/StaticMeshes/SM_RPG7_Launcher`(单个StaticMesh，**空发射器**，不带火箭弹)。由`Tools/split_rpg7.py`从Fab下载的`rpg7.glb`拆出(原模型发射器+火箭弹摆在一个"爆炸图"里)，`Tools/import_rpg7.py`导入。模型原点在发射器尾端枪管轴线上、枪口朝+X，长95.7cm，`modelScale=1`，`muzzleOffsetX=96`(管口)，`anchorOffset*`=0；`attachOffset`=(-24, 7.5, 15)(瞄准时比步枪靠后模型长度的1/4、靠右模型宽度的一半、高15)；`backOffset`=(-5, 4-13.6, -32)：背着时比步枪降低模型长度的1/3(32)、向背部中心平移模型宽度的一半(13.6，宽度按背着时的侧向范围约27估)。
- 开局在背包里(不挂肩膀)，配10发`asset_ammo_rpg`，从背包拖到肩膀再按1/2切换，见`Forever/Player/ForeverCharacter.md`。

## 依赖关系

- 依赖：`player/weapon_mod.h`。
- 被谁依赖：`basic.cpp`注册；`Core/player/weapon.h`消费；`Source/Forever/Player/ForeverWeaponComponent`读数值。
