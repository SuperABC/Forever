# ForeverPlayerController

## 职责
第三人称玩家的PlayerController。目前是空壳——不做Enhanced Input映射管理,也没有其他逻辑。

## 关键设计
- 阶段0曾经在这里加`IMC_Default`/`IMC_MouseLook`的Mapping Context,阶段1核对`Blueprint/Player/MainCharacter`的dump后发现旧项目并不是这么做的:输入映射的增删挂在**被占有的Pawn**(`AForeverCharacter::PossessedBy`/`UnPossessed`)上,不是固定挂在Controller上。原因是旧项目要支持"玩家上/下车时从控制Character切到控制Vehicle Pawn"——每次换Pawn,映射要跟着走。详见`ForeverCharacter.md`。所以这里改回空壳,不再持有`defaultMappingContexts`。
- 保留这个类(而不是直接用引擎默认`APlayerController`)是因为dump显示旧项目的`MainController`蓝图EventGraph里有一大块热键逻辑(Tab开暂停菜单、M开地图、B开背包等),这些是阶段4/5系统就绪后才能迁移的内容,届时会加回这个类里。当前这些热键**没有实现**,详见仓库根目录`MAINCONTROLLER_TODO.md`。

## 依赖关系
- 被`AForeverGameMode`的`PlayerControllerClass`引用。

## 待办/后续阶段
- 阶段4/5:按`MAINCONTROLLER_TODO.md`里列的清单,把旧`MainController`蓝图里各个热键对应的功能迁移进来(每项都要等它依赖的系统/UI先就绪)。
