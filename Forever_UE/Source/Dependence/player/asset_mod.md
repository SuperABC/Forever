# asset_mod.h / asset_mod.cpp

## 职责

`AssetMod`：物品/资产的类型定义，目录性质，比照`ProductMod`/`WeaponMod`。

## 关键设计

- **四分类是同一个`AssetMobility`枚举字段，不是四个子类**：`Estate`(不动产)/`Vehicle`(载具)/`Object`(独立物体)/`Containee`(容器内容物)。"是否为容器"用`volume > 0`表达，和`mobility`正交，照抄老工程的设计，见`Core/player/asset.md`。
- **`SetProperty()`两段式**：具体子类在这里赋值公开字段，不在构造函数里赋值。
- **字段**：`weight`/`size`/`volume`；`backpack`(能否放进"后背"槽)；`weapon`+`weaponType`(能否放进左肩/右肩，以及在肩膀上激活时装备的`WeaponMod` id)；`usable`(能否被"使用")；`throwable`(投掷物，按3键激活、瞄准后扔出，和`weapon`正交)；`meshPath`+`meshScale`(世界里掉落物/投掷预览的静态网格，空=Forever层默认Cube占位)。
- **脚本**：`scriptModName`+`milestoneNames`，和Vehicle/Job同一套机制；两者都空(默认)=这件资产没有脚本、也不挂弹选项的触发盒。`$$self.name`=资产实例名，`$$self.type`=mod id。
- 带存档钩子，默认不存任何东西，见`common/mod_state.md`。

## 依赖关系

- 依赖：`common/mod_state.h`。
- 被谁依赖：`asset_factory.h`创建它；`Core/player/asset.h`/`player.h`消费它；具体实现在`Source/Basic/player/asset_basic.*`。

## 待办/后续阶段

- 无。
