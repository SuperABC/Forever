# asset.h / asset.cpp

## 职责

`Asset`：物品/资产的实例，用"factory+mod+自己的可变状态"模式——`AssetMod`
(`asset_mod.h`)是类型定义，构造时把字段拷贝进自己的私有字段（不持有mod的可变状态），
`count`/`contents`这些"这一份具体是多少/装着什么"是`Asset`自己的可变状态。

`AssetMod`子类直接在构造函数里赋值公开字段（参考`asset_mod.h`里`WheatProduct`/
`PistolWeapon`的写法），不提供字段拼装的工具方法。

## 四分类：一个枚举字段，不是四个子类

`AssetMobility`(`Estate`/`Vehicle`/`Object`/`Containee`)是`mobility`一个字段，四类共用
同一个`Asset`类。"是否为容器"是独立的正交属性：`volume > 0`即为容器，和`mobility`无关——
一个容器本身还能被装进另一个容器/背包，没有限制。

## 堆叠：`count`字段

`Asset`带一个`int count`代表"这一份的数量"——120发子弹是1个`Asset`实例(`count=120`)，
不是120个实例。`Use()`每次消耗1份(`count--`)，`count`降到0时返回`true`，调用方负责把这个
`Asset`从所在容器/槽位/房间移除并`delete`。

## 容器：`contents`

只做一维体积预算：`GetSpace() = volume - Σ(content->size * content->count)`，
`AddContent()`空间不够直接拒绝，没有更复杂的三维摆放逻辑。`contents`按
`Asset::GetName()`索引，独占持有——析构时递归`delete`。

## 按类型递归汇总/扣减：`CountByType`/`ConsumeByType`

供子弹/备弹这类"数量分散在多个容器/子容器"的场景用，递归写法支持任意层级嵌套容器。
`ConsumeByType`两阶段实现：先用`CountByType`确认总量够不够，够了才调用私有的
`ConsumeUpTo()`真正扣减——避免"扣到一半发现不够"把状态改坏一半。`ConsumeUpTo`按
`contents`的遍历顺序(`unordered_map`，顺序不重要，只要同一次调用内确定即可)逐个扣，扣到
0的条目整条移除并`delete`，不够再递归进子容器。

## 武器/背包系统的桥接：`AssetMod::weaponType`字段

`WeaponMod`和`AssetMod`各自独立注册，id各带自己concept的前缀(资产`"asset_pistol"`、武器`"weapon_pistol"`)。
武器"激活"时是`ForeverWeaponComponent`的`WeaponMod*`实例，不激活时是槽位/容器里的一个
`Asset*`实例。武器资产在`SetProperty()`里填`weaponType`(要装备的WeaponMod id)，`Asset::GetWeaponType()`
读它；转换逻辑在`Source/Forever/Player/ForeverCharacter.cpp::ActivateShoulderWeapon()`里（UE层），
`Asset`/`Player`这两个Core类不持有`WeaponMod`/`ForeverWeaponComponent`的任何指针。
工业系统的`Product`(小麦/牛肉/汉堡)和对应的资产(`asset_wheat`/`asset_beef`/`asset_burger`)目前没有
代码上的关联，id也不同(`product_wheat`/`asset_wheat`)；以后要互相换算时同样加一个显式字段，不要靠id相等。

## 依赖关系

- 依赖：`Dependence/player/asset_mod.h`/`asset_factory.h`（`AssetMod`/`AssetFactory`）。
- 被谁依赖：`Player`（`leftHand`/`rightHand`/`backPack`/`leftShoulder`/`rightShoulder`
  五个槽位+`estateAssets`不动产载具表，均持有所有权）、`Room`（`assets`表，持有所有权）、
  `Source/Forever/Framework/ForeverAssetFrameworkComponent`（世界掉落物`AAssetElement`
  持有非owning指针，真正所有权仍在`Room`/容器里）。
