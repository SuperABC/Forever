# ForeverKeyBindingSubsystem

## 职责
把"数字按键类"输入动作(Jump、ToggleView等单键触发的动作)的按键绑定从C++/资源里解放出来,改为读取外部文本配置文件。是需求#9(配置文件驱动按键绑定)的落地。

## 关键设计
- 只覆盖**数字单键动作**,不覆盖Move/Look这类模拟量+多按键组合映射(WASD四向、鼠标XY)——这类映射的Modifier/Trigger设置复杂,继续放在`IMC_Default`/`IMC_MouseLook`资源里由编辑器authored,不在本次配置化范围内。阶段1明确"暂不做交互式设置UI",这里选择了性价比最高的覆盖范围。
- 动作(`UInputAction`)和映射(`UInputMappingContext`)都是`NewObject`出来的**运行时对象**,不对应内容浏览器里的任何`.uasset`。好处是新增/修改按键完全不需要在编辑器里碰资源,坏处是这些动作在编辑器里不可见、不能被蓝图或Details面板直接引用,只能通过`GetAction(FName)`按名字取。
- 配置文件路径是`Forever_UE/Resource/Config/KeyBindings.ini`,不在UE标准`Project/Config`目录下,所以用`FConfigFile::Read`直接读文件内容,不走`GConfig`全局单例注册(避免污染引擎的配置系统,也符合"资源目录调整"约定里非UE资源放`Resource/`的规则)。
- 键名解析用`FKey(FName(keyNameString))` + `IsValid()`校验;缺配置项或键名无效都静默回退到硬编码默认键并打`Warning`日志,不会导致某个动作彻底失效或游戏崩溃。
- `UInputMappingContext::MapKey`每次调用都是"新增一条映射"而不是"查找并更新已有映射"(引擎源码里就是`Mappings.Emplace(...)`),所以在全新构造的`bindingContext`上直接为每个动作调用一次即可,不用担心覆盖/查找逻辑。

## 依赖关系
- 依赖:`Forever_UE/Resource/Config/KeyBindings.ini`(找不到该文件时全部用硬编码默认值,不报错)。
- 被谁依赖:`AForeverCharacter::PossessedBy`(取`GetBindingContext()`加入Enhanced Input子系统——阶段1后期核对`MainCharacter`蓝图dump后,输入映射的增删从Controller挪到了被占有的Character上,详见`Player/ForeverCharacter.md`,`AForeverPlayerController`现在不碰这个子系统)、`AForeverCharacter::SetupPlayerInputComponent`(取`GetAction(TEXT("Jump"))`/`GetAction(TEXT("ToggleView"))`/`GetAction(TEXT("Sprint"))`绑定具体处理函数)。
- Mod可扩展点:当前"可绑定动作"注册表(`GetBindingDefinitions()`)是硬编码在本类里的静态列表(目前有`Jump`/`ToggleView`/`Sprint`三项),后续阶段若有新的数字按键动作,在这里加一条`{名字, 默认键}`即可,不需要改配置文件解析逻辑。

## 待办/后续阶段
- 若后续阶段(交互式设置UI)需要让玩家在游戏内改键并持久化,需要在这个类基础上加"运行时改键+写回配置文件"的能力,当前只有"启动时读取一次"。
- Move/Look的模拟量组合按键配置化,留待有实际需求(如交互式设置UI)时再设计,不在本阶段范围内。
