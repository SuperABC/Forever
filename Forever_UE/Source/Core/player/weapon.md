# weapon.h / weapon.cpp

## 职责

`Weapon`：一把武器的实例——和`Asset`一致的"factory+mod，构造时把字段原样拷一份到自己身上"
模式（见`asset.h`/`asset.md`），不把`WeaponMod*`直接暴露给Forever层，所有字段都通过
Getter读。构造时调一次`mod->SetProperty()`(两段式约定，和`Asset::Asset()`同一个调用
时机)。

## 和其余concept一致的三层结构

其余所有concept(`Asset`/`Room`/`Storage`/`Product`/...)都有`Dependence` mod → `Core`
wrapper → `Basic`默认内容的三层结构，`Weapon`是这套结构里Core层的wrapper。`Forever`层的
`UForeverWeaponComponent::currentWeapon`成员是`Weapon*`（不是裸`WeaponMod*`），
`EquipWeapon()`里`new Weapon(&Registry::Get().GetWeaponFactory(), id)`，构造函数内部
完成创建+`SetProperty()`两步，`DestroyWeapon()`是普通`delete`。

## 为什么没有`Fire()`/`ApplyRecoil()`这类虚方法——和Core层wrapper是两件事

`WeaponMod`本身只有纯数据字段(+`Custom`开火方式的`OnFire`回调)，没有`Fire()`/`ApplyRecoil()`——避免发明
"Dependence层声明、Forever层实现"的跨层虚方法签名，真正的开火判定/相机/碰撞查询整个放在
`UForeverWeaponComponent`里，见`weapon_mod.h`文件头注释。这条决策管的是`WeaponMod`要不要长虚方法接口，和
"要不要有Core层wrapper包一层"是两个独立的问题——`Asset`/`AssetMod`同样"瘦"但照样有Core wrapper。

## 武器脚本(2026-11-17)

`Weapon`自己持有一份`Script`(`GetScript()`)：mod的`scriptModName`/`milestoneNames`按`MakeScript`创建，都空则为`nullptr`。
开火后的特效和伤害全由这个脚本响应`weapon_fire`事件完成，**不依赖肩上的`Asset`**(武器逻辑和资产逻辑解耦：资产脚本管捡起/丢弃
等选项，武器脚本管开火)。

- **创建时机**：构造`Weapon`时不建脚本(`UForeverWeaponVisualComponent::FindDef`为读模型参数会临时构造`Weapon`，不该建脚本)，
  `Player`构造时给每个已启用的weapon mod各建一个`Weapon`并调`EnsureScript()`(2026-11-18)，常驻到`~Player`才销毁，不随装备/换枪增删(`Player::FindWeapon(id)`取；`UForeverWeaponComponent::EquipWeapon`只是取引用)。所以飞行中的火箭弹在换枪后仍能通过`weapon:<id>`找到脚本；脚本变量跨换枪保留，但不进存档。
- **稳定键**`weapon:<武器id>`，`ScriptOwnerKind::Weapon`；`UForeverStoryFrameworkComponent::FindScriptByKey`按它从玩家Pawn的`UForeverWeaponComponent`取装备中的那把，
  这样`create_timer`等按键查脚本的机制也能用在武器脚本上。
- 武器脚本没有场景实体：`spawn_effect`/`area_damage`必须填`position`(通常是`$$local.hit`)。

`Weapon`不算伤害、不模拟弹道，`Weapon::GetFireMode()`只有`Hitscan`/`Custom`两种。

## 依赖关系

- 依赖：`Dependence/player/weapon_mod.h`/`weapon_factory.h`（`WeaponMod`/
  `WeaponFactory`），`story/script.h`（武器脚本）。
- 被谁依赖：`Source/Forever/Player/ForeverWeaponComponent`（`currentWeapon`成员是
  `Weapon*`）。

## FireMode / UsesAmmo
`GetFireMode()`转发`WeaponMod::fireMode`；`UsesAmmo()`=`magazineCapacity>0`，相机这类不用弹药的武器返回false，`FireCustom(post)`转发`WeaponMod::OnFire(post)`(仅`Custom`开火方式用)。由`UForeverWeaponComponent`据此跳过弹药检查/换弹/弹药显示。
