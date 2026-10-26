# ai/transformer：通用对战模型的训练工具

## 职责

训练一个**通用**对战模型：双方的招式文件不同也能用，输入"双方招式 + 当前局面 + 这局的出招记录"，输出这回合怎么出招。
强化学习自博弈(PPO)，没有人类数据。规则直接复用`battle/`，所以规则/系数和游戏完全一致。
表格式的老版本见`../table/`。

## 文件

| 文件 | 作用 |
|---|---|
| `env.cpp`、`features.h` | 训练环境`wxenv.dll`：批量并行对局，导出C接口；特征(招式75维/局面41维/每回合记录59维x8回合)定义在`features.h`，以后游戏里推理复用 |
| `build.ps1` | 编译`wxenv.dll`(改了`battle/`后重新跑；执行策略拦截时用`powershell -ExecutionPolicy Bypass -File build.ps1`) |
| `wxenv.py` | `wxenv.dll`的ctypes封装 |
| `gen_pool.py` | 生成随机化的招式文件池(真实家族的随机子集/扰动 + 合成家族 + 式的子集) |
| `model.py` | 模型：Perceiver式小Transformer，输出 招类 -> 第一个招式 -> (双招)第二个招式(必须同式) |
| `bots.py` | 脚本机器人：随机AI(游戏原来的)/单家族/最高威力/积攒炸弹，做陪练和评测 |
| `train.py` | PPO训练 |
| `eval_moves.py`、`eval_report.py` | 招式强度评估：`python eval_report.py checkpoints/latest.pt <式>`，输出招类使用率、各家族的单招/双招矩阵和固定套路，进度写在`progress_<式>.txt` |
| `export_weights.py` | 把检查点导出成`../../../Tools/Local/nn_weights.js`(模拟器html加载) |
| `chain.sh`、`train_only.sh`、`eval_only.sh` | 一键脚本：训练+导出+评估多个式 / 只训练+导出 / 只评估(`sh eval_only.sh 1 20`) |
| `report_<式>.txt` | 当前这一版模型在第1/5/10/15/20式上的评估报告 |
| `checkpoints/` | `latest.pt`(当前这一版模型，可续训)；训练日志和历史检查点已清理，训练时会重新生成 |

## 模型

- 输入全是特征，不含招式编号：每个招式一个75维向量(消耗/威力相对双方上限、属性、各效果的比例和数值)，所以换招式文件、换阶位规模都能用。
- 一组可学习的潜变量依次读取"局面、最近8回合记录、自己的全部招式、对手的全部招式"(交叉注意力，计算量随招式数线性增长)，
  然后每个自己的招式读取潜变量得到带上下文的表示，用指针网络打分。
- 约92万参数(fp32约3.7MB，fp16约1.8MB)。

## 训练

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
python train.py --hours 10            # 从头训练，约1.7k样本/秒(RTX 3070笔记本)
python train.py --resume --hours 10   # 接着训练
```

每轮：随机抽N盘对局(双方招式文件、属性各自随机，40%镜像)，对手 50%自己 / 30%历史快照 / 20%脚本机器人；
奖励 = 双方"损失"差的变化(每回合) + 终局胜负。每50轮评测一次(对四种机器人各打512盘，座位各一半)并存检查点。

## 局限

- 训练文件每方最多60个招式(游戏里完整表有300个)，注意力是线性的所以推理不受限，但没在300个招式的文件上训练过。
- 只有一个对手分布(自己+历史+4种机器人)，没有专门针对人类套路的"陪练"。
