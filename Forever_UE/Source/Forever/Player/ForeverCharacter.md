# ForeverCharacter

## 职责
玩家可控角色基类,被玩家自己的初始角色和`ChangeControlChange`切换过去操控的任意`ACitizenElement`共用。支持第三人称(肩后SpringArm+Camera)/第一人称(头部或眼高Camera)切换、移动/跳跃/冲刺、瞄准开火(转发给`UForeverWeaponComponent`,见`ForeverWeaponComponent.md`)、MeetOption对话选项操作。

## 关键设计
- 不建Blueprint外壳:构造函数用`ConstructorHelpers::FObjectFinder`/`FClassFinder`引用`SKM_Manny_Simple`/`SKM_Quinn_Simple`两个骨骼网格(`maleBodyMesh`/`femaleBodyMesh`，默认Manny，市民`Init`时按性别`ApplyBodyMesh`切换)、`ABP_Unarmed`动画蓝图和`IA_Move`/`IA_Look`/`IA_MouseLook`三个Input Action、`IMC_Default`/`IMC_MouseLook`两个Mapping Context作为默认值,均为`EditDefaultsOnly`,可被覆盖。
- `lookAction`(IA_Look)和`mouseLookAction`(IA_MouseLook)都绑到同一个`Look()`处理函数,但分属`IMC_Default`/`IMC_MouseLook`两个Context——手柄摇杆(模拟量)和鼠标位移(增量)需要不同的Input Modifier配置。两者都必须绑定,缺`mouseLookAction`会导致鼠标转视角没反应。
- 网格体相对Root的`(-96, 0, -90°)`偏移是标准UE Mannequin挂接惯例。
- `Jump`/`ToggleView`/`Sprint`/`MeetOptionUp/Down/Select`/`FireWeapon`/`ReloadWeapon`/`SwitchWeapon1/2`/`ThrowItem`/`AimWeapon`不直接引用`UInputAction`资源,改从`UForeverKeyBindingSubsystem`(见`Input/ForeverKeyBindingSubsystem.md`)按名字取运行时动态创建的Action,按键从外部配置文件读取。
- `PossessedBy`/`UnPossessed`只增删这个Pawn自己的`inputMapping`/`inputLookMapping`(`IMC_Default`/`IMC_MouseLook`)——`UForeverKeyBindingSubsystem`那份动态Context改由`AForeverPlayerController::InitializeOnce`常驻添加一次(见`ForeverPlayerController.md`),因为`TogglePhone`等热键要求任何时候都生效,不依赖"当前是否正占有一个真正的Pawn"。移动/视角这类Pawn专属映射仍然跟着"被占有"状态走,保证玩家上/下车、切换被操控角色时映射能跟着当前Pawn切换(见`CONVENTIONS.md`第7节)。
- `PossessedBy`只对`APlayerController`占有做武器/房间相关初始化(`AIController`占有市民导航测试时只加输入Context、其余直接返回)。**这里不发装备**:开局给什么由剧情脚本在`game_start`里用`grant_asset`决定(2026-11-25以前16个资产id和数量写死在这里;现在的那套在`Forever_Mod/Test/Resource/test.script`)。肩膀/背包槽位挂在Player身上,换人操控时跟着玩家走。每次占有都先`weaponVisualComponent->BeginShowing()`再`ActivateShoulderWeapon(1)`激活左肩,让新角色手上握着枪;并按胶囊体当前重叠的房间触发盒重新设置`Player::currentRoom`(占有前就站在房间里,引擎不会补发BeginOverlap)。`UnPossessed`时`EndShowing()`、移除本Pawn的Context、关闭投掷物列表。
- **V键第一/三人称切换**(`ToggleCameraView`):
  - `firstPersonCamera`优先挂到骨骼网格`head`socket;不存在则退化为挂在Root上、用胶囊体半高0.9倍作为眼高近似值。
  - 两个摄像机同一时刻只有一个`Active`(`UCameraComponent::SetActive`),不是靠`SetViewTarget`,因为两者都挂在同一Pawn上。
  - 切第一人称时`GetMesh()->SetOwnerNoSee(true)`避免本地玩家看到自己身体穿模;`bUseControllerRotationYaw`/`CharacterMovement.bOrientRotationToMovement`由`UpdateRotationMode()`统一切换。
- **Shift冲刺**(`StartSprint`/`StopSprint`,按住触发):`MaxWalkSpeed`在`walkSpeed`(默认180，市民脚本`speed`倍率1.0=按它走)和`walkSpeed * sprintSpeedMultiplier`(默认27.8，冲刺速度约5000)之间切换,冲刺速度是走路速度的倍数,改`walkSpeed`会联动。无专属冲刺动画,`BS_Idle_Walk_Run`按速度采样,超出原本"Run"档位设定的速度上限后画面会有轻微脚下打滑感,是动画资产本身的限制。
- **瞄准**(`StartAim`/`StopAim`,默认右键):手上没有激活武器(`weaponComponent->HasWeapon()`为false)或Player的右手(`GetRightHand()`)正攥着背包物品时拒绝瞄准。复用`cameraBoom`(`USpringArmComponent`)现有插值,不新增第三个摄像机:瞄准目标`aimSocketOffset`/`aimArmLength`,取消瞄准回到`defaultSocketOffset`/`defaultArmLength`,`Tick()`里用`FMath::VInterpTo`/`FInterpTo`每帧推进(插值速度`aimTransitionSpeed`)保证过渡平滑。瞄准时角色朝向跟随摄像机而非移动方向:`UpdateRotationMode()`统一计算`bFaceCamera = bIsFirstPerson || bIsAiming`,`ToggleCameraView`/`StartAim`/`StopAim`都调用这个函数而非各自维护判断。
- **数字键切枪**(`ActivateShoulderWeapon(slot)`):激活左肩(1)/右肩(2)当前挂着的武器Asset(若有),武器Asset本身留在肩膀槽位不动,`weaponComponent->EquipWeapon()`只是让组件额外持有一份"活的"`WeaponMod`副本用于开火/换弹模拟,两者互不删除/创建对方。切到的肩膀是空的则`weaponComponent->ClearWeapon()`清空手上武器,不会继续保留切换前那把。
- `ACitizenElement`默认关闭Actor Tick,若被玩家占有且需要用到`AForeverCharacter::Tick()`(如瞄准插值),必须在`PossessedBy`里重新打开,且其自身`Tick()`不能在判定"无待处理路径点"时把Tick又关掉——已在`ACitizenElement::PossessedBy`/`Tick()`里用`IsPlayerControlled()`判断规避,见`CONVENTIONS.md`第11节、memory"Citizen Tick disabled by default"。此后任何打算在`AForeverCharacter`子类上使用原生`Tick()`的功能都要确认子类有没有类似的自我关闭逻辑。
- **投掷物**(数字键3，`throwableComponent`)：短按3(松开时触发)`CycleThrowable()`切换；长按3(按住真实时间超过`longPressThreshold`，Tick里检测)弹出`UThrowListWidget`选择列表，见`UI/ThrowListWidget.md`。长按时间必须用`GetRealTimeSeconds`，列表打开时世界慢放到0.1倍，世界时间不可靠。`bSwallowThrowRelease`标记"这次按键已用来弹列表"，随后的松开不当作短按；UI Only下pawn的`Completed`不一定触发，所以该标记在下一次按下时重置，而不是只靠松开清掉。`UnPossessed`时会关闭列表。激活的投掷物与肩上的
  武器互斥：激活时`weaponComponent->ClearWeapon()`，`ActivateShoulderWeapon`开头若投掷物在激活则`Deactivate()`并`StopAim()`。
  `StartAim`在投掷物激活时走自己的分支：不要求`HasWeapon()`，也没有武器的"右手必须空"限制，改用`CanThrow()`
  (拿着它的手就是投掷手，或至少一只手空)；两只手都被别的东西占着时仍保持激活但不能瞄准。左键`StartFireWeapon`
  在投掷物激活时转`Throw()`(一次一个)，不走武器连发。`bIsAiming`与武器共用，`IsAimingThrow()`供HUD区分。
  投掷出去的物品记进`Player::worldAssets`，不会自动消失。`UForeverThrowableComponent`和`UForeverWeaponVisualComponent`是friend,前者需要直接开关`StartAim`/`StopAim`,后者需要调`ActivateShoulderWeapon`。
- **游泳**(`UpdateWaterState`，Tick里每帧调，只对`IsPlayerControlled()`的角色生效；市民走Core导航不下水)：
  - **水体数据**：`QueryWater(世界坐标)`(取到`Map*`后调共用的`ForeverWater::QuerySurface`，`Water/WaterQuery.h`，船的浮力同一份)把厘米坐标除以1000得到地图格，读Core的`Map::GetWater(x,y)`(有没有水+水位，地图单位)和`Map::SampleHeight`(水底高度)，都乘1000换算成世界Z。水面mesh(`ForeverWaterFrameworkComponent`)也是同一套数据，所以判定和画面一致；水面高度=本格水位+`ForeverWater::WaveHeight(x,y,t)`(`Water/WaterWave.h`，和材质里同一表达式)，`QueryWater`同时输出带波浪的`surfaceZ`(浮高、`bIsUnderwater`、潜水上限用)和不带波浪的`baseSurfaceZ`(入水/出水深度阈值用，避免在阈值边缘被浪推着抖)；先判格子范围，因为`GetWater`越界会打警告、这里每帧调。水位逐格定值(河流格子各自不同)，没有水体Actor/`PhysicsVolume`。
  - **地形以下不算在水中**：`QueryWater`先用`ForeverWater::IsBelowTerrain`(`Map::SampleHeight`含1/16细化高度，比较胶囊体中心高度)判断，在地形表面以下(如海底隧道里：格子有水但人在地下)直接返回"不在水中"，不会入水、不会被浮力顶飞；在水面下但在地形以上才是水中。水下后处理的相机判定同一规则。船的浮力(`ABoatElement`)不用这条。
  - **借用`MOVE_Flying`，不用`MOVE_Swimming`**：引擎的`PhysSwimming`每帧检查`GetPhysicsVolume()->bWaterVolume`，没有水体体积就立刻`SetMovementMode(MOVE_Falling)`踢出游泳(`CharacterMovementComponent.cpp`的PhysSwimming末尾)。`MOVE_Flying`无重力、三维移动、没有这个检查，`MaxFlySpeed`/`BrakingDecelerationFlying`控制速度和停止。自己维护`bSwimState`，动画蓝图读`bIsSwimming`而不是`IsSwimming()`/`IsFlying()`。
  - **两个状态**：游泳(浮在水面，不下潜，镜头`ViewPitchMax=0`不能朝上看)和潜水(可上下，镜头全视角)。从地面入水先进游泳；游泳时按住冲刺且视线与竖直向下的点积>`swimDiveLookDot`(0.5)才进潜水；潜水回到水面且不再满足下潜条件时回游泳。`bSwimState`=游泳或潜水，`bDiveState`=潜水。
  - **入水/出水**：`脚底淹没深度 >= swimEnterDepthRatio(0.8)×身高`才入水；游泳(水面)中`水底到水面深度 < swimExitDepthRatio(0.6)×身高`才出水，所以只能从很缓的沙滩坡走上岸，没有跳跃出水。两个比例之间是滞回。出水用`SetMovementMode(MOVE_Falling)`。出水判据用地形水底深度而不是脚底，因为浮在水面时胶囊体离水底很远。
  - **浮高**：胶囊体中心浮在`水面+swimFloatOffset`(默认-60，原地踩水用)；原地/游动同一个高度，两种姿势头的高度差在AnimBP里用Transform (Modify) Bone(root，Component Space的Z)补。游泳状态竖直速度每帧直接补到浮高(一帧到位，不平滑，不会下潜)；潜水时没有输入默认原地悬停(`swimBuoyancySpeed`默认0；>0才以该速度缓慢上浮)，向上速度钳在"到浮高的距离/帧时间"以内，不冲出水面。
  - **操控**：游泳时前进方向只在水平面内；潜水时带视角俯仰(低头下潜)，空格=上浮(游泳状态空格无效)。Shift冲刺=快速游泳(`swimSpeedMultiplier`0.7×`swimSprintMultiplier`2.5×`walkSpeed`)。游泳/潜水时`bOrientRotationToMovement`关掉，偏航由`UpdateSwimBody`自己转(引擎Flying模式会让胶囊体跟加速度俯仰)。
  - **身体俯仰**：胶囊体始终竖直，只转网格(绕胶囊中心)。潜水时俯仰=镜头俯仰×`diveBodyPitchFollow`(0.6)，按住空格上浮再加`diveRiseBodyPitch`(35度)，`FInterpTo`平滑；不是等于镜头方向，也不是全程平的。
  - **给动画蓝图的变量**(蓝图里Cast成`AForeverCharacter`)：`bIsSwimming`(游泳或潜水都为真)、`bIsDiving`(仅潜水)、`bIsSwimMoving`(游泳/潜水且有移动输入，动画蓝图可用它在游动/踩水间切)、`bIsUnderwater`(头部——`head`socket，没有则胶囊顶——低于水面)、`bIsFastSwimming`；速度从移动组件的`Velocity`取。游泳动画(Mixamo重定向的`Swimming`/`Treading_Water`，流程见`Tools/MixamoRetarget/README.md`)本身带趴姿，胶囊体保持竖直、不旋转网格；中心高度对不上就调`swimFloatOffset`。
  - **未做**：氧气/溺水、水下相机碰撞和后处理(雾/色调/焦散)、水花粒子和音效。
- 蹲下(C键)和挥拳(左键)未实现:蹲姿需要在AnimGraph加`Blend Poses by bool`才能平滑过渡,效果不理想,已放弃。

### 头像方块

没有角色资产，所有角色(玩家和全部NPC，`ACitizenElement`也继承这个类)都是同一个小白人，所以每个角色头上
放一个贴头像的方块来区分：`avatarMesh`(UStaticMeshComponent)挂在`head`骨骼socket上，和
`firstPersonCamera`同一种挂法(没有head时退化成挂在胶囊体顶部)，跟着走路姿势一起动。方块用项目的
`/Game/Asset/Common/Mesh/Cube.Cube`(原生边长100，缩放=`avatarSize`/100)，不参与碰撞/overlap/导航。
`BeginPlay`里重新摆一次：`avatarOffset`是相对head骨骼的偏移(骨骼局部坐标)，`avatarOffsetInCharacter`是在它基础上按角色方向
(+X前+Y右+Z上)的微调(当前往下5cm盖住下巴，按骨骼参考姿势转回骨骼坐标再叠加)，`avatarSize`默认30cm；
旋转按head骨骼的参考姿势(`FAnimationRuntime::GetComponentSpaceTransformRefPose`+mesh相对旋转)把骨骼
自己的朝向抵消掉，`avatarRotation`就变成相对角色本身的旋转——0时方块+X面朝角色正前方(正面)、+Z朝上，
之后仍跟着head骨骼的动画一起动。然后再用`/Game/Asset/Common/Material/TextureStatic`建动态材质实例，把`avatarTexture`(现在所有人
都是硬编码的`/Game/Asset/Characters/Avatar/default`)设进`Texture`参数。TextureStatic材质需要在编辑器里
手动建(一个TextureSampleParameter2D `Texture`连Base Color)；还没建时保留Cube自带材质。已知效果：
Cube每个面都是0-1 UV，6个面都显示头像；第一人称相机在方块内部，Cube单面渲染、从里面看不到，不挡视线。

**按角色换贴图**：`SetAvatarTexture(UTexture2D*)`把贴图存进`avatarTexture`(UPROPERTY，运行时建的贴图靠它存活)并更新动态材质的`Texture`参数；动态材质实例`avatarMaterial`只建一次，`PlaceAvatar`(换身体网格时重复调用)沿用它和当前贴图。市民的头像由`ACitizenElement::Init`通过`UForeverAvatarSubsystem`设置，见`ForeverAvatarSubsystem.md`。

- **存档**：`activeShoulderSlot`记录最近一次`ActivateShoulderWeapon`的槽位；读档时`AForeverFrameworkActor::RestoreControlledPawn`调用`RestoreWeaponState(slot, ammo)`改回存档时激活的武器和弹匣余量(`PossessedBy`默认激活左肩、换满弹匣)。
- `ActivateShoulderWeapon`装备成功后绑定槽位到武器组件并恢复该Asset的`magazineAmmo`(没用过=满弹匣)，见`ForeverWeaponComponent.md`。

## 依赖关系
- 依赖资产:`Content/Asset/Characters/Mannequins/Meshes/SKM_Manny_Simple`、`SKM_Quinn_Simple`、`Anims/Unarmed/ABP_Unarmed`、`Content/Blueprint/Player/Input/Actions/{IA_Move,IA_Look,IA_MouseLook}`、`Content/Blueprint/Player/Input/{IMC_Default,IMC_MouseLook}`。
- 依赖`Input/ForeverKeyBindingSubsystem`提供的动态`UInputAction`(`Jump`/`ToggleView`/`Sprint`/`MeetOptionUp`/`MeetOptionDown`/`MeetOptionSelect`/`FireWeapon`/`ReloadWeapon`/`SwitchWeapon1`/`SwitchWeapon2`/`AimWeapon`)。
- 依赖`Source/Core/player/player.h`(`Player::CreateAsset`/`AddByPath`/`GetByPath`/`GetLeftShoulder`/`GetRightShoulder`/`GetRightHand`/`SetCurrentRoom`)、`Framework/ForeverFrameworkActor.h`(反查`Player`实例)。
- 依赖`AForeverPlayerController::GetMeetOptionWidget()`(MeetOption三个处理函数转发)。
- 持有`UForeverWeaponComponent`/`UForeverWeaponVisualComponent`/`UForeverThrowableComponent`子对象(见各自的md)。
- 被`ACitizenElement`继承(玩家可操控的市民走同一套输入/武器/瞄准逻辑)。

## 补充说明
- 第一人称下网格OwnerNoSee，`ToggleCameraView`把`VisibilityBasedAnimTickOption`强制成`AlwaysTickPoseAndRefreshBones`：否则动画被当成"没渲染"而停更，第一人称相机(挂head骨骼)1~2秒后不再随步伐晃，瞄准的武器(挂胶囊体)与镜头就变成相对静止。
- **相机**：第三人称弹簧臂连接点只在游泳/潜水时从胶囊体中心抬到头部(`cameraBoomHeight`默认70cm，`EnterSwimming`加上、`ExitSwimming`恢复到BeginPlay记下的原位置)。第一人称相机每帧在head骨骼上加`firstPersonCameraRaise`(默认15cm，世界向上，转成骨骼局部坐标)。
- **游泳时缩短胶囊体**：竖直的站立胶囊体下端在水面下约`-swimFloatOffset+半高`(≈156cm)，比出水深度(0.6身高≈115cm)深，会在能出水之前就顶住岸坡。`EnterSwimming`把胶囊体缩成`swimCapsuleHalfHeight`(默认45，半径不变，以中心为基准，网格/相机/浮高不动)；`ExitSwimming`恢复站立尺寸并同时上移半高差值，保持下端位置不变，免得嵌进岸坡(出水时会有一点上移)。入水/出水阈值用BeginPlay缓存的站立身高，不读当前胶囊体。
- **入水阻力**：引擎Flying模式摩擦很小，冲刺入水(速度可达几千厘米/秒)会滑很久。游泳状态下水平速度超过`MaxFlySpeed`的部分按`swimOverspeedDecay`(默认8/秒，指数衰减)快速衰减，约半秒回到游泳速度。
