# ai/transformer：通用对战模型的训练工具

## 职责

训练一个**通用**对战模型：双方的招式文件不同也能用，输入"双方招式 + 当前局面 + 这局的出招记录"，输出这回合怎么出招。
强化学习自博弈(PPO)，没有人类数据。规则直接复用`battle/`，所以规则/系数和游戏完全一致。

## 文件

| 文件 | 作用 |
|---|---|
| `env.cpp`、`features.h` | 训练环境`wxenv.dll`：批量并行对局，导出C接口；特征(招式83维/局面49维/每回合记录59维x8回合；75~82和最后8维是increase/shield/static相关的追加维度)定义在`features.h`，以后游戏里推理复用。**PvE模式**(`wx_set_mode(1)`)在PvP维数后面再追加：招式5维(`MoveExtrasPvE`：自己拥有该属性/同属性倍率/增伤/对手抗性乘数/对手易伤)、局面34维(`StateExtrasPvE`：双方的五行属性、五个抗性、最厚盾的属性、暴击)，PvE模型输入维数 88/83；PvP的局面里这些维度恒为0 |
| `build.ps1`、`build.sh` | 编译`wxenv.dll`(Windows)/`libwxenv.so`(Linux/macOS)(改了`battle/`或`features.h`后重新跑；执行策略拦截时用`powershell -ExecutionPolicy Bypass -File build.ps1`) |
| `wxenv.py` | `wxenv.dll`的ctypes封装；`set_mode(pve)`切换PvP/PvE(要在建Env和模型之前)，`Env.reset(..., pve=配置)` |
| `gen_pool.py` | 生成随机化的招式文件池(真实家族的随机子集/扰动 + 合成家族 + 式的子集)；合成/扰动的家族里也有increase(连招增伤)和shield(盾量=武力上限的比例)效果 |
| `pve_gen.py` | PvE训练的随机配置：双方五行属性(35%无/15%五个齐全/其余1~4个)、基础暴击、各种特性(增伤/易伤/抗性/同属性倍率/暴击/闪避/命中/反弹/免疫/每回合回复/立即加盾……)；15%的对局什么都没有(=PvP规则)，双方可以不对等 |
| `smoke_test.py` | 冒烟测试(几分钟)：维数、"无配置时PvE特征=PvP特征+零"、补零热启动输出一致、两种模式的train.py各跑2轮、导出权重；长时间训练的脚本会先跑它 |
| `train_pvp.ps1/.sh`、`train_pve.ps1/.sh` | 一键脚本，见下面"训练" |
| `model.py` | 模型：Perceiver式小Transformer，输出 招类 -> 第一个招式 -> (双招)第二个招式(必须同式) |
| `bots.py` | 脚本机器人：随机/单家族/最高威力/积攒炸弹，训练时当陪练、评测时当参照(不在游戏里) |
| `train.py` | PPO训练 |
| `eval_moves.py`、`eval_report.py` | 招式强度评估：`python eval_report.py checkpoints_pvp/latest.pt <式>`，输出招类使用率、各家族自由对局里用作单招/双招的比例和去掉后胜率、固定套路(不做单招/双招矩阵)，进度写在`progress_<式>.txt`；评测结束后`make_report_all.py`把各式的`report_<式>.txt`汇总成`report_all.md`(各式概览 + 各家族单招/双招使用率/去掉后胜率一张表)，也可单独运行 |
| `ai_test.cpp` | 游戏里C++推理(`battle/ai.cpp`)的对拍程序：用游戏的招式表和导出的`ai_weights_pvp.bin`跑两个回合，打印logit，和网页版同一场景的JS输出比，误差应<=1e-4。编译：`cl /O2 /EHsc /std:c++17 /utf-8 /I ../../battle ai_test.cpp ../../battle/{action,controller,cultivator,effect,modifier,ai}.cpp`，运行：`nntest 招式表xiaohua.txt ai_weights_pvp.bin` |
| `pad_checkpoint.py` | 特征扩维(75/81→83、41→49)之后，把旧检查点的输入层补零列(旧模型输出不变)，不用重训；`pad_to_shapes`是PvP→PvE热启动用的通用版本 |
| `export_weights.py` | 把检查点导出成`../../../Online/public/ai_weights_pvp.js`(网页版加载)和`../../../Resource/act/ai_weights_pvp.bin`(游戏里C++推理读)；`--pve`导出PvE模型到`ai_weights_pve.js/.bin`(不覆盖PvP的)；`--out_dir`输出到别处 |
| `chain.sh`、`train_only.sh`、`eval_only.sh` | 一键脚本：训练+导出+评估多个式 / 只训练+导出 / 只评估(`sh eval_only.sh 1 20`) |
| `report_<式>.txt` | 当前这一版模型在第1/5/10/15/20式上的评估报告 |
| `checkpoints_pvp/` | `latest.pt`(当前这一版模型，可续训)；训练日志和历史检查点已清理，训练时会重新生成 |

## 模型

- 输入全是特征，不含招式编号：每个招式一个75维向量(消耗/威力相对双方上限、属性、各效果的比例和数值)，所以换招式文件、换阶位规模都能用。
- 一组可学习的潜变量依次读取"局面、最近8回合记录、自己的全部招式、对手的全部招式"(交叉注意力，计算量随招式数线性增长)，
  然后每个自己的招式读取潜变量得到带上下文的表示，用指针网络打分。
- 约92万参数(fp32约3.7MB，fp16约1.8MB)。

## 训练

PvP和PvE是**两个模型，分开训练、分开发布**：PvP模型规则冻结、要稳定；PvE的规则(特性/五行)还在变，且PvE对手不需要接近最优。
PvE模型从PvP模型热启动(输入层补零列，一开始输出和PvP模型一致)，再用随机的五行/抗性/暴击/特性对局微调。检查点分开：`checkpoints_pvp/`(PvP)、`checkpoints_pve/`(PvE)。
招式表(`Resource/act/xiaohua.txt`)现在是17个真实家族(含火之高兴=火的increase、安如磐石=土的shield)，PvP和PvE都用它生成招式池。

```powershell
# 1. PvP模型：从零重训；先编译环境、跑冒烟测试，结束后导出权重并评估第1/5/10/15/20式
powershell -ExecutionPolicy Bypass -File train_pvp.ps1 -Iters 2000
powershell -ExecutionPolicy Bypass -File train_pvp.ps1 -Iters 1000 -Resume     # 接着训练，再训1000轮
# 2. PvE模型：从PvP检查点热启动微调
powershell -ExecutionPolicy Bypass -File train_pve.ps1 -Iters 600
powershell -ExecutionPolicy Bypass -File train_pve.ps1 -Iters 300 -Resume
# 参数(两个脚本通用)：-Iters 这次训练多少轮 | -Hours 最多多少小时(0=不限，和-Iters谁先到谁停) | -EvalEvery 每多少轮评测一次
#   -EvalGames 训练中每次评测每种机器人每个席位打多少盘(默认256) | -ReportGames 结束后招式强度报告每个对局组合打多少盘(PvP脚本，默认100)
#   -Games/-Steps 每轮采样多少盘/每盘最多多少回合 | -Lr 学习率(PvE脚本) | -Init 热启动的PvP检查点(PvE脚本)
# 手动：
powershell -ExecutionPolicy Bypass -File build.ps1
python smoke_test.py
python train.py --iters 2000 --eval_every 50 --eval_games 256                     # PvP从头训练
python train.py --mode pve --init checkpoints_pvp/latest.pt --iters 600 --lr 1.5e-4   # PvE热启动微调
python eval_report.py checkpoints_pvp/latest.pt 20 --games 100                    # 招式强度报告，--games是每个对局组合打多少盘
python export_weights.py checkpoints_pvp/latest.pt                                # 导出PvP权重
python export_weights.py --pve checkpoints_pve/latest.pt                          # 导出PvE权重
```
Linux/macOS用`sh train_pvp.sh`/`sh train_pve.sh`，参数写成`--iters 2000 --eval-every 50 --eval-games 256 --report-games 100`(同名)，先自动`sh build.sh`。训练日志在各检查点目录的`train.log`，`plan:`那行写了这次打算训练到哪一轮。
推理端：网页人机对战(`ai.html`)已经支持PvE权重——`ai_weights_pve.js`放在`ai_weights_pvp.js`旁边，顶部"模型"开关就能选PvP/PvE(没有这个文件时PvE选项灰掉)，
网络按权重的输入维数(88/83=PvE)自动选特征，PvE的5+34维特征的JS实现和`features.h`逐项对拍过(最大差<1e-6)。
游戏里的C++推理(`battle/ai.cpp`)目前仍只认PvP维数的权重，PvE模型要接入需要让`ai.cpp`按权重维数调用`MoveExtrasPvE/StateExtrasPvE`。

每轮：随机抽N盘对局(双方招式文件、属性各自随机，40%镜像)，对手 50%自己 / 30%历史快照 / 20%脚本机器人；
奖励 = 双方"损失"差的变化(每回合) + 终局胜负。每50轮评测一次(对四种机器人各打512盘，座位各一半)并存检查点。
PvE模式下每盘对局再随机一份配置(`pve_gen.sample_config`)；评测时双方配置一样(公平)。

## 局限

- 训练文件每方最多60个招式(游戏里完整表有300个)，注意力是线性的所以推理不受限，但没在300个招式的文件上训练过。
- 只有一个对手分布(自己+历史+4种机器人)，没有专门针对人类套路的"陪练"。
