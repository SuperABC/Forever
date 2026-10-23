# ai/table：表格式对战AI的训练工具

## 职责

为五行斗诀(`battle/`)训练"表格式"对战策略：自博弈 + 蒙特卡洛控制，没有神经网络。
游戏里敌方通过`battle/ai.h`的`PolicyAI`读取这里产出的策略文件出招。

## 目录

| 文件 | 作用 |
|---|---|
| `trainer.cpp` | 训练/评测程序源码。直接编译`../../battle/`的规则源码，规则和系数与游戏完全一致 |
| `build.ps1` | 编译出`trainer.exe`；改了`battle/`的规则或系数后要重新跑 |
| `train.py` | 按`Resource/act/xiaohua.txt`为每一式训练一份策略 |
| `seed_policy.txt` | 第一层(出哪类招)的冻结策略，训练时作为初值并保持不变 |
| `tables/form_XX.txt` | 训练用招式表(生成物)：每个家族的第XX式各一个，共15个招式 |
| `policies/policy_XX.txt` | 第XX式的策略(生成物) |
| `policies/summary.txt` | 每式的双方属性、自对弈回合数、各家族使用率 |

## 策略的结构

- **第一层(324格)**：局面 = 自己真气档位(4) x 自己炸弹量(3) x 自己活力档(3) x 对手活力档(3) x 对手连续积攒次数(3)；
  选招类：跳过 / 积攒 / 释放炸弹 / 单招 / 双招。活力 = 健康和武力占比里较小的。
- **第二层(32格)**：上下文 = 自己真气档位(4) x 对手上一招属性(8，含"非攻击招")；单招选1个家族，双招选2个家族。
- 没有任何"真气不够就跳过"之类的硬编码规则。
- 第一层冻结为`seed_policy.txt`(比较平的分布)，只训练第二层——这是游戏当前敌方AI的做法，
  所以AI对局面不敏感、对"出哪个家族"有学习。想要更强的AI，去掉`freeze1=1`让两层都学习。

策略文件格式(文本)：第1行家族数n；接着324行，每行5个概率(第一层)；接着32行，每行n个概率(单招)；再32行，每行n个概率(双招)。
`PolicyAI::Load`要求n等于游戏里的家族数(15)，家族顺序和`xiaohua.txt`里一致。

## 使用

```powershell
cd Forever_Mod\Wxdj\Cpp\Wxdj\ai\table
powershell -File build.ps1                  # 编译trainer.exe
python train.py                             # 训练全部20式(约半小时，14线程)
python train.py 1 20                        # 只训练指定的式
python train.py --iters 10                  # 快速试跑
```

## 什么时候要重训

- 改了`xiaohua.txt`的招式数值或效果：重训受影响的式(直接`python train.py`全部重训最保险)。
- 改了`battle/`的规则或`def.h`的系数：先`build.ps1`再重训。
- 改了标准初始属性的公式：同步改`train.py`里的`BASE`/`SHRINK_*`/`GROW_MP`，并同步`battle/battle.md`。
- 招式家族个数变了：策略文件的第二层要重训(`PolicyAI::Load`会拒绝家族数不符的文件)。

## trainer.exe直接用法

```
trainer.exe <招式表> <模式> N T B 线程数 [种子] [key=val...]
```

模式：`learn`(训练，N/T/B占位填0)、`report`(统计已有策略)、`m`(每个招式的"权重提升"价值测试)、`duel`(甲方改属性对打)。
常用参数：`hp= atk= mp= rec= agi=`(双方属性)、`iters= epi= K= tau1= tau2=`(训练)、`init=`(初始策略)、
`freeze1=1`(冻结第一层)、`out=`/`policy=`(输出/读取的策略文件)。

## 局限

- 状态是粗粒度分档，策略很粗；学出来的均衡不唯一。
- 训练表里每个家族只有同一式的一个招式；游戏里敌方出招时每个家族固定取最高式。
