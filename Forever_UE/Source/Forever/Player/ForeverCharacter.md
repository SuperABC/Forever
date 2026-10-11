# ForeverCharacter

## 职责
玩家可控角色基类,被玩家自己的初始角色和`ChangeControlChange`切换过去操控的任意`ACitizenElement`共用。支持第三人称(肩后SpringArm+Camera)/第一人称(头部或眼高Camera)切换、移动/跳跃/冲刺、瞄准开火(转发给`UForeverWeaponComponent`,见`ForeverWeaponComponent.md`)、MeetOption对话选项操作。

## 关键设计
- 不建Blueprint外壳:构造函数用`ConstructorHelpers::FObjectFinder`/`FClassFinder`引用`SKM_Manny_Simple`骨骼网格、`ABP_Unarmed`动画蓝图和`IA_Move`/`IA_Look`/`IA_MouseLook`三个Input Action、`IMC_Default`/`IMC_MouseLook`两个Mapping Context作为默认值,均为`EditDefaultsOnly`,可被覆盖。
- `lookAction`(IA_Look)和`mouseLookAction`(IA_MouseLook)都绑到同一个`Look()`处理函数,但分属`IMC_Default`/`IMC_MouseLook`两个Context——手柄摇杆(模拟量)和鼠标位移(增量)需要不同的Input Modifier配置。两者都必须绑定,缺`mouseLookAction`会导致鼠标转视角没反应。
- 网格体相对Root的`(-96, 0, -90°)`偏移是标准UE Mannequin挂接惯例。
- `Jump`/`ToggleView`/`Sprint`/`MeetOptionUp/Down/Select`/`FireWeapon`/`ReloadWeapon`/`SwitchWeapon1/2`/`AimWeapon`不直接引用`UInputAction`资源,改从`UForeverKeyBindingSubsystem`(见`Input/ForeverKeyBindingSubsystem.md`)按名字取运行时动态创建的Action,按键从外部配置文件读取。
- `PossessedBy`/`UnPossessed`只增删这个Pawn自己的`inputMapping`/`inputLookMapping`(`IMC_Default`/`IMC_MouseLook`)——`UForeverKeyBindingSubsystem`那份动态Context改由`AForeverPlayerController::BeginPlay`常驻添加一次(见`ForeverPlayerController.md`),因为`TogglePhone`等热键要求任何时候都生效,不依赖"当前是否正占有一个真正的Pawn"。移动/视角这类Pawn专属映射仍然跟着"被占有"状态走,保证玩家上/下车、切换被操控角色时映射能跟着当前Pawn切换(见`CONVENTIONS.md`第7节)。
- `PossessedBy`里会给被占有的角色重新配出一套初始装备:左肩挂`weapon_pistol`、右肩挂`weapon_rifle`并激活左肩(`ActivateShoulderWeapon(1)`),背上挂一个`asset_container`,内放手枪/步枪备弹各120发——每次占有前先清空`leftShoulder`/`rightShoulder`/`back`三个槽位上一次占有可能留下的旧Asset,这样切到任意可操控角色都能立刻测装备/换弹/切枪,但同一角色被取消占有又重新占有时装备不会保留,这是已知简化。
- **V键第一/三人称切换**(`ToggleCameraView`):
  - `firstPersonCamera`优先挂到骨骼网格`head`socket;不存在则退化为挂在Root上、用胶囊体半高0.9倍作为眼高近似值。
  - 两个摄像机同一时刻只有一个`Active`(`UCameraComponent::SetActive`),不是靠`SetViewTarget`,因为两者都挂在同一Pawn上。
  - 切第一人称时`GetMesh()->SetOwnerNoSee(true)`避免本地玩家看到自己身体穿模;`bUseControllerRotationYaw`/`CharacterMovement.bOrientRotationToMovement`由`UpdateRotationMode()`统一切换。
- **Shift冲刺**(`StartSprint`/`StopSprint`,按住触发):`MaxWalkSpeed`在`walkSpeed`(默认500)和`walkSpeed * sprintSpeedMultiplier`(默认10)之间切换,冲刺速度是走路速度的倍数,改`walkSpeed`会联动。无专属冲刺动画,`BS_Idle_Walk_Run`按速度采样,超出原本"Run"档位设定的速度上限后画面会有轻微脚下打滑感,是动画资产本身的限制。
- **瞄准**(`StartAim`/`StopAim`,默认右键):手上没有激活武器(`weaponComponent->HasWeapon()`为false)或Player的右手(`GetRightHand()`)正攥着背包物品时拒绝瞄准。复用`cameraBoom`(`USpringArmComponent`)现有插值,不新增第三个摄像机:瞄准目标`aimSocketOffset`/`aimArmLength`,取消瞄准回到`defaultSocketOffset`/`defaultArmLength`,`Tick()`里用`FMath::VInterpTo`/`FInterpTo`每帧推进(插值速度`aimTransitionSpeed`)保证过渡平滑。只对第三人称生效。瞄准时角色朝向跟随摄像机而非移动方向:`UpdateRotationMode()`统一计算`bFaceCamera = bIsFirstPerson || bIsAiming`,`ToggleCameraView`/`StartAim`/`StopAim`都调用这个函数而非各自维护判断。
- **数字键切枪**(`ActivateShoulderWeapon(slot)`):激活左肩(1)/右肩(2)当前挂着的武器Asset(若有),武器Asset本身留在肩膀槽位不动,`weaponComponent->EquipWeapon()`只是让组件额外持有一份"活的"`WeaponMod`副本用于开火/换弹模拟,两者互不删除/创建对方。切到的肩膀是空的则`weaponComponent->ClearWeapon()`清空手上武器,不会继续保留切换前那把。
- `ACitizenElement`默认关闭Actor Tick,若被玩家占有且需要用到`AForeverCharacter::Tick()`(如瞄准插值),必须在`PossessedBy`里重新打开,且其自身`Tick()`不能在判定"无待处理路径点"时把Tick又关掉——已在`ACitizenElement::PossessedBy`/`Tick()`里用`IsPlayerControlled()`判断规避,见`CONVENTIONS.md`第11节、memory"Citizen Tick disabled by default"。此后任何打算在`AForeverCharacter`子类上使用原生`Tick()`的功能都要确认子类有没有类似的自我关闭逻辑。
- 蹲下(C键)和挥拳(左键)未实现:蹲姿需要在AnimGraph加`Blend Poses by bool`才能平滑过渡,效果不理想,已放弃。

## 依赖关系
- 依赖资产:`Content/Asset/Characters/Mannequins/Meshes/SKM_Manny_Simple`、`Anims/Unarmed/ABP_Unarmed`、`Content/Blueprint/Player/Input/Actions/{IA_Move,IA_Look,IA_MouseLook}`、`Content/Blueprint/Player/Input/{IMC_Default,IMC_MouseLook}`。
- 依赖`Input/ForeverKeyBindingSubsystem`提供的动态`UInputAction`(`Jump`/`ToggleView`/`Sprint`/`MeetOptionUp`/`MeetOptionDown`/`MeetOptionSelect`/`FireWeapon`/`ReloadWeapon`/`SwitchWeapon1`/`SwitchWeapon2`/`AimWeapon`)。
- 依赖`Source/Core/player/player.h`(`Player::CreateAsset`/`AddByPath`/`RemoveByPath`/`GetLeftShoulder`/`GetRightShoulder`/`GetRightHand`)、`Framework/ForeverFrameworkActor.h`(反查`Player`实例)。
- 依赖`AForeverPlayerController::GetMeetOptionWidget()`(MeetOption三个处理函数转发)。
- 持有`UForeverWeaponComponent`子对象(见`ForeverWeaponComponent.md`)。
- 被`ACitizenElement`继承(玩家可操控的市民走同一套输入/武器/瞄准逻辑)。

## 待办/后续阶段
- 角色外观(骨骼网格/动画蓝图)改为运行时按所控制的NPC数据动态设置,目前仍是构造函数默认值。
