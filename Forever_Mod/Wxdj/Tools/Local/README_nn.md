# 神经网络AI（wxdj_simulator.html）

`wxdj_simulator.html` 里的敌方出招模式"神经网络AI（通用模型）"，推理代码已经内嵌在这个html里（照 `Cpp/Wxdj/ai/transformer` 的
`model.py`/`features.h`/`env.cpp` 移植），权重放在同目录的 `nn_weights.js`（约4.9MB，可选）。

- 有 `nn_weights.js`：打开页面，敌方出招默认是神经网络AI，标题栏显示检查点（如"latest.pt 第800轮"）；"AI用第N式"设AI每回合用第几式（默认第20式，和训练一致）。
- 没有 `nn_weights.js`：神经网络AI选项自动隐藏，其余模式照常用。

## 换检查点

```powershell
cd Forever_Mod\Wxdj\Cpp\Wxdji	ransformer
python export_weights.py checkpoints\latest.pt      # 或 checkpoints\model_00250.pt
```

## 一致性

移植后和PyTorch/C++训练环境逐项对拍过：同一局面下特征完全相同，策略输出的差别小于1e-4。
改了训练端的特征或网络结构，要同步改html里内嵌的推理代码并重新导出权重。
