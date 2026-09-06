# ForeverPlayerController

## 职责
第三人称玩家的PlayerController,负责把默认Input Mapping Context加进Enhanced Input子系统。

## 关键设计
- 不建Blueprint外壳,构造函数用`ConstructorHelpers::FObjectFinder`硬编码引用`IMC_Default`和`IMC_MouseLook`两个mapping context。两个都要加——`IMC_Default`只覆盖手柄摇杆等模拟量输入,鼠标视角走的是`IMC_MouseLook`(参见`ForeverCharacter.md`里的说明),漏掉会导致鼠标转视角没反应。
- 只在`IsLocalPlayerController()`为真时添加mapping context,避免服务端/远端controller重复处理。
- 省去了旧工程里的移动端触屏控件(`MobileControlsWidgetClass`等)相关逻辑——当前没有移动端支持计划,按"不为假设的未来需求设计"原则先不加。

## 依赖关系
- 依赖资产:`Content/Blueprint/Player/Input/{IMC_Default,IMC_MouseLook}`(从旧工程拷贝而来)。
- 被`AForeverGameMode`的`PlayerControllerClass`引用。

## 待办/后续阶段
- 阶段1:配置文件驱动的按键绑定框架落地后,`defaultMappingContexts`的填充方式可能需要调整为从配置读取。
