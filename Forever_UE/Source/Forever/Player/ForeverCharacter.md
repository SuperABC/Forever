# ForeverCharacter

## 职责
玩家可控的角色,支持第三人称(肩后SpringArm+Camera)和第一人称(挂在头部或眼高的Camera)切换。用Enhanced Input绑定移动/视角/跳跃/视角切换。

## 关键设计
- 不建Blueprint外壳:构造函数里用`ConstructorHelpers::FObjectFinder`/`FClassFinder`硬编码引用`SKM_Manny_Simple`骨骼网格、`ABP_Unarmed`动画蓝图和`IA_Move`/`IA_Look`/`IA_MouseLook`三个Input Action作为默认值。这些都是`EditDefaultsOnly`,子类或运行时代码随时可以覆盖,不影响后续换资产。
- `lookAction`(IA_Look)和`mouseLookAction`(IA_MouseLook)都绑定到同一个`Look()`处理函数,但分属`IMC_Default`和`IMC_MouseLook`两个不同的Input Mapping Context——手柄摇杆(模拟量)和鼠标位移(增量)需要不同的Input Modifier配置,拆成两个IMC/IA是旧工程既有的做法,沿用之。**这不是可选项**:少绑`mouseLookAction`会导致WASD和跳跃正常但鼠标转视角完全没反应(阶段0实测踩过这个坑)。
- 网格体相对Root的`(-96, 0, -90°)`偏移是标准UE Mannequin的挂接惯例(胶囊体中心到脚底、朝向对齐)。
- **Jump不再直接引用`IA_Jump`资源**:阶段1把Jump和新增的ToggleView都改成从`UForeverKeyBindingSubsystem`(见`Input/ForeverKeyBindingSubsystem.md`)按名字取运行时动态创建的`UInputAction`,按键从外部配置文件读取。`IA_Jump`资源本身还留在`Content/`里但已无代码引用,先不删除,待后续清理。
- **输入映射Context跟着"被占有"状态走,不固定挂在Controller上**:`PossessedBy`/`UnPossessed`里增删`inputMapping`/`inputLookMapping`(对应`IMC_Default`/`IMC_MouseLook`)以及`UForeverKeyBindingSubsystem`的动态Context,是照旧蓝图`MainCharacter`的`ReceivePossessed`/`ReceiveUnpossessed`原样迁移的(对应资源在dump里叫"Input Mapping"/"Input Look Mapping"变量)。**这不是可选的架构偏好**:用户确认旧项目这么做是为了支持"玩家上/下车时从控制Character切到控制Vehicle Pawn再切回来"——每次切换被控制的Pawn,当前Pawn的输入映射要能干净地卸载,下一个被控制的Pawn要能带上自己的映射,固定在Controller上就做不到这点。同样的道理也覆盖阶段6"任意NPC可被玩家操控"的换人需求。阶段7做车辆时,车辆Pawn类要照这个模式实现自己的`PossessedBy`/`UnPossessed`,而不是假设Controller会处理。
  - `AddMappingContext`/`RemoveMappingContext`没有显式传`FModifyContextOptions`——旧蓝图传的`(bIgnoreAllPressedKeysUntilRelease=True,bForceImmediately=False,bNotifyUserSettings=False)`和UE5.7里`FModifyContextOptions`的默认构造值完全一致,所以直接用默认参数,不用手工拼一份一样的值。
- **V键第一/三人称切换**(`ToggleCameraView`):
  - `firstPersonCamera`构造时优先挂到骨骼网格的`head`socket(`GetMesh()->DoesSocketExist(TEXT("head"))`);没有该socket则退化为挂在Root上、用胶囊体半高的0.9倍作为眼高的近似值——这是为了避免对具体骨骼网格的socket命名做强假设,后续换装/换骨骼资产时也能有一个不崩溃的兜底效果,实际观感需要在PIE里核实。
  - 两个摄像机同一时刻只有一个`Active`(`UCameraComponent::SetActive`),不是靠`SetViewTarget`——因为两个摄像机都挂在同一个Pawn上,引擎会自动选取"活跃"的那个作为视图目标。
  - 切第一人称时`GetMesh()->SetOwnerNoSee(true)`,避免本地玩家在第一人称视角下看到自己身体穿模;`bUseControllerRotationYaw`和`CharacterMovement.bOrientRotationToMovement`两组标志位在第一/三人称间对调,让第一人称下角色朝向跟随视角、第三人称下角色朝向跟随移动方向(和大多数第三人称射击/动作游戏的惯例一致)。
  - 已知限制:`firstPersonCamera`没有挂到`head`socket时的兜底位置(眼高近似值)是构造函数里算一次的固定`RelativeLocation`,不会随角色姿态动态调整,先记录。
- **Shift冲刺**(`StartSprint`/`StopSprint`,按住触发,不是切换):把`CharacterMovementComponent->MaxWalkSpeed`在`walkSpeed`(默认500)和`walkSpeed * sprintSpeedMultiplier`(倍数默认3)之间切换——冲刺速度是走路速度的倍数而不是独立的固定值,改`walkSpeed`时冲刺速度会跟着联动。
  - **没有专属冲刺动画**:`Anims/Unarmed/`目录下没有区别于Jog的Sprint动画集,所以冲刺目前只是数值变化——角色跑得更快,但`BS_Idle_Walk_Run`这个BlendSpace是按速度采样的,超出它原本给"Run"档位设定的速度上限后画面上会有一点脚下打滑感,是这套动画资产本身的限制,不是bug。
- **蹲下(C键)和挥拳(左键)最终没有做**:两者都试过——挥拳方案(运行时`PlaySlotAnimationAsDynamicMontage`接现有`DefaultSlot`)本身没问题,但蹲姿要做出平滑过渡得改AnimGraph加`Blend Poses by bool`节点,操作繁琐、效果也不理想,用户决定放弃,两部分代码和阶段1额外导入的`Anims/Unarmed/Crouch/*`动画资产、`UForeverAnimInstance`类都已经移除,`ABP_Unarmed`也恢复成了原始的`AnimInstance`父类。

## 依赖关系
- 依赖资产:`Content/Asset/Characters/Mannequins/Meshes/SKM_Manny_Simple`、`Anims/Unarmed/ABP_Unarmed`、`Content/Blueprint/Player/Input/Actions/{IA_Move,IA_Look,IA_MouseLook}`、`Content/Blueprint/Player/Input/{IMC_Default,IMC_MouseLook}`(均从旧工程拷贝而来;后两个是阶段1从`ForeverPlayerController`移过来的引用,详见`ForeverPlayerController.md`)。
- 依赖`Input/ForeverKeyBindingSubsystem`提供的`Jump`/`ToggleView`/`Sprint`动态`UInputAction`及其Mapping Context。
- 被`AForeverGameMode`的`DefaultPawnClass`引用。

## 待办/后续阶段
- 阶段6:角色外观(骨骼网格/动画蓝图)改为运行时按所控制的NPC数据动态设置,不再依赖构造函数里的默认值。
