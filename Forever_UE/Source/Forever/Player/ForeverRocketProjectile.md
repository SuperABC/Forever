# ForeverRocketProjectile.h / ForeverRocketProjectile.cpp

## 职责

火箭弹Actor(`AForeverRocketProjectile`)：由脚本的`spawn_actor` Change在枪口放出(位置=枪口，+X=发射方向)，之后**飞行、加速、尾焰、碰撞、爆炸特效都是它自己的事**；
撞到任何东西时生成爆炸特效，再通过`UForeverProjectileBridgeComponent::ReportHit`把命中报给发射它的武器脚本(`projectile_hit`事件)，脚本里用`area_damage`结算范围伤害，然后Actor自毁。
伤害规则(半径/数值/署名)始终在脚本里(`Resource/Story/weapon_rpg.script`)，这个类只管视觉和"撞到了"。

## 关键设计

- **组件**：`UBoxComponent`根碰撞盒(对所有通道Block、QueryOnly)、`UStaticMeshComponent`火箭弹网格(`rocketMesh`，模型原点在**尾端**、弹头朝+X)、`UProjectileMovementComponent`、`UForeverProjectileBridgeComponent`、尾焰(拉长的球+点光源)、48个环形复用的烟团。
  Actor原点=碰撞盒中心=火箭弹中心，网格/尾焰挂在盒子尾端(局部`-boxHalfExtent.X`，`TailLocation()`)，所以`boxHalfExtent.X`要约等于火箭弹模型半长。
- **飞行**：`BeginPlay`里按朝向写初速`initialSpeed`；`UProjectileMovementComponent`本身没有加速度，`Tick`里沿当前方向把速度加到`maxSpeed`(`acceleration`)；无重力、不反弹、不转向。`lifeSeconds`(默认8秒)内没撞到任何东西就静默销毁(不爆炸)。
- **发射者不挡弹**：`spawn_actor`把`Instigator`设为玩家当前操控的Pawn，`BeginPlay`里对碰撞盒`IgnoreActorWhenMoving(Instigator)`。
- **碰撞=`OnProjectileStop`→`Explode`(只执行一次，`bExploded`)**：
  1. `AForeverExplosionEffect::Spawn(world, 撞击点, 法线, explosionScale, explosionEffectClassPath)`：爆炸是**独立的世界Actor**(Owner为空、不挂在火箭弹上)，火箭弹之后自毁不会带走它；路径留空=默认程序化爆炸，`explosionScale`默认3(比枪击大)。
  2. 击中活着的`ACitizenElement`则取市民姓名当`Sufferer`；`ReportHit(撞击点沿法线外推reportOffset, 法线, Sufferer)`。外推是为了让脚本里`area_damage`开`occlusion`时的遮挡射线不被爆心所贴的那面墙挡住；特效仍放在原始撞击点。
  3. `ReportHit`→`HitHandler`→`UForeverStoryFrameworkComponent::NotifyProjectileHit`→`BroadcastToScript`→`ProcessScriptActions`**全程同步执行**，返回时脚本里的`area_damage`已结算完，之后才`Destroy()`(先关碰撞、隐藏、停移动)。
  所有者脚本键(`OwnerScriptKey`)由`spawn_actor`在生成时自动写入；没有键(不是脚本放出来的)时`ReportHit`什么都不发生，爆炸特效照常。
- **尾焰(程序化，不需要Niagara)**：火焰球沿-X拉长并闪烁(复用`/Game/Asset/Effects/Explosion/ExplosionFire`材质，参数`Color`/`EmissiveStrength`/`Opacity`，同爆炸特效已抵消曝光)；每隔`smokeInterval`在尾端留一团世界坐标的烟(`ExplosionSmoke`材质)，半径从`smokeRadiusStart`胀到`smokeRadiusEnd`并淡出；尾部点光源随闪烁变化。
  以后要换Niagara尾焰可以在蓝图子类里隐藏这些组件、自己挂Niagara，不用改C++。
- **蓝图只覆盖参数**：`/Game/Asset/Projectile/BP_RPG7Rocket`(`Forever_UE/Tools/make_rocket_blueprint.py`生成)父类是本类，设`rocketMesh=SM_RPG7_Rocket`、`boxHalfExtent`、速度/爆炸/尾焰参数。所有`UPROPERTY`都是可调参数，名字一经蓝图引用就是兼容接口。

## 依赖关系

- 依赖：`ForeverExplosionEffect`(爆炸)、`ForeverProjectileBridgeComponent`(命中报告)、`Element/CitizenElement`、Core `populace/citizen.h`(取姓名/判死亡)、引擎`ProjectileMovementComponent`。
- 被谁依赖：`Tools/make_rocket_blueprint.py`生成的蓝图；脚本通过`spawn_actor`的蓝图类路径引用。

## 待办/后续阶段

- 弹道沿镜头方向、从枪口出发(平行于视线)，没有向瞄准点修正。
- (已解决 2026-11-18)火箭弹飞行中换枪：武器脚本由`Player`常驻持有，`weapon:weapon_rpg`键始终能解析，命中报告不会丢。
- 只做视觉与命中报告：没有冲击力/载具伤害，没有声音。
