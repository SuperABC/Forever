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

`WeaponMod`本身只有纯数据字段+一个`ComputeDamage(float)`虚方法，没有`Fire()`/
`ApplyRecoil()`——避免发明"Dependence层声明、Forever层实现"的跨层虚方法签名，真正的
开火判定/相机/碰撞查询整个放在`UForeverWeaponComponent`里，见`weapon_mod.h`文件头
注释。这条决策管的是`WeaponMod`要不要长虚方法接口，和"要不要有Core层wrapper包一层"是
两个独立的问题——`Asset`/`AssetMod`同样"瘦"（没有行为虚方法）但照样有Core wrapper。

## `ComputeDamage`是唯一没有在构造时静态拷贝的字段

其余所有字段（`damage`/`fireRate`/`magazineCapacity`/...）构造时原样拷进`Weapon`自己
的私有成员，`mod`指针之后只在析构时用来调`factory->DestroyWeapon(mod)`。但
`ComputeDamage(float distance)`是按距离算伤害衰减的虚方法（默认线性衰减，特殊武器可以
override成别的曲线），没法在构造时就"拷成一个值"，所以`Weapon::ComputeDamage()`转发
给`mod->ComputeDamage(distance)`——这是`mod`指针必须活到`Weapon`析构为止的唯一理由。

## 依赖关系

- 依赖：`Dependence/player/weapon_mod.h`/`weapon_factory.h`（`WeaponMod`/
  `WeaponFactory`）。
- 被谁依赖：`Source/Forever/Player/ForeverWeaponComponent`（`currentWeapon`成员是
  `Weapon*`）。
