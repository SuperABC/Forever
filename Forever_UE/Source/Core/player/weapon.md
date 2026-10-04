# weapon.h / weapon.cpp

## 职责

`Weapon`：一把武器的实例——照抄`Asset`的"factory+mod，构造时把字段原样拷一份到自己身上"
模式（Pattern B，见`asset.h`/`asset.md`），不把`WeaponMod*`直接暴露给Forever层，所有
字段都通过Getter读。构造时调一次`mod->SetProperty()`(两段式约定，和`Asset::Asset()`
同一个调用时机)。

## 补的是一个历史遗留空档，不是新设计

`WeaponMod`/`WeaponFactory`落地时(`weapon_system_plan.md`)的文件清单压根没规划Core层
wrapper，此后一直是Forever层的`UForeverWeaponComponent`直接拿裸`WeaponMod*`用——这和
其余所有concept(`Asset`/`Room`/`Storage`/`Product`/...)都有Core wrapper的既定三层结构
(`Dependence` mod → `Core` wrapper → `Basic`默认内容)不一致，属于设计时的遗漏，不是
"Weapon需要特殊对待"的刻意决定。这次补上Core层，纯粹是补齐结构，不改变任何运行时行为：
原来Forever层`EquipWeapon()`里`CreateWeapon()`紧跟着手动调`newWeapon->SetProperty()`，
现在这两步挪进了`Weapon::Weapon()`构造函数，调用时机完全一致。

## 为什么没有`Fire()`/`ApplyRecoil()`这类虚方法——和Core层wrapper是两件事

`WeaponMod`本身只有纯数据字段+一个`ComputeDamage(float)`虚方法，没有`Fire()`/
`ApplyRecoil()`——这是`weapon_mod.h`文件头注释记录的刻意偏离（避免发明"Dependence层
声明、Forever层实现"的跨层虚方法签名，真正的开火判定/相机/碰撞查询整个放在
`UForeverWeaponComponent`里）。这条决策管的是`WeaponMod`要不要长虚方法接口，和
"要不要有Core层wrapper包一层"是两个独立的问题——`Weapon`这个wrapper该不该存在，不受
`WeaponMod`瘦不瘦这件事影响，`Asset`/`AssetMod`同样"瘦"（没有行为虚方法）但照样有
Core wrapper。

## `ComputeDamage`是唯一没有在构造时静态拷贝的字段

其余所有字段（`damage`/`fireRate`/`magazineCapacity`/...）构造时原样拷进`Weapon`自己
的私有成员，`mod`指针之后只在析构时用来调`factory->DestroyWeapon(mod)`。但
`ComputeDamage(float distance)`是按距离算伤害衰减的虚方法（默认线性衰减，特殊武器可以
override成别的曲线），没法在构造时就"拷成一个值"，所以`Weapon::ComputeDamage()`转发
给`mod->ComputeDamage(distance)`——这是`mod`指针必须活到`Weapon`析构为止的唯一理由。

## 依赖关系

- 依赖：`Dependence/player/weapon_mod.h`/`weapon_factory.h`（`WeaponMod`/
  `WeaponFactory`）。
- 被谁依赖：`Source/Forever/Player/ForeverWeaponComponent`——`currentWeapon`成员
  从裸`WeaponMod*`改成了`Weapon*`，`EquipWeapon()`里`Registry::Get().GetWeaponFactory()
  .CreateWeapon(id)` + 手动`SetProperty()`换成`new Weapon(&Registry::Get()
  .GetWeaponFactory(), id)`，`DestroyWeapon()`换成普通`delete`。
