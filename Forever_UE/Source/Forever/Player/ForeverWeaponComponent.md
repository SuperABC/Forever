# ForeverWeaponComponent

## 职责
挂在`AForeverCharacter`上的武器组件,玩家角色与任何被`ChangeControlChange`切换过去操控的citizen共用同一基类,因此都天然带一份。负责开火判定/伤害/换弹/切枪/瞄准这条核心链路。

## 已实现范围
核心武器机制:`WeaponMod`/`WeaponFactory`+`Player`/`Citizen`的生命值字段+hitscan开火/伤害/击杀+换弹夹(扣减Player背包里的真实备弹)+数字键切枪+瞄准+后坐力,命中citizen致死会广播一次`CitizenDeceaseEvent`。

**未做**:通缉值(`ChangeWantedChange`)、警察`JobMod`、反抗小游戏、`ArrestPlayerChange`/`ReleasePlayerChange`——这些需要先有一个"把任意运行时Event广播给剧情脚本匹配"的通用机制(目前只有`BroadcastGameStart`这一个特化入口)和一个警察`JobMod`,工作量明显大于武器本身,留待后续单独实现。

## 关键设计

### 高频/低频两条路径
**高频路径**(每次开火/命中):`Fire()`直接调`LineTraceSingleByChannel`+`WeaponMod::ComputeDamage()`+`Citizen::TakeDamage()`,纯C++调用,不经过Story/Change/Event——一场火拼每秒可能打好几发,都走Story的`Script::MatchEvent`(对`mainScript`所有`actives`里程碑做完整匹配)是明显的性能陷阱(参照`ForeverPopulaceFrameworkComponent.md`里`Populace::Tick`集中调度导致卡顿的教训)。
**低频/叙事阈值路径**(目前只有一个):命中导致Citizen死亡时广播一次`CitizenDeceaseEvent`——这是唯一"值得让剧情系统知道"的时刻。

### 为什么不做成通用的"广播任意Event"机制
`UForeverStoryFrameworkComponent::BroadcastCitizenDecease`是单独写的方法,没有抽象出一个"把任意`Event*`广播给任意`Script*`"的通用接口:
1. 目前只有三个"构造具体Event、广播给具体脚本"的调用点(`BroadcastGameStart`/`OptionDialog`/`BroadcastCitizenDecease`),三者结构相似但每次要广播的对象集合不完全一样,案例数不够之前抽象形状大概率猜不对。
2. 通缉值→警察JobMod这条真正需要通用广播机制的链路不在当前范围内,现有需求用已有写法就能满足。

### `WeaponMod`没有`Fire()`/`ApplyRecoil()`虚方法
`WeaponMod`只留纯数据字段(`damage`/`maxRange`/`baseSpread`/`fireRate`/`magazineCapacity`/`reloadDuration`等)+一个纯数学的`ComputeDamage(float distance)`虚方法(只用Core类型,和`BuildingMod::Layout()`只用`Quad`/`Road*`同一约定),真正的开火判定/相机取值/碰撞查询都放在这个UE侧组件里——避免发明一套"Dependence层声明、Forever层实现"的跨层虚方法签名。

### 命中判定用`ECC_Pawn`,不是`ECC_Visibility`
这个项目的`Pawn`碰撞预设对`Visibility`通道是`Ignore`(见`CONVENTIONS.md`第11节),用`ECC_Visibility`做射线检测会打墙正常、打citizen直接穿过去。`Fire()`用`ECC_Pawn`通道,`BlockAll`/`BlockAllDynamic`对未覆盖通道默认Block(挡墙),`Pawn`预设对`Pawn`通道也是默认Block(挡citizen),两者都正常命中。
只有打中`ACitizenElement`才算"命中":`Cast<ACitizenElement>(hit.GetActor())`失败(打墙/打别的/未命中)在伤害判定上完全等价,直接返回,不计算`ComputeDamage()`。命中也不往屏幕上打印。

### 命中判定用摄像机射线
`LineTraceSingleByChannel`的`start`始终是摄像机位置(保证"看哪打哪")。不画子弹轨迹线(原来的`DrawDebugLine`调试线已删除)，目前没有`muzzleFlashEffectPath`/`impactEffectPath`对应的真实VFX资产。
命中citizen时在`hit.ImpactPoint`额外生成一个`AForeverExplosionEffect`(程序化爆炸,纯视觉,见`ForeverExplosionEffect.md`);打墙不触发。

### 挂载点本地偏移在`WeaponMod`里定义
`attachOffsetX/Y/Z`(角色本地坐标系,X前/Y右/Z上,单位cm)放在`WeaponMod`(`Source/Dependence/player/weapon_mod.h`),不写死在Forever层——不同武器可以给不同持握点,不需要改Forever层代码。`SpawnWeaponMesh()`里`SetupAttachment`之后紧跟`weaponMesh->SetRelativeLocation(...)`应用这个偏移;没有mesh时的退化分支(`character->GetActorLocation()`)也要应用同一套偏移(按角色朝向`RotateVector`),否则两条分支视觉效果不一致——这是个通用的坑:任何"有mesh用mesh位置,没mesh退化成角色位置"的分支都要确认两条分支应用了同一套变换。`PistolWeapon`/`RifleWeapon`(`weapon_basic.cpp`)当前给`(20, 20, 40)`,为目测值。该偏移只对没有真实持枪socket的情况生效,换成真实手部socket后可能需要归零或重新调。

### 武器装备:肩膀Asset与"活的"WeaponMod副本分开维护
`EquipWeapon(weaponId)`按`WeaponFactory`注册的id构造一份`Weapon`(Core层wrapper,`Source/Core/player/weapon.h`),找不到该id或未在`config.json`的`weapon_mods`数组启用时静默保留当前武器不变。`ClearWeapon()`销毁mesh+`Weapon`实例但不装备新的,供`AForeverCharacter::ActivateShoulderWeapon()`在"切到的肩膀是空的"时调用。`HasWeapon()`供`AForeverCharacter::StartAim()`判断能不能瞄准。这一层只负责"手上激活的是哪把武器及其开火/弹药状态",肩膀槽位上的Asset本体由`AForeverCharacter`/`Player`管理,见`ForeverCharacter.md`。

### 死亡处理:`Citizen::IsDead()`单向,不会复活
`Citizen`/`Player`(Core)有`health`/`maxHealth`+`TakeDamage`/`Heal`/`IsDead()`,目前没有死亡后果系统(复活/game over)。`UForeverPopulaceFrameworkComponent::HandleCitizenDeath(Citizen*)`负责`Destroy()`掉对应`ACitizenElement`(如果存在)并从`activeInstances`摘除;`TickComponent`的流式生成循环、`FindOrSpawnCitizenByName`都会跳过`IsDead()`的citizen。`Player::TakeDamage`目前没有任何调用方,为将来"citizen/警察向玩家开火"预留。

### 瞄准:复用现有cameraBoom/followCamera插值
实现在`AForeverCharacter`,不在本组件里。不新建第三个`UCameraComponent`,直接在`cameraBoom`(`USpringArmComponent`)上插值`SocketOffset`/`TargetArmLength`。子弹从瞄准相机位置发出不需要改本组件代码:`Fire()`本来就读`character->GetFollowCamera()`的实时位置做开火起点,`followCamera`本身没动,动的是父级`cameraBoom`,`GetComponentLocation()`自动取到瞄准后的新位置。完整设计见`ForeverCharacter.md`"瞄准"一节。准心单独放在`AForeverHUD`类(见`ForeverHUD.md`),瞄准中才画。

### 必须瞄准中才能开火
`Fire()`最前面(比`fireRate`冷却/弹药检查更早)判断`if (!character || !character->IsAiming()) return;`——不瞄准时按开火键完全是空操作,不消耗弹药、不触发冷却、不打印任何消息。这个检查必须放在最前面,否则"没瞄准时按开火键"仍会消耗弹药/占用冷却。全自动武器持续按住开火键但未瞄准时,`TickComponent()`每帧调`Fire()`都会在这里直接返回,一旦开始瞄准会立刻恢复开火,不需要重新按键。

### 按住左键连续开火+弹药显示
`PistolWeapon`/`RifleWeapon`均为`fullAuto = true`,连发逻辑(`StartFire()`/`TickComponent()`/`fireRate`冷却)对两者通用。玩家手上有武器时,`TickComponent()`每帧用`AddOnScreenDebugMessage`(固定Key`200`,显示0.1秒)刷新`"弹药: X/Y"`,所以一直显示;武器被清掉或换人操控后这一行很快自己消失。只认`IsPlayerControlled()`的拥有者。空弹匣扣扳机时什么都不打印。目前没有正式UMG弹药UI。

### 换弹:按`ammoType`从Player背包扣减整个弹匣容量的备弹
`Reload()`检查未在换弹中且弹匣未满后启动计时(`reloadDuration`),真正的备弹检查/扣减放在计时结束时(`TickComponent`),不在按下R的瞬间就扣。无论弹匣里剩多少发,一次换弹都按`WeaponMod::GetAmmoType()`扣除整个弹匣容量的备弹(`Player::ConsumeByType`),不是"补齐差额";备弹不够就不扣减、不填满,`currentAmmo`保持原值(没有做"部分填充"这个edge case)。

### 后坐力:仿PUBG手感,无自动回正
`WeaponMod`的`recoilPitchMin/Max`/`recoilYawMin/Max`(度)每次开火各自在自己的区间内独立取随机值(不是固定角度),`recoilPitchMin/Max`都是正数保证垂直方向只会往上踢。`Fire()`开火后直接改`Controller->GetControlRotation()`的Pitch/Yaw再`SetControlRotation`回去,不用`AddControllerPitchInput`/`AddControllerYawInput`(会被`InputPitchScale`/`InputYawScale`缩放,符号不直观)。故意不做自动回正/衰减——连发时准心会持续走偏,直到玩家自己拉回,这是预期行为。单发/连发走同一个`Fire()`,后坐力逻辑无需区分。`PistolWeapon`后坐力比`RifleWeapon`轻,具体数值为目测值。

### 与投掷物互斥
同一时刻手上要么是肩上的武器、要么是投掷物(见`ForeverThrowableComponent.md`)：激活投掷物会`ClearWeapon()`，按1/2切回武器会让投掷物退出激活。投掷物激活期间`StartFireWeapon`不会走本组件的`StartFire`。

## 已知坑(避免重踩)
- 碰撞检测必须用`ECC_Pawn`,不能用`ECC_Visibility`,见上"命中判定"一节及`CONVENTIONS.md`第11节。
- 任何"有mesh用mesh位置,没mesh退化成角色位置"的分支,要确认两条分支应用了同一套偏移/变换。
- 若武器/瞄准等功能挂在默认关闭Tick的`AForeverCharacter`子类(如`ACitizenElement`)上失效,优先检查该子类自己的`Tick()`覆写是否在条件满足时又把Tick关掉了,而不是只看`PossessedBy`有没有打开Tick——见`CONVENTIONS.md`第11节、memory"Citizen Tick disabled by default"。

## 资产状态
`PistolWeapon`/`RifleWeapon`(`Source/Basic/player/weapon_basic.h/.cpp`)的`firstPersonMeshPath`当前留空(武器模型尚未导入)。`SpawnWeaponMesh()`路径为空时直接跳过挂mesh,不影响开火/伤害/换弹逻辑。资产导入后把路径填进这两个类的`SetProperty()`即可(两段式构造约定,见`weapon_mod.h`),不需要改其它代码。

## 依赖关系
- 依赖:`Source/Core/player/weapon.h`(`Weapon`,封装`Source/Dependence/player/weapon_mod.h`/`weapon_factory.h`的`WeaponMod`/`WeaponFactory`,见`weapon.md`)、`Source/Core/common/registry.h`(`Registry::Get().GetWeaponFactory()`)、`Source/Core/populace/citizen.h`(`TakeDamage`/`IsDead`)、`Source/Core/player/player.h`(`ConsumeByType`换弹扣备弹)、`Source/Forever/Player/ForeverCharacter.h`(摄像机取值/`IsFirstPerson()`/`IsAiming()`)、`Source/Forever/Element/CitizenElement.h`(`GetCitizen()`反查命中目标)、`Source/Forever/Framework/ForeverFrameworkActor.h`/`ForeverStoryFrameworkComponent.h`/`ForeverPopulaceFrameworkComponent.h`(死亡时广播Event+清理Actor)。
- 被谁依赖:`AForeverCharacter`(`CreateDefaultSubobject`+`StartFireWeapon`/`StopFireWeapon`/`ReloadWeapon`/`ActivateShoulderWeapon`转发)。

## 待办/后续阶段
- 通缉值(`ChangeWantedChange`)/警察`JobMod`/反抗小游戏(`StartPuzzleChange`可复用现成的`PuzzleMod`/`Canvas`/`PuzzleWidget`机制)/`ArrestPlayerChange`——需要先补"任意Event广播给剧情脚本"的通用机制。
- `Player::TakeDamage`没有调用方,等有会反击的NPC/敌人再接上。
- 武器状态不按角色持久化(每次被占有都重置成默认肩挂装备)。
- 具体弹道/后坐力/散射数值需在PIE里调优(机制已实现,当前数值为随手定的测试值)。
- 准心是固定尺寸的原生"+",未换成贴图资产、未跟随`baseSpread`动态张开。
- 武器mesh资产(`firstPersonMeshPath`)待导入后手动填路径。
