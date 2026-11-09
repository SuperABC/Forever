# ForeverBulletSubsystem

## 职责
`FireMode::Ballistic`武器的子弹仿真器(`UTickableWorldSubsystem`，只在Game/PIE世界创建)。`UForeverWeaponComponent::FireBallistic`发射时调`AddBullet`，之后子弹每帧在这里飞行、碰撞、结算，并画曳光弹。

## 关键设计
- **子弹是纯数据`FBullet`，不是Actor**：位置、速度、已飞距离、射程、重力倍率、射手弱引用、武器type、伤害曲线、忽略列表。没有每发一个Actor的开销。
- **固定子步**：`Tick(DeltaTime)`里按`1/120s`切子步(单次最多推进0.1s防卡顿帧)，每个子步先积分重力(`世界重力Z × bulletGravityScale`)，再对`prev→next`做`LineTraceSingleByChannel(ECC_Pawn)`线段扫掠，高速也不会穿透。ECC_Pawn的原因同`ForeverWeaponComponent.md`。
- **暂停/慢动作自动生效**：DeltaTime已带全局时间膨胀，商店/地图暂停时子弹冻结，投掷列表0.1x慢动作里子弹也变慢。
- **结算**：扫到任何阻挡物子弹就消失；只有打中`ACitizenElement`才调`ApplyCitizenHit`(伤害、爆炸、致死广播`CitizenDeceaseEvent`并`HandleCitizenDeath`)，打墙/地面无任何效果。飞出`maxRange`(按累计飞行距离)也消失。伤害按命中点的累计飞行距离在伤害曲线上插值。
- **伤害曲线采样**：`WeaponMod::ComputeDamage`是虚函数(mod可override成任意衰减)，而Weapon对象会随换枪销毁。所以发射时在`[0,maxRange]`等距采样9点存进子弹，命中时线性插值，既保留mod的曲线又不依赖Weapon的生命周期。
- **射手失效**：飞行期间射手可能死亡/销毁，命中时`shooter`弱引用失效则击杀事件的凶手为空，其余照常结算。命中时目标可能已被别的子弹打死(`IsDead()`)，此时无效果。
- **曳光弹**：一个`UInstancedStaticMeshComponent`(引擎Sphere，沿飞行方向拉成粗1.5cm的尾巴)承载所有子弹，每帧重建实例；材质复用`/Game/Asset/Materials/ExplosionFire`(Additive自发光，已抵消自动曝光，参数`Color`/`EmissiveStrength`/`Opacity`，带Fresnel衰减)。尾巴长度=max(300cm, 本帧飞过的距离)，且不超过已飞距离(刚出膛不拖到玩家脸上)。**为什么这么大**：子弹一帧飞好几米，尾巴短了就只闪成一个点；Fresnel让细长体边缘变暗，所以亮度取大。长度/粗细/亮度是cpp顶部的常量，需要在PIE里目测调整。

## 注意
- 子弹不进存档，换关/退出即丢。
- 子弹出生点在枪口，枪口如果在墙内(贴墙开枪)扫掠可能直接命中墙而消失，目前不处理。
- `WeaponMod`新增了`muzzleVelocity`/`bulletGravityScale`/`pelletCount`三个数据字段，Dependence头文件变了，Basic和`Forever_Mod/*`外部mod需要一起重编。
