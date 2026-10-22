# puzzle_wxdj

## 职责

五行斗诀回合制战斗小游戏(`WxdjPuzzle`，id `puzzle_wxdj`)：玩家(下标0)对售货员(下标1)，玩家点按钮/招式出招，
敌方AI随机出招，战报滚动，一方阵亡后弹出结果遮罩，点"关闭"结束。战斗规则全在`battle/`，这里只负责画面和鼠标交互。

## 界面布局

- 顶部：双方状态条(MP/ATK/HP 自上而下)。
- 下方左半：战报面板，**从最新一条往上画**，画到面板顶为止，保证最新战报始终可见。
- 下方右半：操作栏 + 招式list。操作栏第一行`单招`/`双招`(模式单选)，第二行`积攒真气炸弹`/`释放真气炸弹(已积攒N)`/`跳过`；
  其下一行提示文字，再下面是招式list，右侧贴滚动条。

## 五种操作

| 操作 | 交互 |
|---|---|
| 跳过 | 点`跳过` → `SkipAction` |
| 单招 | 模式=单招，点list一行立即出招 → `GetAction(i)` |
| 双招 | 模式=双招，依次点两行(可点同一行两次)，第二次点击提交 → `GetAction(i,j)`；切换模式/再点模式按钮取消已选 |
| 积攒真气炸弹 | 点按钮，无消耗，始终可点 → `AccumulateAction` |
| 释放真气炸弹 | 点按钮；已积攒为0时置灰 → `BombAction`(威力在`Controller::ActionTurn`里由`Cultivator::BombPower()`取走并清零) |

list里真气不足的行置灰且不可点；双招模式选了第一招后，"第一招+该行"的双招消耗(`0.75*(p1+p2)`，和`DualAction`构造一致)
超过当前真气的行也置灰；还没选第一招时，用"该行配上最便宜的一招"判断能不能选。

## 关键设计

- **控件统一**：每帧`RebuildWidgets`生成一份`Widget{id, rect, enabled}`列表，命中测试(`HitTest`，后加入的优先)、
  绘制、点击激活(`Activate`)共用，避免两套坐标。招式行id = `ID_ROW_BASE + 招式下标`。
- **按钮状态**：悬停(`hoverId`，取`GetMouseState`位置)变亮(+40)、按下(`pressedId==自己且仍悬停`)变暗(×0.65)、
  抬起恢复；禁用压灰且不响应。**点击在左键抬起时触发**，且抬起时必须仍在按下的那个控件上，按住拖出按钮再松开不触发。
  `Loop`末尾发现左键已松开(比如在窗口外松的)就清`pressedId`，避免卡在按下状态。
- **list按整行滚动**：Canvas没有裁剪区域，超出矩形的内容无法截断，所以`scrollOffset`以行为单位，只生成/绘制完整可见的行。
  滚轮(`Canvas::ConsumeMouseWheel`)在右半面板内一刻度滚一行；滚动条滑块可按住拖动(拖动期间只看鼠标y，不要求仍在滑块上)，
  点轨道空白处往点击方向翻一页；行数不超过可见行数时不画滚动条。
- **招式数据走post查路径**：`Init`向宿主发`{"post":"resource path","name":"xiaohua.txt"}`(Core的`PostImplement`)，
  `xiaohua.txt`放在`Wxdj/Resource/act/`。post为空或加载失败时`loadFailed`置位，`Loop`画一行"招式数据加载失败"并返回-1。
- **状态全是实例成员**：宿主每次`start_puzzle`都`new`一个新实例，没有static状态。`GetName()`用`static int count`+`id`拼唯一名，
  `count`通过`GetInstanceCounter`/`SetInstanceCounter`进存档。
- **Canvas没有自动折行**：`WrapText`按UTF-8码点累加`StringWidth`折行，不劈开多字节字符。
- **敌方AI**：3/4概率出单招(下标范围`ac+6`含积攒/炸弹等特殊选项)，1/4概率出双招，原样保留老工程逻辑。
- **阵亡判定**：玩家阵亡(含同归于尽)判负，只有敌方阵亡才判胜。
- 数值：玩家HP1200/ATK1300/MP1800(敏捷22、回真气45)，售货员1000/1100/1600(20/40)。

## 依赖关系

- 依赖：`Dependence`的`PuzzleMod`/`Canvas`(含`ConsumeMouseWheel`)/`PostHandle`/`JsonValue`，本目录`battle/`。
- Core端依赖`"resource path"`查询(`Config::GetResourceFilePath`)；UE端`UPuzzleWidget`负责现场构造`PostImplement`传进
  `Init`/`Loop`、换算鼠标坐标、转发鼠标滚轮。

## 待办/后续阶段

- **胜负不回传剧情**：`UPuzzleWidget`只判`Loop`返回值是否非0就结束，不把1/-1交给剧情脚本；要让`Wxdj.script`按
  胜负分支，需要UE侧在`EndPuzzle`时发story事件/设置变量。
- 战报面板暂不支持滚轮回看历史(只显示最新的若干条，上限200条)。
- 玩家目前只有`xiaohua.txt`这一套招式，没有随剧情成长/换招式表。
