# puzzle_wxdj

## 职责

五行斗诀回合制战斗小游戏(`WxdjPuzzle`，id `puzzle_wxdj`)：玩家(下标0)对售货员(下标1)，玩家点按钮/招式出招，
敌方AI出招，战报滚动，一方阵亡后弹出结果遮罩，点"关闭"结束。战斗规则全在`battle/`，这里只负责画面和鼠标交互。

## 界面布局

- 顶部：双方状态条(MP/ATK/HP 自上而下，最下面一行是护盾：每块盾一段文字，写"木盾 1200·3回合"(组合属性写"木火盾"，五个齐全写"五行盾")，没有盾写"无"；护盾没有上限，所以只有数字没有条)。状态条下面一行是场地效果：每个当前生效的场地一个色块(灼烧伤害x1.5/锈蚀必中/扬尘命中率50%/泥沼消耗x1.5，剩余回合数)，没有就显示"场地：无"；场地出现/刷新/消失会写进战报。
- 下方左半：战报面板，**从最新一条往上画**，画到面板顶为止，保证最新战报始终可见。
- 下方右半：操作栏 + 招式list。操作栏第一行`单招`/`双招`(模式单选)，第二行`积攒真气炸弹`/`释放真气炸弹(已积攒N)`/`跳过`；
  其下一行提示文字，再下面是一个统一的"第N式"滑块，再下面是招式家族list(每个家族一个按钮)，右侧贴滚动条。
- 鼠标悬停在家族按钮上，跟随鼠标弹出hint框，写出当前滑块所选那一式的全部效果和数值(物理/穿透/减伤/反弹/吸收的各属性比例、禁锢概率、持续伤害/回复的立即量/每段量/间隔/次数)；无效果的招式不弹。

## 五种操作

| 操作 | 交互 |
|---|---|
| 跳过 | 点`跳过` → `SkipAction` |
| 单招 | 模式=单招，点家族按钮立即按统一滑块当前的式出招 → `GetAction(i)` |
| 双招 | 模式=双招，依次点两个家族按钮(可点同一个两次)，**两招必须同式**：选了第一招后滑块锁定(置灰不可动)，直到第二招提交或切换模式，第二次点击提交 → `GetAction(i,j)`；切换模式/再点模式按钮取消已选 |
| 积攒真气炸弹 | 点按钮，无消耗，始终可点 → `AccumulateAction`；积攒的那一回合自己受到的所有伤害多20%(`ACCUMULATE_VULNERABILITY`，战斗内核里算，UI不用处理) |
| 释放真气炸弹 | 点按钮；已积攒为0时置灰 → `BombAction`(威力在`Controller::ActionTurn`里由`Cultivator::BombPower()`取走并清零) |

真气不足也能出招(透支)：不足的真气按2倍扣武力值(`Cultivator::ConsumeMP`)，家族按钮不因真气置灰。

## 关键设计

- **控件统一**：每帧`RebuildWidgets`生成一份`Widget{id, rect, enabled}`列表，命中测试(`HitTest`，后加入的优先)、
  绘制、点击激活(`Activate`)共用，避免两套坐标。家族按钮id = `ID_ROW_BASE + 家族下标`，统一滑块id = `ID_SLIDER`。家族由招式名去掉"第X式"分组(`BuildFamilies`)，滑块默认停在最高式。
  滑块在按下和拖动中按鼠标x取整到某一式(`SetSliderFromMouse`，可选式数`formMax`取各家族式数最小值)，松开不触发点击。
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
- **敌方AI**：用`battle/ai.h`的`AI`，加载`Resource/act/ai_weights_pvp.bin`(训练好的通用模型，约92万参数/3.7MB，由`ai/transformer/export_weights.py`导出，和网页版人机对战是同一个模型、同一套特征与推理逻辑)：每回合出招前`BeginRound`，出招结算后`EndRound`(用本回合日志里的闪避记录和状态变化攒最近8回合的出招记录)，出招时从每个家族的最高式(第20式)里选，招类/第一招/双招的第二招(同式)都按网络输出的概率采样。权重文件缺失、结构对不上或推理出错时，敌方退化成纯随机出招(3/4概率出单招，下标范围含积攒/炸弹/跳过，1/4概率出双招)。C++推理和网页JS推理的logit对拍过(两个回合带出招记录，误差<=2e-5)，对拍程序见`ai/transformer/nn_ai_test.cpp`。重训模型后跑`export_weights.py`就同时更新网页和游戏的权重。
- **阵亡判定**：玩家阵亡(含同归于尽)判负，只有敌方阵亡才判胜。
- 数值：双方都用4-6阶位标准属性 HP25800/ATK38900/MP24800、回真气619(敏捷：玩家22、售货员20)，见`battle/battle.md`。

## 依赖关系

- 依赖：`Dependence`的`PuzzleMod`/`Canvas`(含`ConsumeMouseWheel`)/`PostHandle`/`JsonValue`，本目录`battle/`。
- Core端依赖`"resource path"`查询(`Config::GetResourceFilePath`)；UE端`UPuzzleWidget`负责现场构造`PostImplement`传进
  `Init`/`Loop`、换算鼠标坐标、转发鼠标滚轮。

## 玩家五行属性

`SetParam`(由`UPuzzleWidget`在`Init`之前调用)解析剧情参数`elem=金木水火土的任意组合`(玩家)和`enemy_elem=…`(敌人)，`Init`里`SetElements`；不传就是无属性，和以前完全一样。
状态条名字后面用`[金木]`标出当前五行属性；护盾hint里写了同属性扣盾减半。规则见`battle/battle.md`的"玩家五行属性"。

## 待办/后续阶段

- **PvE模型接入**：`battle/ai.cpp`只认PvP维数(81/49)的权重；训练出PvE模型(`ai/transformer`的`train_pve`)后，让`ai.cpp`按权重维数加载`ai_weights_pve.bin`并调用`features.h`的`MoveExtrasPvE/StateExtrasPvE`，再在`SetParam`/剧情里选用。

- **胜负不回传剧情**：`UPuzzleWidget`只判`Loop`返回值是否非0就结束，不把1/-1交给剧情脚本；要让`Wxdj.script`按
  胜负分支，需要UE侧在`EndPuzzle`时发story事件/设置变量。
- 战报面板暂不支持滚轮回看历史(只显示最新的若干条，上限200条)。
- 玩家目前只有`xiaohua.txt`这一套招式，没有随剧情成长/换招式表。
- **被禁锢**：玩家这回合被禁锢(`Controller::IsLocked(0)`，回合开始时由禁锢效果设置)时，所有家族按钮和"释放真气炸弹"置灰且不可点，只有"积攒真气炸弹"和"跳过"可用；操作栏外框标红，提示行换成红底白字"被禁锢！本回合无法出招..."。
- **敏捷**：状态条名字后面显示"AGI 当前/上限"(跳过一次损失`agiLoss`点、每回合恢复`agiRecover`点，规则见`battle/battle.md`)。
