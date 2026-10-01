# asset.h / asset.cpp

## 职责

`Asset`：物品/资产的实例——照抄`Storage`的"factory+mod+自己的可变状态"模式，不是照抄
老工程"类型定义和某个背包里的一份库存混一起"的`Asset`。`AssetMod`(`asset_mod.h`)是类型
定义，构造时把字段拷贝进自己的私有字段（不持有mod的可变状态），`count`/`contents`这些
"这一份具体是多少/装着什么"是`Asset`自己的可变状态。

老工程有`DefineEstate()`/`DefineVehicle()`/`DefineObject()`/`DefineContainer()`/
`DefineContainee()`一套工具方法帮子类拼装字段；这次不迁移，`AssetMod`子类照抄
`WheatProduct`/`PistolWeapon`的风格，直接在构造函数里赋值公开字段，见`asset_mod.h`。

## 四分类：一个枚举字段，不是四个子类

`AssetMobility`(`Estate`/`Vehicle`/`Object`/`Containee`)是`mobility`一个字段，四类共用
同一个`Asset`类。"是否为容器"是独立的正交属性：`volume > 0`即为容器，和`mobility`无关——
一个容器本身还能被装进另一个容器/背包，没有限制（照抄老工程）。

## 堆叠：`count`字段

老工程"一个实例=一个物体"，这次改成`Asset`带一个`int count`代表"这一份的数量"——120发
子弹是1个`Asset`实例(`count=120`)，不是120个实例。`Use()`每次消耗1份(`count--`)，
`count`降到0时返回`true`，调用方负责把这个`Asset`从所在容器/槽位/房间移除并`delete`。

## 容器：`contents`

只做一维体积预算：`GetSpace() = volume - Σ(content->size * content->count)`，
`AddContent()`空间不够直接拒绝，没有更复杂的三维摆放逻辑，照抄老工程。`contents`按
`Asset::GetName()`索引，独占持有——析构时递归`delete`。

## 按类型递归汇总/扣减：`CountByType`/`ConsumeByType`

供子弹/备弹这类"数量分散在多个容器/子容器"的场景用（这次测试案例其实只有一层，但递归
写法不比单层复杂，直接支持嵌套容器）。`ConsumeByType`两阶段实现：先用`CountByType`确认
总量够不够，够了才调用私有的`ConsumeUpTo()`真正扣减——避免"扣到一半发现不够"把状态改坏
一半。`ConsumeUpTo`按`contents`的遍历顺序(`unordered_map`，见`industry.md`同类说明：
顺序不重要，只要同一次调用内确定即可)逐个扣，扣到0的条目整条移除并`delete`，不够再递归
进子容器。

## 武器/背包系统的桥接：靠`GetType()`同一个id字符串，不是字段引用

`WeaponMod`和`AssetMod`各自独立注册，但用**同一个id字符串**（比如`"weapon_pistol"`）。
武器"激活"时是`ForeverWeaponComponent`的`WeaponMod*`实例，不激活时是槽位/容器里的一个
`Asset*`实例——`Asset`自己不持有到`WeaponMod`的任何指针/引用，两套系统靠这个共享id字符串
桥接，转换逻辑在`Source/Forever/Player/ForeverCharacter.cpp::ActivateShoulderWeapon()`里
（UE层），`Asset`/`Player`这两个Core类完全不知道`WeaponMod`/`ForeverWeaponComponent`的
存在。工业系统的`Product`(小麦/牛肉/汉堡)同理：`Asset`只存`GetType()`这个字符串
(比如`"product_burger"`)，不持有`Product*`——需要展示分类信息时由UI层另外查
`Industry::FindProduct(asset->GetType())`。

## 依赖关系

- 依赖：`Dependence/player/asset_mod.h`/`asset_factory.h`（`AssetMod`/`AssetFactory`）。
- 被谁依赖：`Player`（`leftHand`/`rightHand`/`backPack`/`leftShoulder`/`rightShoulder`
  五个槽位+`estateAssets`不动产载具表，均持有所有权）、`Room`（`assets`表，持有所有权）、
  `Source/Forever/Framework/ForeverAssetFrameworkComponent`（世界掉落物`AAssetElement`
  持有非owning指针，真正所有权仍在`Room`/容器里）。
