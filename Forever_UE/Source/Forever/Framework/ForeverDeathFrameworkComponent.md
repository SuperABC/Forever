# ForeverDeathFrameworkComponent.h / ForeverDeathFrameworkComponent.cpp

## 职责

`UForeverDeathFrameworkComponent`(挂在`AForeverFrameworkActor`上，`GetDeathFramework()`)：玩家扮演的市民被打死之后的整个流程——
慢镜头+渐暗 → 结算界面(继续游戏/退出游戏) → 继续游戏跳15天 → 在复活点重新操控。NPC死亡不经过这里。

## 流程

1. **接管**：`FForeverDamage::DamageCitizen`把市民打死、发完`citizen_decease`后调`HandlePlayerDeath`。死者是`GetPlayerCitizen()`(开车时是车里藏着的那个)才接管，
   返回true时调用方**不撤Actor**(尸体留着演慢镜头)。接管时：
   - `AForeverPlayerController::CloseGameplayPanels`关掉手机/背包/地图/投掷物列表/识别模式；Pawn上的按键`DisableInput`；Controller上的热键按`IsPlayerDead()`屏蔽。
   - Pawn就是死者时`AForeverCharacter::BeginDeathPresentation`：第一人称切第三人称、布娃娃、镜头拉远绕圈、褪色(见`Player/ForeverCharacter.md`"死亡镜头")。开车死的只有慢镜头和渐暗。
   - 全局时间缩放=`slowMotionDilation`(0.2)，`UDeathWidget::BeginDeath`。
   - 发`player_decease`(给所有脚本含主线，字段同citizen_decease)。
2. **Dying**：按真实时间(`FApp::GetDeltaTime`)走`slowMotionDuration`(3秒)，进度喂给`UDeathWidget::SetProgress`(渐暗+标题渐显)。
3. **Menu**：时间缩放置0(世界停住，同暂停界面)，`ShowMenu`出现两个按钮、UI Only输入。没配死亡界面时直接走"继续游戏"，不让玩家卡死。
   - **退出游戏**：`UPausePanelWidget::ExitToMainMenu`，和暂停界面的退出按钮同一个函数(不存档，回主菜单)。
   - **继续游戏**：`HideMenu`、时间缩放还原1，`BeginTimeSkip(现在+reviveSkipDays天, 回调Revive)`，跳不了(比如正在跳别的时间)就直接`Revive`。
4. **Reviving**：跳时间的黑屏完全盖住(`UFadeSubsystem::IsHeldBlack`)后撤尸体(`HandleCitizenDeath`，平时市民死亡的那一步推迟到这里)、收起死亡界面。
   跳的过程中玩家没有Pawn，`GetPlayerCitizen`退回`GetDeadCitizen()`，Core照样保护这个人(不离城/不自然死亡/不搬家)。
5. **Revive**(`FinishTimeSkip`里、黑屏渐隐之前调)：按名字重新找到市民；跳时间期间脚本可能已经把他复活并放到别处(市民Scheduler的`npc_revive`+`npc_teleport`)，
   所以先撤掉他现有的Actor，没复活就`Citizen::Revive`(满血)，清掉房间/绿化/行走，位置设成`Map::PickJunctionRespawn`(随机路口的人行道转角)，
   再`UForeverStoryFrameworkComponent::PossessCitizenByName`(和change_control同一套：生成Actor、Possess、激活动态属性、冻结世界等建筑LOD)。
   地图上没有路口就在家里复活。最后发`player_revive`(name、location=复活地址)。

## 关键设计

- 时间全按真实时间算，世界被慢放/停住时照样推进；组件`bTickEvenWhenPaused`。
- 结算界面期间世界停住：脚本计时器(包括市民Scheduler死后4秒复活自己的那种)、市民、交通都不走，尸体定格。
- 复活点现在是随机路口——**待办(TODO.md)**：有医院以后，城里没有医院就不能复活，有医院在医院门口复活。
- 参数(`slowMotionDilation`/`slowMotionDuration`/`reviveSkipDays`)是`EditAnywhere`，可以在FrameworkActor的组件细节里改。

## 依赖关系

- 依赖：`AForeverFrameworkActor`(`BeginTimeSkip`的完成回调、`GetPlayerCitizen`、`DescribeLocation`)、`UForeverPopulaceFrameworkComponent::HandleCitizenDeath`、
  `UForeverStoryFrameworkComponent`(`BroadcastToEveryone`/`PossessCitizenByName`)、`AForeverPlayerController`(`GetDeathWidget`/`CloseGameplayPanels`)、
  `UI/DeathWidget`、`UI/FadeSubsystem`、`UI/PausePanelWidget::ExitToMainMenu`、Core `Map::PickJunctionRespawn`、`story/event.h`(`PlayerDeceaseEvent`/`PlayerReviveEvent`)。
- 被谁依赖：`Player/ForeverDamage`(`HandlePlayerDeath`)、`AForeverPlayerController`(`IsPlayerDead`)、`UI/DeathWidget`(按钮)、`AForeverFrameworkActor::GetPlayerCitizen`。
