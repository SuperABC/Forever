# ForeverExplosionEffect

## 职责
命中点的一次性程序化爆炸特效(纯视觉)。由脚本的`spawn_effect` Change(`UForeverAssetFrameworkComponent::ApplySpawnEffect`，枪的`weapon_fire`/手雷的`time_up`等事件里)通过`AForeverExplosionEffect::Spawn(world, 位置, 法线, 缩放, 蓝图路径)`生成,播放`totalDuration`秒后自动`Destroy()`。

## 关键设计
- **几何不依赖手工资产**:网格全部用引擎内置`/Engine/BasicShapes/Sphere`球体网格+`UPointLightComponent`,由Actor的`Tick`驱动。Niagara未启用,也不引入。
- **五个层次**:火球(快速膨胀后缓慢胀大并淡出,橙黄→暗红)、冲击波环(压扁的球当圆盘,盘面法线对准命中面,外扩同时淡出)、火花(`SparkCount`个小球,命中面法线半球内随机射出,Tick里手动积分重力,不开物理)、闪光(点光源强度平方衰减)、烟(`SmokeCount`=10个小灰球延迟出现、上升变大、淡入淡出)。
- **两个Substrate材质**:`/Game/Asset/Effects/Explosion/ExplosionFire`(Additive,自发光=Color×EmissiveStrength×Opacity×(1-Fresnel),火球/环/火花共用)和`ExplosionSmoke`(TranslucentGreyTransmittance,自发光=Color,不透明度重载=Opacity×(1-Fresnel),烟用)。参数名`Color`/`EmissiveStrength`/`Opacity`必须与材质一致。淡出统一动画`Opacity`,火球初期`EmissiveStrength`很高以触发Bloom。路径是`EXPLOSION_FIRE_MATERIAL_PATH`/`EXPLOSION_SMOKE_MATERIAL_PATH`两个宏。
- **曝光在材质里抵消**:项目太阳是10万lux,自动曝光下白天画面很暗,C++读不到渲染线程算出的曝光值。所以`ExplosionFire`/`ExplosionSmoke`的自发光都乘了材质节点Eye Adaptation Inverse(1/当前曝光),让参数值直接等于画面显示亮度,昼夜通用。代码里`emissiveScale`(火)和`smokeBrightness`(烟)只是这个显示亮度的倍率。**不要从这两个材质里去掉该节点**,否则白天特效会变黑。(历史:曾按太阳强度在C++里估算场景亮度,昼夜比例对不上,已废弃。)
- **所有部件`NoCollision`+不投阴影**,避免挡后续射线或投出实心阴影。
- **延迟生成**:`Spawn`用`SpawnActorDeferred`先写入法线/缩放再`FinishSpawning`,`BeginPlay`里的初始化(火花方向、环朝向)才能用到。
- **镜头震动**:`Spawn`末尾用`UGameplayStatics::PlayWorldCameraShake`播放`UForeverExplosionShake`(`ForeverExplosionShake.h/.cpp`),内半径300cm满强度、外半径1500cm衰减为0,所以只有玩家离爆炸近才抖。UE5.7已移除`ULegacyCameraShake`,Perlin/Wave模式在未启用的EngineCameras插件里,因此自己继承`UCameraShakePattern`:几条正弦叠加+`(1-t)^2`衰减,每次随机相位。`IsFinishedImpl`必须自己按已过时间返回,否则基类认为立刻结束。
- 尺寸/时长/速度都是`UPROPERTY(EditAnywhere)`,单位为scale=1时的cm/秒,`effectScale`整体缩放。

## 依赖关系
- 依赖:`/Engine/BasicShapes/Sphere`、`/Game/Asset/Effects/Explosion/ExplosionFire`、`ExplosionSmoke`(手工建的材质资产)。
- 被谁依赖:`UForeverWeaponComponent::Fire()`(命中citizen后调用`Spawn`)。
- 不涉及Core/Mod。

## 蓝图子类 / spawn_effect（2026-11-17）

`Spawn(world, location, normal, scale, effectClassPath)`新增可选的蓝图类路径：非空时`LoadClass<AForeverExplosionEffect>`并生成该子类，路径不含`.`时自动补`.<名>_C`；指定了路径却加载不到时**不生成任何特效**(不回退默认爆炸，只打一条Warning`特效蓝图加载失败，不生成特效: <路径>`)；路径留空才用默认类(这是枪击等C++直接调`Spawn`的行为；脚本`spawn_effect`的`effect`留空则根本不调`Spawn`，没有默认特效)。mod的特效就是**父类为本类、只覆盖`UPROPERTY(EditAnywhere)`参数的蓝图**，可以放在mod的UE迷你工程plugin里，也可以放在主工程(枪击`/Game/Asset/Effects/BP_BulletImpact`、手雷`/Game/Asset/Effects/BP_GrenadeExplosion`，由`Tools/make_effect_blueprints.py`生成，2026-11-17手雷从Test迷你工程搬回主程序)，由脚本`spawn_effect`的`effect`字段指定。新增被BP继承的参数时注意名字一经发布就是兼容接口。
