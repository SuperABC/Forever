# AI（public/ai.html）

`ai.html`（人机对战页）里的敌方出招，推理代码已经内嵌在这个html里（照 `Cpp/Wxdj/ai/transformer` 的
`model.py`/`features.h`/`env.cpp` 移植），权重放在同目录的 `ai_weights.js`（约4.9MB）。游戏(UE)里用的是同一个模型的C++推理(`Cpp/Wxdj/battle/ai.h/.cpp`)，权重是 `Resource/act/ai_weights.bin`。

- 敌方出招有三种：**AI**（默认）、**随机**（纯随机：3/4概率从单招/积攒/炸弹/跳过里挑一个，1/4概率随机双招）、**手动**（自己控制敌方）。
- 有 `ai_weights.js`：打开页面，敌方默认是AI，标题栏显示检查点（如"latest.pt 第199轮"）；"AI用第N式"设AI每回合用第几式（默认第20式，和训练一致）。
- 没有 `ai_weights.js`：AI选项自动隐藏，敌方默认纯随机。UE里同理：权重文件缺失时敌方退化成纯随机。

重训后重新导出：`python Cpp/Wxdj/ai/transformer/export_weights.py`（同时更新网页和游戏的权重），权重必须和训练端的特征、网络结构完全一致。
移植后和PyTorch/C++训练环境逐项对拍过：同一局面下特征完全相同，输出的差别小于1e-4。
