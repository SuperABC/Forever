# weapon_mod.h / weapon_mod.cpp

## 职责

`WeaponMod`：一把武器"打起来是什么样"的**纯数据描述**——伤害/射速/弹匣/挂载点/资产路径等字段直接是public成员(照抄`BuildingMod`的风格)。

## 关键设计

- **没有`Fire()`/`ApplyRecoil()`这类虚方法**：开火/命中判定整个放在Forever层的`UForeverWeaponComponent`里，直接读这些字段计算，不需要`WeaponMod`反过来调用UE接口。伤害不在`WeaponMod`里：开火后每颗弹丸向武器自己的脚本发`weapon_fire`事件，特效和伤害由脚本的`spawn_effect`/`single_damage`等Change完成(2026-11-17，原`damage`/`ComputeDamage`/`muzzleVelocity`/`bulletGravityScale`已删)。`scriptModName`/`milestoneNames`指定武器脚本(同`AssetMod`)，都空则没有脚本、开火事件不发。
- **`SetProperty()`两段式**：具体子类在这里填字段，不在构造函数里赋值；调用点在Core层`Weapon::Weapon()`构造函数里，创建完实例后立刻调一次。
- **武器和`Asset`背包系统靠"同一个id字符串"桥接**，不靠字段关联：`WeaponMod`自己不持有assetId，见`Core/player/player.h`的`ActivateShoulderWeapon()`/`Core/player/asset.md`。
- **资产引用**：`firstPersonMeshPath`是UE软路径，以`_C`结尾=整件武器的Actor Blueprint类，否则=单个StaticMesh；留空=没有模型，开火/伤害/换弹逻辑不受影响。
- **挂载和锚点**(两个锚点都是枪托，模型原点在枪托/握把附近，+X朝前)：`attachOffset*`是瞄准时枪托相对基础位置(`Core/player/weapon.cpp`的`kAimBase*`)的偏移；`backOffset*`是背枪时相对基础位置(`kBackBase*`)的偏移，写的是右肩，左肩自动把Y取反；`modelScale`是模型缩放；`anchorOffset*`是"枪托"在模型自己坐标系里的位置(缩放后，默认0=模型原点)；`muzzleOffset*`是枪口在模型坐标系里的位置，开火弹道debugLine从这里画出。
- **射击参数**：`fireRate`(最小开火间隔，秒)/`fullAuto`/`maxRange`/`baseSpread`(散射半角，度，圆锥内随机)。
- **弹药**：`magazineCapacity`/`reloadDuration`/`ammoType`(换弹时从Player背包按这个type扣备弹，`Player::ConsumeByType`)。
- **后坐力**：每次开火瞬间踢一下视角，不自动回正，靠玩家压枪抵消。`recoilPitchMin/Max`(都是正数，保证只往上踢)和`recoilYawMin/Max`在范围内随机取值，模拟每发不完全一致的手感，不是精确复刻某支枪的真实轨迹。单发/连发都走同一个`Fire()`调用，所以都会踢。
- 瞄准倍率明确不做，见`weapon_*.md`。

## 依赖关系

- 依赖：`common/mod_state.h`。
- 被谁依赖：`weapon_factory.h`创建它；`Core/player/weapon.h`消费它；`Source/Forever/Player/ForeverWeaponComponent`、`ForeverWeaponVisualComponent`读它的字段；具体实现在`Source/Basic/player/weapon_*.*`。

## 待办/后续阶段

- 无。

## 开火方式 `FireMode`
`fireMode`(`Hitscan`默认 / `Custom`)只是数据标签，决定Forever层`UForeverWeaponComponent::Fire()`走哪条结算路径：`Hitscan`=摄像机射线瞬间判定，**每颗弹丸(`pelletCount`，各自在`baseSpread`圆锥内散射)各向武器脚本发一次`weapon_fire`事件**，特效和伤害由脚本完成(2026-11-17取消了飞行弹道：`Ballistic`、初速、重力倍率、曳光线都已删，狙击枪没有下坠和飞行时间；以后带发射物的武器由蓝图弹射物Actor覆盖，见`Forever/Player/ForeverProjectileBridgeComponent.md`)，`Custom`=开火后调`WeaponMod::OnFire(PostHandle*)`，由mod自己通过`post`向Core发请求(相机发`"screenshot"`截屏；不耗弹药、无伤害、无脚本)。`OnFire`默认空实现，`post`只在这次调用期间有效。`magazineCapacity=0`表示不用弹药(不扣子弹、不换弹、不显示弹药数)。
