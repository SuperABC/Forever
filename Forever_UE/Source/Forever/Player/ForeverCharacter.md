# ForeverCharacter

## 职责
玩家可控的第三人称角色。持有肩后摄像机(SpringArm+Camera),用Enhanced Input绑定移动/视角/跳跃。

## 关键设计
- 不建Blueprint外壳:构造函数里用`ConstructorHelpers::FObjectFinder`/`FClassFinder`硬编码引用`SKM_Manny_Simple`骨骼网格、`ABP_Unarmed`动画蓝图和`IA_Jump`/`IA_Move`/`IA_Look`/`IA_MouseLook`四个Input Action作为默认值。这些都是`EditDefaultsOnly`,子类或运行时代码随时可以覆盖,不影响后续换资产。
- `lookAction`(IA_Look)和`mouseLookAction`(IA_MouseLook)都绑定到同一个`Look()`处理函数,但分属`IMC_Default`和`IMC_MouseLook`两个不同的Input Mapping Context——手柄摇杆(模拟量)和鼠标位移(增量)需要不同的Input Modifier配置,拆成两个IMC/IA是旧工程既有的做法,沿用之。**这不是可选项**:少绑`mouseLookAction`会导致WASD和跳跃正常但鼠标转视角完全没反应(阶段0实测踩过这个坑)。
- 网格体相对Root的`(-96, 0, -90°)`偏移是标准UE Mannequin的挂接惯例(胶囊体中心到脚底、朝向对齐)。
- 视角切换(V键第一/三人称)不在本类实现,留给阶段1。

## 依赖关系
- 依赖资产:`Content/Asset/Characters/Mannequins/Meshes/SKM_Manny_Simple`、`Anims/Unarmed/ABP_Unarmed`、`Content/Blueprint/Player/Input/Actions/{IA_Jump,IA_Move,IA_Look,IA_MouseLook}`(均从旧工程拷贝而来)。
- 被`AForeverGameMode`的`DefaultPawnClass`引用。

## 待办/后续阶段
- 阶段1:加V键第一/三人称切换、配置文件驱动的按键绑定(替换掉当前硬编码的IA资产路径查找方式,改为可配置)。
- 阶段6:角色外观(骨骼网格/动画蓝图)改为运行时按所控制的NPC数据动态设置,不再依赖构造函数里的默认值。
