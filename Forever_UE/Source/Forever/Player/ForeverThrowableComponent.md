# ForeverThrowableComponent

## 职责
投掷物(手雷/烟雾弹)的激活、切换缓存、瞄准预览和投掷。挂在`AForeverCharacter`上，和`UForeverWeaponComponent`并列、互斥：同一时刻手上要么是肩上的武器，要么是投掷物。数字键3短按切换，右键瞄准显示预览曲线，左键扔出一个单位。

## 关键设计
- **投掷 = 把Asset扔出去**：不新增专用Actor。`Player::TakeOneThrowable(type)`取出一个单位(count=1的Asset)，交给`UForeverAssetFrameworkComponent::SpawnThrownAsset`——复用掉落物`AAssetElement`，由**真实物理模拟**飞行；Asset记进`Player::worldAssets`(房间外的物品，归Player所有、会存档)，扔出去之后不会自动消失，落地后的效果以后再设计。预览曲线只是预览，用`UGameplayStatics::PredictProjectilePath`算，真实轨迹由物理决定，可能和预览有偏差，起点/初速公式(`ComputeLaunch`)预览和投掷共用，参数(`throwOffset`/`throwSpeed`/`throwElevation`)按手感调。
- **缓存稳定性**：`rankByType`给每个type在第一次见到时分配一个永久序号(只增不删)，`entries`按序号排序。背包里少量存取不改变已有类别的相对顺序，某类用完后再次出现回到原位置而不是排到末尾。新type的序号按`Player::CollectThrowables()`的扫描顺序(左手->右手->背包)分配。
- **没有背包变化事件**，所以`Refresh()`在这些时机调用：每次按3、投掷前后、开始瞄准前、`UForeverInventoryWidget::RefreshBody()`(背包界面所有放入/取出/使用/丢弃都会走到)。
- **用完自动切换**：`Refresh()`发现激活的type没了，取旧下标处的下一个(原来排在它后面的会顶到同一个下标，末尾循环)并激活；一个都没有了才退出投掷状态、结束瞄准、显示"当前没有投掷物"。退出后手上是空的(不会自动切回武器)，按3可重新开始。
- **手的规则**(`CanThrow()`)：激活的type直接拿在某只手里(拿着它的手就是投掷手)，或至少一只手是空的。投掷物在手持容器**里面**不算拿在手里。两只手都被别的东西占着时仍可激活，只是`AForeverCharacter::StartAim`拒绝瞄准。
- **预览模型**：瞄准时在投掷起点显示一个`UStaticMeshComponent`，网格/缩放取自资产的`meshPath`/`meshScale`(和掉落物同一个`AAssetElement::LoadAssetMesh`)；`meshPath`空时用Cube缩到0.1。路径加载失败回退引擎球体(项目球体`/Game/Asset/Meshes/Sphere`需要用户在编辑器里从`Engine/Content/BasicShapes/Sphere`复制一份)。
- **HUD文字**复用武器名的`AddOnScreenDebugMessage` Key 201，切换武器/投掷物时同一行互相覆盖。
- **预览曲线画成世界里的`DrawDebugLine`线段**(每帧重画)：和场景一起深度测试，被墙/地形挡住的部分看不见。不用HUD的屏幕空间线条(没有遮挡关系)。代价：DrawDebug在Shipping/Test构建里被编译掉，和武器子弹轨迹同样的限制；以后要发布构建可换成沿路径的一串小球网格。瞄准投掷时`AForeverHUD`不画准星。
- **什么都可以标记成可投掷**：只看`Asset::IsThrowable()`，和是不是武器无关，比如汉堡把`throwable`设为true就能被扔(Containee机动性的物品只能放容器里，不能拿在手上)。

## 依赖关系
- 依赖：`Core/player/player.h`(`CollectThrowables`/`TakeOneThrowable`/`HasThrowableInHand`)、`AForeverCharacter`(瞄准状态、`GetWeaponComponent`，是它的friend)、`UForeverAssetFrameworkComponent::SpawnThrownAsset`、`AAssetElement::LoadAssetMesh`。
- 被谁依赖：`AForeverCharacter`(按键转发、`StartAim`/`StartFireWeapon`/`ActivateShoulderWeapon`)、`AForeverHUD`、`UForeverInventoryWidget`(刷新缓存)。

## 待办/后续
- 长按3弹出选择列表。
- 投掷物落地后的效果(手雷爆炸/烟雾弹冒烟/消失)，目前扔出去的物品会一直留在世界里。
- 房间外的物品由投掷和背包界面的室外丢弃产生；室外拾取没做(只认房间)。
- 投掷物的HUD显示还是资产id，没有中文显示名。
- 在被附身的`ACitizenElement`上需要PIE确认组件Tick正常(该类默认关闭Actor Tick)。
