# Train：五行斗诀对战AI的训练与评测

训练一个**通用**PvP对战模型：双方的招式表不同、招式数值不同也能用。输入"双方招式 + 当前局面 + 这局的出招记录"，输出这回合怎么出招。
强化学习自博弈(PPO)，没有人类数据；**训练里没有任何脚本对手、基准机器人或人类套路**，套路由模型自己在对局里发现。规则直接复用 `Cpp/Wxdj/battle/`，和游戏完全一致。

## 目录

```
Train/
├── env/        训练环境：env.cpp(批量并行对局，导出C接口)、features.h(特征定义，游戏里 battle/ai.cpp 也 include 它)、
│               wxenv.py(ctypes封装)、build.ps1 / build.sh(编译 wxenv.dll / libwxenv.so)
├── train/      训练代码：train.py(PPO)、model.py(网络)、gen_pool.py(招式表生成)、bots.py(陪练开局的随机出招)、
│               pad_checkpoint.py(加特征维数后补零列)、avg_weights.py(快照权重平均)
├── eval/       评测代码：eval_report.py(单个最高式的评测)、eval_moves.py(公共函数)、make_report_all.py(多个式汇总)
├── tools/      export_weights.py：把模型导出到游戏(Cpp/act/ai_weights_pvp.bin)和网页(Browser/public/ai_weights_pvp.js)
├── scripts/    四个入口脚本：retrain.py(重训)、resume.py(续训)、train_eval.py(训练评测)、eval.py(仅评测)；_common.py 是共用函数
├── models/     训练好的模型：base/base_avg.pt(当前底座)、base/latest.pt、base/snaps/(训练时的历史快照)
├── runs/       【生成物】训练输出，每个 --name 一个目录：final.pt(权重平均后的模型)、latest.pt、snaps/、train.log、table_used.txt
└── results/    【生成物】评测结果，每次评测直接覆盖：report_<式>.txt、report_all.md、table_used.txt
```
招式表在 `Cpp/act/`：`complete.txt` 是完整招式表(7种属性 x 14种效果 x 20式，**训练和评测默认都用它**：训练用其中无属性的单效果家族当模板、家族属性随机抽，评测在其中混合抽样25个家族)，开头的注释里有各阶位的标准三维，**调数值就改它里面无属性的14个家族**，然后跑 `python tools/gen_tables.py` 重新生成其余6种属性；`xiaohua.txt` 是游戏和网页模拟器用的表(19个有实际名字的招式家族：千腿、金钟罩 ...)，也由 `tools/gen_tables.py` 从 complete.txt 生成，别手改。

## 四个脚本

用虚拟环境的python。**下面的 `scripts/xxx.py` 是相对路径，必须先 `cd` 到 `Train` 再运行**；在别的目录运行就把它写成绝对路径
(如 `D:\Projects\WXDJ\Forever\Forever_Mod\Wxdj\Train\scripts\eval.py`)，否则 Python 会报"找不到文件"然后直接退出。
每个脚本都有 `--help`，没列出的参数会原样传给 `train/train.py`。

| 脚本 | 做什么 | 默认 |
|---|---|---|
| `scripts/retrain.py` | 重训：从零训练，训完做权重平均得到 `runs/<name>/final.pt`；中断后加 `--resume` 接着训 | 200轮，训练表 `complete.txt`，`--name retrain` |
| `scripts/resume.py` | 续训：从已有模型出发(`--from`)接着训，改了招式表后用它，不用重训 | 200轮，从 `models/base/base_avg.pt`，学习率1e-4，`--name resume` |
| `scripts/train_eval.py` | 训练评测：先训练再评测；不给 `--from` 是重训，给了是续训 | 200轮训练 + 1000盘评测 |
| `scripts/eval.py` | 仅评测：评测一个模型 | `models/base/base_avg.pt`，评测 `complete.txt` 里混合抽样的25个家族，1000盘，最高式 1/6/10/14/17/20 |

```powershell
cd D:\Projects\WXDJ\Forever\Forever_Mod\Wxdj\Train
D:\Projects\WXDJ\.venv\Scripts\python.exe scripts/eval.py                           # 评测当前底座(1000盘)
D:\Projects\WXDJ\.venv\Scripts\python.exe scripts/eval.py --games 3000 --forms 20   # 只看最高式20，3000盘
D:\Projects\WXDJ\.venv\Scripts\python.exe scripts/resume.py                         # 用当前complete.txt续训200轮
D:\Projects\WXDJ\.venv\Scripts\python.exe scripts/train_eval.py --from models/base/base_avg.pt   # 续训200轮再评测
D:\Projects\WXDJ\.venv\Scripts\python.exe scripts/retrain.py --iters 3000 --hours 6              # 重训3000轮(约5小时)
D:\Projects\WXDJ\.venv\Scripts\python.exe scripts/retrain.py --name base2 --iters 8500 --hours 18.5 --snap_every 10 --eval_every 100   # 长训练(约18小时，每轮约7.3秒)
                                                                                  # 接着训：同一个 --name 后面加 --resume，--iters 是在已训的基础上再训多少轮(脚本自己读已训轮数)
```
- **训练轮数** `--iters`(默认200)；**评测盘数** `--games`(默认1000)。`train_eval.py` 里 `--games` 是评测盘数，训练每轮采样多少盘用 `--train_games`。
- 每次训练会先清空 `runs/<name>/`，每次评测会先清空 `results/`，所以结果总是覆盖上一次的。想留住就换 `--name`，或把 `results/` 拷走。
- 训练结束后做权重平均，`final.pt` 就是这个平均模型。训练满1000轮以上时：取最近1000轮里每隔100轮的一个快照(离目标轮数最近的那个)，共10个做平均；不到1000轮时退化成旧逻辑：对最后的快照(约最后20%)做平均；快照太少时用最终权重。快照间隔(`--snap_every`)最好不超过100轮，否则10个目标会取到重复的快照、实际个数不足10个。

## 评测哪些招式：默认在 complete.txt 里混合抽样25个家族

**评测默认读 `Cpp/act/complete.txt`**：98个家族超过容量，不带 `--select` 时自动混合抽样25个家族(每种效果至少1个、每种属性至少3个、余下随机补，固定种子，见 `gen_pool.mixed_sample`)，直接 `scripts/eval.py` 即可。
训练用的招式表默认也是 `complete.txt`(`train_eval.py` 里分别是 `--table` 和 `--eval_table`)。

- 评测的家族数有上限：每方最多125个招式 = 25个家族 x 5式。`--select` 选出的家族超过25个会报错并提示。
- 想评测某个子集，用 `--select 片段`(名字里含这些片段的家族，逗号分隔)，
  例：`--select 火属性`、`--select 穿透,减伤`、`--select 无效果`。
- 没有各阶位三维表的招式表(如 `xiaohua.txt`)，评测会自动从 `complete.txt` 补上，三维始终以 `complete.txt` 里的为准。训练时不受影响(训练池是生成的，每个文件最多25个家族)。

## 对局与招式表的规则

- 一场对局只指定**最高式M**，1~M式都可用。训练和评测的招式文件 = 每个家族取以M为最高式的**连续5式窗口**(M<5时是1..M)。游戏里敌方AI同样取最高式往下5个式(`puzzle_wxdj.cpp` 的 `ENEMY_AI_WINDOW`)。
- **各阶位的标准三维(健康/武力/真气)** 写在 `complete.txt` 开头的注释表里，直接手改即可：训练和评测的环境(`train/gen_pool.py` 的 `stage_stats`)每次都会读这张表，不需要同步；
  网页模拟器(`Browser/public/ai.html`)里有一份拷贝，改完表后要跑 `python Browser/tools/sync_stages.py` 同步一次。表里没有的量(每回合回复真气、炸弹倍率)仍按公式算。
  游戏里 `puzzle_wxdj.cpp` 的 `STANDARD_HP/ATK/MP` 是单独写死的常量，不会自动同步。
- 属性系数(同式同消耗下的威力比)：无属性1.0，单属性0.7，五行0.5。`complete.txt` 里所有招式的消耗/威力 = 无招同式数值 x 属性系数。
- **训练招式表的生成规律**(`train/gen_pool.py`)：模板 = 表里每个无属性的单效果家族(13个)，标尺 = 无属性无效果；**每个虚拟家族的属性在 无/金/木/水/火/土/五行 里等概率抽(各1/7)**，单属性威力 x0.7、五行 x0.5，消耗和效果参数不变；玩家本身仍无五行属性。训练日志每5轮多一行"属性 无x 金x ..."，是网络所选单招的属性占比。
  1. 每个文件有且只有一个无效果家族(位置随机)，其余家族挂 1/2/3 个效果，比例 0.6/0.3/0.1，同一招不重复同类效果。
  2. 每个效果采样**一个统一倍率 m** ~ N(1, 0.3)(截断到 [0.3, 效果自身上限]，反弹<0.8、禁锢≤0.95)，m 乘到该效果的全部数值参数上(比例类各分量、lock概率、increase/static/shield 比例，
     持续类 dot/rebate/recover/cure 的立即量和每段量；轮数、间隔不变)。
  3. 用 m **反向补偿**基础数值，"偏离量 x m"：每一式k，比值 = 1 + m x (模板比值(k) - 1)，模板比值 = 该效果家族的消耗(或威力) / 无属性无效果同式的对应值(也就是 xiaohua 里这个效果的"定价")；
     m=1 正好还原表里的数；多个效果按先后顺序把各自的比值连乘到标尺上；比值下限 0.05。
     例：穿透第20式 m=1.25：效果比例 0.6->0.75，消耗 3200->2656，威力 9440->5796。
  4. 另外加三类**不参与补偿**的小噪声 N(1, 0.1)(截断到 [0.6, 1.4])：消耗一个、威力一个、效果每个数值参数各一个；每个家族采样一次，5个式共用。
  5. 每个训练文件是一场对局：最高式M往下5个连续式(M<5时是1..M)，10~25个家族，每方最多125个招式；**每个文件必须有静止增伤招**(正常抽完如果整个文件都没有，就随机挑一个家族重抽成带静止增伤的)。
  `WXDJ_ACT` 环境变量指定招式表，`WXDJ_SELECT` 选评测的家族子集(脚本会设置它们)。

## 训练配方(`train/train.py` 的默认值)

**每局的抽法**(`sample_games`)有两个**互相独立**的随机：
- 招式库：50% 随机抽一个文件、双方都用它；40% 双方各自随机抽一个文件，但最高式相同；10% 双方各自纯随机抽文件。
- 三维(健康/武力/真气)：40% 双方都用标准值；40% 三维都乘扰动系数(对数正态，标准差0.4)、双方用同一组系数；20% 双方各自独立扰动。
  敏捷、跳过损失敏捷(1.0)、每回合敏捷回复(0.2)、炸弹积攒倍率是PvP的标准值，**不随机**：敏捷 = 5 + 最高式/4，炸弹倍率 = 阶位标准值(都取各方文件最高式对应的值)。
所以有约20%的局双方招式库和三维完全一样(同一文件 x 标准值)。评测里的自我对比(训练日志的 `EVAL` 行)用镜像局，和上面的抽法无关。

对手按权重 自己0.32 / 历史快照0.45 归一化；**陪练(席位1)每盘开局前抽一次开局种类**：40% 单招类开局(这些局里三种各1/3，即总数的 13.3%：①第1回合积攒、第2回合放炸弹；②第1回合出静止增伤招(先在静止增伤家族里等概率选家族，再在该家族各式里等概率；没有静止增伤招就按③)；③第1回合出单招(每个家族一个选项等概率，再在该家族各式里等概率))，10% 第1回合随机双招(同式)，50% 全程照常出招；开局之后(①是第3回合起，其余是第2回合起)全部交给自博弈/快照
(`--open_single 0.4 --open_double 0.1`)；随机出的招不是网络的决定，不参与学习，学习方(席位0)正常学习，这样模型能学会应对任意类型的开局；保留全部历史快照(每5轮一个，fp16存到 `snaps/`)并均匀抽对手，每轮4个快照槽；
熵系数初值0.02、衰减到0.5倍；每轮1024盘x80回合。奖励 = 双方"损失"差的变化(每回合) + 终局胜负。每50轮评测一次(和这次训练第一次评测、上一次评测存下的自己各打一遍，只作进度参考：自博弈会绕圈，胜率不是单调上升的)。

**网络对最高式的感知**：局面特征(SD=50)的第41维是"本局最高式/20"(双方所有招式里式号最大的，`features.h` 的 `StateExtras`；游戏 `battle/ai.cpp` 和网页 `ai.html` 的 `fillState` 同步计算)。
旧的49维检查点用 `train/pad_checkpoint.py` 在第41列插一列零即可继续用，输出不变(`models/base` 里的已补过)。

## 编译环境

规则变更记录：2026-11-02 起积攒的那一回合闪避敏捷减半(`ACCUMULATE_AGI_FACTOR=0.5`)，已有模型都是在旧规则下训的。
`env/wxenv.dll` 是编译产物(不进git)。改了 `Cpp/Wxdj/battle/` 或 `env/features.h` 之后重新编译：
`powershell -ExecutionPolicy Bypass -File env/build.ps1`(Linux/macOS用 `sh env/build.sh`)。

## 导出到游戏/网页

`python tools/export_weights.py models/base/base_avg.pt`(或 `runs/<name>/final.pt`)会覆盖 `Cpp/act/ai_weights_pvp.bin` 和 `Browser/public/ai_weights_pvp.js`；`--out_dir` 可输出到别处。
权重必须和 `env/features.h` 的特征、`train/model.py` 的网络结构完全一致。

## 局限

- 训练只覆盖5式窗口，"低式几乎无用"是设计假设，没有验证。
- 没有任何脚本对手，所以没有"对固定机器人的胜率"这种绝对强度指标；评测看的是各家族的使用率和去掉后胜率。
- 新增效果类型需要改特征(补零续训)；只改数值、不加新类型时直接续训即可。
