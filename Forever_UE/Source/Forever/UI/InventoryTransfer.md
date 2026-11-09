# InventoryTransfer

背包拖拽的搬运逻辑（不含控件）。`InventoryTransfer::Transfer(player, assetFramework, pawnLocation, source, dest, all, error)` 在**松手之后**才被调用，之前拖拽过程不做任何判断。

- 区域 `EInvZone`：左右肩、左右手、后背、容器列表、房间列表、附近列表；`FInvSource` 记物品当前位置（含所在容器 parent），`FInvDest` 记目标（容器列表/进入了子容器的房间、附近列表带 container）。
- 目标规则：肩=武器且空；手=`mobility==Object`且空；后背=`mobility==Object`+`GetBackpack()`且空；容器=存在，且不能把容器放进自己或后代（防环）、体积够（`AddContent`）；房间=`GetCurrentRoom()!=nullptr`；附近恒可。同区域内拖拽是无操作。**肩膀已有武器时不报"已有物品"，改为互换**（见下）。
- 数量：`all`=整个堆叠，否则只 1 个。目标里有同 type 的非容器物品 → 合并（数量累加，被并入实例销毁）；否则 n<count 时 `Player::SplitAsset` 拆成新实例（名字由 mod 保证唯一）。
- 失败回滚：整体搬运放不进去就放回原位；拆分放不进去就把数量加回并销毁拆出的实例。
- 世界表现：从房间根/附近根摘出 → `DestroyWorldAsset`；放入房间根/附近根 → `SpawnWorldAsset`（位置=玩家）。进入子容器里的物品没有 Actor。

## get_asset/lose_asset事件
`Transfer`在地面区(Room/Nearby)→身上区(肩/手/背/容器)时发`get_asset`(规则失败/放不下也发fail)，身上区→地面区成功时发`lose_asset`，同类区之间不发；事件发给资产自己的脚本(sender传空)。

## 肩膀武器互换
把武器拖到已有武器的肩膀上（左右肩同理，也包括左右肩之间互拖）：`SwapShoulderWeapon` 把肩上原来的武器放到新武器**原来所在的位置**（同一个槽位/容器/房间/附近），新武器背到肩上。两件武器都必须是单个（count==1）。流程：摘下新武器→摘下肩上武器→按普通目标规则校验肩上武器能放到原位置（例如原位置是后背就放不下，是手则要 mobility==Object）→各自放好；任一步失败整体回滚，提示原因。世界表现（原位置是房间根/附近根时生成肩上武器的 Actor、销毁新武器的 Actor）和 get/lose_asset 事件沿用普通搬运。肩上武器被换掉后，`ForeverWeaponVisualComponent` 会自动重新 `ActivateShoulderWeapon`。
