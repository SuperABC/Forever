# ForeverProjectileBridgeComponent.h / ForeverProjectileBridgeComponent.cpp

## 职责

mod蓝图里的**弹射物Actor**(火箭弹等)和脚本之间的桥：蓝图自己用引擎自带的`ProjectileMovementComponent`/`BoxComponent`实现飞行(加速)、碰撞、尾焰和爆炸特效，
撞到东西后调`ReportHit(Hit, Normal, Sufferer)`，由**所有者(发射它的武器)的脚本**在`projectile_hit`事件里用`spawn_effect`/`area_damage`这些Change完成特效和伤害。
蓝图不需要知道Change的存在，也不能直接调Change——伤害规则和数值始终在脚本里。

## 关键设计

- `OwnerScriptKey`：所有者脚本的稳定键(如`weapon:weapon_rocket`)，由放置弹射物的Change(`spawn_actor`，2026-11-18已做，生成时自动写成执行它的脚本的键)赋值；空=`ReportHit`什么都不发生，蓝图自己的表现照常。
- 只依赖UE类型(不依赖Core/Dependence)，所以`Forever_Mod/Test/UE/Test/Source/Forever/Player`里硬链接了同一份文件，mod蓝图能在迷你工程编辑器里选到这个组件(同`AForeverExplosionEffect`，见`Forever_Mod/README.md`)。
  类名和`UPROPERTY`/`UFUNCTION`名一旦被mod蓝图引用就成了兼容性接口，不要随便改。
- 怎么把命中交给脚本又不依赖Core：组件里只有一个静态委托`HitHandler()`(`FForeverProjectileHitHandler`)，主程序的`UForeverStoryFrameworkComponent::BeginPlay`绑定它，
  调`NotifyProjectileHit`发`projectile_hit`事件(local：`hit`/`normal`/`sufferer`)；迷你工程的桩模块没人绑，所以只是个可选的类。
- 火箭筒的完整链路(已实现，2026-11-18)：左键→`weapon_fire`→`Resource/Story/weapon_rpg.script`的`spawn_actor`(`fire`/`direction`)放出`BP_RPG7Rocket`并写入`OwnerScriptKey`→Actor自己飞行→碰撞→Actor自己生成爆炸特效→`ReportHit`→`projectile_hit`→脚本`area_damage`→Actor自毁。火箭弹Actor见`ForeverRocketProjectile.md`(C++类，蓝图只填参数)；mod自己的弹射物蓝图仍可以只靠这个组件+蓝图节点实现，不依赖那个C++类。

## 依赖关系

- 依赖：UE `Components/ActorComponent.h`。被谁依赖：`UForeverStoryFrameworkComponent`(绑定处理者)。
