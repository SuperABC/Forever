# ForeverWeaponVisualComponent

武器可视化：肩膀槽位里有武器就生成模型、取下就删除；不瞄准时背在背后(绑脊柱骨骼，跟着动画)，瞄准时当前激活的那把举到枪口位置、枪口朝摄像机瞄准方向。**只是视觉**，开火判定仍然是 `UForeverWeaponComponent::Fire()` 从摄像机射出。

## 结构
- 作为 `AForeverCharacter` 的常驻组件（市民以后装备武器也复用）。默认不 Tick，`BeginShowing()` 才打开：现在由 `AForeverCharacter::PossessedBy` 开启、`UnPossessed` 调 `EndShowing()` 销毁全部模型（换人/上车时模型跟着玩家的肩膀走）。
- 数据源 `GetShoulderAsset(slot)`：现在 = 被玩家占有时读 `Player` 的左/右肩（`Player` 的肩膀是全局的），否则没有。市民有自己的装备时改这一个函数。
- 武器 type（`asset->GetWeaponType()`，如 `weapon_pistol`）→ 临时构造 Core `Weapon` 读 `WeaponMod::firstPersonMeshPath` 和 `attachOffsetX/Y/Z`，缓存到 `defCache`。`firstPersonMeshPath` 以 `_C` 结尾 = Actor Blueprint 类（`LoadClass<AActor>`，整件武器，枪体+弹匣的相对位置在蓝图里，所以**不需要手动抄偏移**）；否则 = 单个 StaticMesh。Basic 的手枪/步枪指向资源包 `Weapons_Free/Meshes/Actors/AC_pistol_001`/`AC_rifle_001`。
- 模型用 `SpawnActor` 生成（Owner=角色，关碰撞、关 Tick），缩放取 `WeaponMod::modelScale`。资源包里是 StaticMesh，没有骨骼/插槽；**两个锚点都是枪托 = 模型原点**（资源包模型的轴心在枪托/握把附近，+X 朝前），不需要算包围盒。

## 同步（背包没有变化事件，所以轮询）
`TickComponent` 每帧：
1. 左右肩的 `Asset*` 与缓存不同（装备/拖走/换成别的）→ 重建那个槽位的模型，空了就删。
2. 手上正在用的武器（`UForeverWeaponComponent` 里能开火的那份）：`AForeverCharacter::ActivateShoulderWeapon` 会 `NotifyActivated(slot, asset)` 记下它是由哪个 Asset 装备出来的；激活槽位里的 `Asset*` 变了 → 自动 `ActivateShoulderWeapon(activeSlot)`（空=卸下，有枪=装备）。这顺手修掉了"拖走激活肩膀里的枪后模型消失但还能开火"的老问题。注意 `EquipWeapon` 会把弹匣补满，只在 Asset 真正变化时才会发生；投掷物激活（`ClearWeapon` 但槽位没变）不会误触发。
3. 摆放。

## 摆放
- **背枪**：`AttachToComponent(GetMesh(), holsterBone)`，`holsterBone` 默认 `spine_03`；枪托放在 `WeaponMod::backOffsetX/Y/Z`（背枪锚点，相对该骨骼，角色朝向下的坐标 X前/Y右/Z上，写右肩的位置，左肩的枪自动取反 Y）；枪的姿势用组件的 `holsterRotationLeft/Right`（模型+X 枪口的朝向），内部按骨骼参考姿势朝向补偿（和 `AForeverCharacter::PlaceAvatar` 同一个办法）。
- **瞄准**（`IsAiming() && HasWeapon()`，只对激活的那把；投掷物瞄准时 `HasWeapon()==false`，枪留在背后）：挂到角色根，每帧设置相对变换——枪托（模型原点）固定在 `WeaponMod::attachOffset`（瞄准锚点，"右手靠前"），旋转取控制器俯仰（瞄准时角色偏航本来就跟摄像机），枪体从枪托向前延伸、绕枪托俯仰，所以不会向后穿进身体。
- **第一人称**：背枪对本人隐藏（`SetOwnerNoSee`），瞄准持枪可见。
- 没有手部动画/IK：瞄准时手臂仍是 Unarmed 动画，枪"悬"在手前。

## 要调的值（都是大概值）
- **每把武器各自**（`WeaponMod`，`Basic/player/weapon_basic.cpp`，由 Core 决定）：`modelScale`；`attachOffsetX/Y/Z` = 瞄准锚点（枪托在角色本地的位置）；`backOffsetX/Y/Z` = 背枪锚点（枪托相对背枪骨骼的位置，右肩，左肩取反 Y）。
- **组件上**（`UPROPERTY(EditAnywhere)`）：`holsterBone`、`holsterRotationLeft/Right`（背枪的姿势/朝向）。
- 生成模型时打一行日志 `scaled model size ... cm, x from ... to ...`，调缩放时对照这行的实测尺寸。
- 资源包模型实测（从 .uasset 包围盒解出）：手枪约 97cm 长、步枪约 226cm 长，+X 朝前，轴心在枪托附近（X 范围约 -19 到 +78 / +207），比真实尺寸大很多；Basic 里手枪 `modelScale`=0.25（约 24cm）、步枪 0.4（约 90cm）。
## 打包
武器资源只通过字符串路径引用，cook 追踪不到；`Config/DefaultGame.ini` 已经有 `+DirectoriesToAlwaysCook=(Path="/Game/3rdParty")`，不需要额外配置。

## 锚点偏移 `anchorOffsetX/Y/Z`（WeaponMod，2026-10-24）
"枪托"锚点默认是模型原点，`anchorOffsetX/Y/Z` 把它在**模型自己的坐标系里**挪一下（cm，缩放后的尺寸，X前/Y右/Z上）。背枪锚点（`backOffset`）和瞄准锚点（`attachOffset`）都是"把这个锚点放到目标位置"，所以锚点往下(-Z)挪=枪整体显示得更高，往上(+Z)=更低，往枪托方向(-X)=枪更靠前。Basic 里手枪 `anchorOffsetZ`=-1.5、步枪 +2。

## 基础位置在 Core，mod 只微调（2026-10-24）
瞄准锚点和背枪锚点的**基础位置**由 Core 决定（`Core/player/weapon.cpp` 的 `kAimBase*`：瞄准 (20, 20, 40)，相对胶囊根；`kBackBase*`：背枪 (-22, 16, 12)，相对 `holsterBone`，右肩，左肩取反 Y）。`WeaponMod::attachOffset` / `backOffset` 现在只是在基础位置上的微调，不填(0)就是基础位置；`Weapon` 的 getter 返回的是"基础+微调"之后的最终位置，UE 侧不变。Basic 里手枪/步枪只用这两个字段分别微调（不用 `anchorOffset`、`muzzleOffset` 也没填）。
