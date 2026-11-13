# puzzle 小游戏集(Basic)

本批 10 个小游戏(`puzzle_dodge/rhythm/dance/poker/chase/burger/tubes/miner/tower/coins`)只做了 mod 本身的逻辑与渲染，在独立的 `Tools/ModHost`(无 UE/Core)里验证过；什么场景调用哪个由剧情/工作系统另行设计。俄罗斯方块和按任意键退出见 `puzzle_*.md`。每个游戏类的声明里不再写说明，玩法与参数以本文为准。

## 公共约定

- 源码：`player/puzzle_<名>.h/.cpp`，公共小工具在头文件 `puzzle_common.h`(全 inline，不依赖 Core)：`PuzzleParams`(参数)、`PuzzleRng`(随机)、`PuzzleInput`(每帧输入)、画矩形/文字/按钮/结算面板、`LoadModTextFile`、宏 `PUZZLE_IDENTITY`。
- 参数：`SetParam`收到空格分隔的 `key=value` 串(如 `time=60 difficulty=3`)，未给的取默认值。所有游戏都认 `seed=N`(固定随机，复现/测试用)。部分带"测试用"的参数(`preset`/`fixed`/`orders`/`startphase`)只为 ModHost 脚本能确定性验证。
- 返回：`Loop` 结束时返回 puzzle 自己持有的 `JsonValue*`(根是 object)，Core 展平成 `$$local.<字段>`，数组为 `$$local.items[0]`。各游戏都带 `completed`：正常分出结果为 true，ESC 中途退出为 false(具体含义见各游戏的返回项；躲避方块里 `completed` 表示是被碰撞结束的)。
- 资源：图片/文本放 `Resource/Mod/Basic/`，文件名以 puzzle id 加下划线开头(`puzzle_dodge_bg.png`、`puzzle_rhythm_demo.chart`、`puzzle_chase_article.txt`…)，通过 post `"resource path"` 按文件名查找。
- 触摸/鼠标/键盘：除 `puzzle_chase` 必须键盘外，都可以只用鼠标/触摸操作(手机网页)，键盘作为并存快捷键；ESC 随时退出。
- 拿到参数的方式：配置里 `puzzle_mods` 的项即 id，启动 puzzle 时的参数串即 `SetParam` 的入参。

## 逐个说明

### puzzle_dodge 躲避方块(被捕判定)
- 玩法：正方形场地(内部用 [0,1] 归一化坐标，画面按画布大小等比映射，窗口缩放不影响状态)，白底 + 警察抓小偷图(α0.5) + 四周灰色障碍环(0.5 透明)。按住并拖动红色小方块开始；四个蓝色长方形(面积相同，长宽比 = `ratio^u`，u 在 [-1,1] 随机，默认即 1:3~3:1)从各自一侧朝外半圆随机方向飞出，撞边缘反弹，无视障碍环；每 `interval` 秒加速。红块碰到蓝块或障碍环即结束。红块边长为场地的 7%。
- 参数：`speed`(初速，场地边长/秒，默认 0.24)、`accel`(每次加速倍数，默认 1.15)、`interval`(秒，默认 10)、`ratio`(长宽比最大倍数，默认 3)。
- 返回：`{score(整数秒), time, completed}`。

### puzzle_rhythm 节奏大师
- 玩法：4 轨音符下落，键 d f j k 或触摸屏幕底部对应轨道；tap 单点、hold 长条(可中途换轨，每半拍一个判定点)。无音乐。画面里只画黄色 Perfect 区(每轨一个白色判定框)，判定线上移，Perfect 区前后(早/晚)各有不画出的 Normal 窗口；长条头必须命中并一直按住，中途松开(超过 Normal 窗口 110ms，换轨/左右滑动时新轨晚按或早按 110ms 内都算连上)后面的判定点全算 Miss，中途才开始按不算；长条换轨时画成一根连续的折线。判定窗口 Perfect 45ms、Normal 110ms；总分 100 万按判定单元平均分，Perfect 满分、Normal 60%。评级 S≥95万、A≥85万、B≥70万。
- 谱面：`Resource/Mod/Basic/puzzle_rhythm_demo.chart`(经 post `"resource path"` 按文件名找)，文本格式(`#` 开头是注释)：`bpm <每分钟节拍数>`、`offset <第0拍对应毫秒>`(两者可写在任意位置)、`tap <拍> <轨0-3>`、`hold <起拍> <止拍> <起轨> [<拍>:<轨> ...]`(后面的节点表示到该拍长条换到该轨)。长条每半拍一个判定点，每个判定点按住了正确轨道算 Perfect，否则 Miss。
- 参数：`chart`(谱面文件名)、`speed`(下落时间 ms，默认 1500)、`lead`(开始前等待 ms，默认 2000)、`perfect`、`normal`。
- 返回：`{score, perfect, normal, miss, maxCombo, completed}`。

### puzzle_dance 广场舞(修炼)
- 玩法：15 回合(默认序列长度 `3 3 3 4 4 4 5 5 5 6 6 7 7 8 8`)，按方向键(或底部按钮)输入箭头序列，按错清空本回合输入；输完按"完成"，计时条上紫色滑块在橙色完美区内完成得 2 分，否则 1 分，超时 0 分；连续两个完美回合时本回合额外 +1。
- 操作：方向键或屏幕方向按钮输入，空格/"OK"按钮完成；滑块走到头还没完成得 0 分。
- 参数：`lengths`(序列长度表，逗号分隔)、`time`(每回合固定秒数；不给则 `base+per×长度`，`base` 默认 4、`per` 默认 0.4)、`pstart`/`pend`(完美区起止比例，默认 0.7~0.9)；`fixed` 为固定箭头序列(LURD)，仅测试用。
- 返回：`{score, perfectRounds, normalRounds, failedRounds, maxStreak, completed}`。

### puzzle_poker 德州扑克(赌场)
- 画面：四种花色用图形画(黑桃/红桃/方块/梅花)，牌面按真实纸牌排版(角标点数+花色，2~10 按标准点阵，J/Q/K 画框)。
- 玩法：玩家对 `players-1` 个 AI，每人 `chips` 筹码，每手先交底注 `ante`，翻牌前/翻牌/转牌/河牌四轮下注。每轮首个下注只能下 `2×ante`(10)，之后只能翻倍(10→20→40…)；可弃牌/过牌/跟注/翻倍/全下。全下允许筹码不对称，按档位拆主池/边池，多投未被跟的部分退还。有人输光立即结算结束；随时可 Quit(本手当弃牌，AI 间剩余自动打完)。
- AI：蒙特卡洛估算胜率 + 底池赔率，带少量诈唬，`ai=easy|normal|hard` 调阈值与采样数。
- 操作：按钮或键盘 `f` 弃牌、`c` 过牌/跟注、`r` 翻倍、`a` 全下；摊牌后点任意处或按任意键下一手。
- 参数：`players`(2~6，默认 3)、`chips`(默认 2000)、`ante`(默认 5)、`ai`。
- 返回：`{profit:[玩家, AI1, ...](总和为0), hands, completed(是否因有人输光结束)}`，展平后 `profit[0]`…。
- 牌型评估 `PokerPuzzle::Evaluate`(7 选 5)与边池结算 `Settle` 是 public static，已用独立自测程序验证(类别排序、A-5 轮子顺、同花顺/葫芦、踢脚、平分、边池、退还)。

### puzzle_chase 生死时速(秘书工作，必须键盘)
- 玩法：上方跑道，小偷以固定速度跑(`wpm`)，警察每敲对文章下一个字符前进一格，追上小偷成功；小偷跑到文章末尾仍没追上失败。敲错不前进并有短暂硬直。不区分大小写。
- 文章：`Resource/Mod/Basic/puzzle_chase_article.txt`(`article=` 可换)，读入后换行/连续空白合成一个空格，只保留可直接敲出的 ASCII。
- 参数：`wpm`(小偷速度，字/分，1 词=5 字符，默认 40)、`lead`(初始领先字符数，默认 15)、`penalty`(错键硬直 ms，默认 300)、`article`。
- 返回：`{success, progress(0~1), time(秒), accuracy(0~1), completed}`。

### puzzle_burger 汉堡店(餐厅后厨)
- 玩法：全拖拽。面包/牛肉/鸡肉拖进 2×4 平板锅，点格子翻面(面包只煎一面，不能翻面)；肉每格两面各有独立计时(只累计朝下的面)，到 `cook` 秒变熟，再过 `burn` 秒变糊。面包单面熟且没糊即可，肉要两面都熟且无糊面；没熟/糊了的只能扔垃圾桶。生菜/西红柿/洋葱/4 种酱料直接拖到盘。食材用卡通图标 `puzzle_burger_<bun|beef|chicken|lettuce|tomato|onion|ketchup|mayo|salad|mustard>.png` 表示，不显示字母。4 个盘位各放一个汉堡(食材集合，无顺序)，拖到出餐区与订单比对：一致得分并消单，不一致盘被清空不扣分。
- 订单：随机间隔 `order_min~order_max` 秒，必含面包 + 牛肉或鸡肉，其余食材/酱料各 50% 随机；分值 `10 + 3×额外食材数`，倒计时 `40 + 5×额外食材数` 秒，同时最多 `max_orders` 单。
- 参数：`time`(默认 60)、`cook`(6)、`burn`(5)、`order_min`(6)、`order_max`(12)、`max_orders`(4)；`orders` 为开头几单固定(如 `bun+beef+tom+ketchup;bun+chick`，食材名 bun/beef/chick/lett/tom/onion/ketchup/mayo/salad/mustard)，仅测试用。
- 返回：`{score, served, expired, completed}`。(假设："1×4 个位置"理解为 4 个独立盘位。)

### puzzle_tubes 多色试管(保洁)
- 玩法：点选一个试管再点另一个倒水：目标为空则倒出上方连续同色层；顶色相同则倒入 `min(连续同色层数, 目标剩余空位)`；否则取消选中。每个管都是空的或装满且同色才算完成。步数 = 有效倒水次数。
- 生成：打乱后爬山去掉相邻同色层，并用 DFS 验证可解(100 个种子全部可解且相邻同色层数为 0)。
- 参数：`colors`(默认 6)、`capacity`(每管层数，默认 4)、`empty`(空管数，默认 2)；`preset` 为固定开局(管用分号隔开，每管从底到顶的颜色数字)，仅测试用。
- 返回：`{steps, completed}`。(不满的单色管不算，必须同色凑满一管。)

### puzzle_miner 黄金矿工(矿场)
- 玩法：钩子绕转轴摆动，点击/按任意键放出，碰到物体后按重量决定收回速度。大/中/小金块 500/250/100(收回速度，勾住东西后已减半：小金块快、中/大金块中、小石块中、大石块慢，钻石快)，石块不计分且重，钻石 600 作为"额外收入"单独记录，金块都不动，每局在靠近底部有 `moving_gems`(默认 2)个左右移动的钻石，石块可能遮挡金块；难度越高摆得越快、物体越多、石块越多。不分关卡不开商店，限时 `time` 秒。
- 参数：`time`(默认 60)、`difficulty`(1~3，默认 2)、`moving_gems`(默认 2)、`hook`(放钩速度，画布高度/秒，默认 0.7)；`swing`(摆速倍率，默认 1，仅调试用)、`preset`(固定物体 `类型,x,y;...`，类型 g1/g2/g3/s1/s2/d，x,y 为 0~1 画布比例，仅测试用)。
- 只有金块计入 `gold`，钻石计入 `diamond`；石块可能遮挡金块(钩子碰到最前面的那个就停)。
- 返回：`{gold, diamond, completed(是否正常到时结束)}`。

### puzzle_tower 平衡楼房(工地)
- 玩法：吊车摆动吊着一层房子，点击/任意键松绳垂直落下。与顶层(按它此刻摇摆后的位置)重叠宽度占楼层宽度比例 ≥ `overlap` 搭上，否则这块掉走扣一条命。楼体摇摆幅度 = 相邻楼层左右偏移绝对值之和(以楼层宽度计)的函数，越高越大；吊车摆速随层数略增；镜头随楼高上卷。
- 参数：`lives`(默认 3)、`swing`(摆速倍率，默认 1)、`overlap`(默认 0.25)；`startphase` 为吊车起始相位(默认随机)，仅测试用。
- 返回：`{floors(成功搭上的层数), completed(是否因命用完结束)}`。

### puzzle_coins 零花钱大作战(银行柜员)
- 玩法：`cols`(10)列硬币，每列最高 `maxh`(10)，列底部(靠近玩家)才能操作。硬币从每列顶部往下排，最下面一枚离玩家最近可操作。每隔 `interval` 秒每列顶部(远端)增加一枚随机自然硬币(1/5/10/50)，间隔每次乘 `decay` 逐步缩短(下限 `min_interval`)；任何一列超高即失败。吸取一列底部连续同面值整组(手里已有同面值硬币时累加，面值不同则不能吸取)，放到某列底部，只检查该列底部连续块合成：1×5→5、5×2→10、10×5→50、50×2→100、100×5→500、500×2→消除并 +1000，整块消耗只产生一枚上级币并继续连锁。失败后可重新开始(分数清零)或退出。
- 操作：←/→ 换列，`s`/↓ 吸取，`d`/↑/空格放下；触摸点列选中，再点"吸取/放下"按钮。
- 参数：`cols`(10)、`maxh`(10)、`interval`(5)、`min_interval`(1.5)、`decay`(0.97)；`preset="1,1,5;10;;..."`(列之间分号，列内从远端到近端；给了就不做开局下移)，仅测试用。
- 返回：`{score, time(存活秒数), completed(是否因失败结束)}`。

## 在 ModHost 里验证

`Tools/ModHost/tests/<名>.txt` 是各游戏的脚本(文件头注释里写了运行时用的 `--param`)，例如：

```
x64/Release/ModHost.exe --puzzle puzzle_rhythm --script Tools/ModHost/tests/rhythm.txt
x64/Release/ModHost.exe --puzzle puzzle_dodge --script Tools/ModHost/tests/dodge.txt
```

脚本里 `at <绝对毫秒> press|release <键>`、`at <毫秒> mdown|mmove|mup|shot`、`type <文本>` 是为这批游戏新增的命令，见 `Tools/ModHost/README.md`。
