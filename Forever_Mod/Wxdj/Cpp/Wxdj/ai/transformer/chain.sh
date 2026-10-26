#!/bin/sh
# 训练200轮 -> 导出权重 -> 评估第1式和第20式
cd "$(dirname "$0")"
python train.py --games 1024 --steps 80 --pool 3000 --hours 3 --max_iters 200 --eval_every 50 > checkpoints/stdout.log 2>&1
cp checkpoints/latest.pt checkpoints/export_src.pt
python export_weights.py checkpoints/export_src.pt > /dev/null 2>&1
sed -i '2s/export_src.pt/latest.pt/' ../../../../Tools/nn_weights.js
for f in 1 5 10 15 20; do
  PYTHONIOENCODING=utf-8 python eval_report.py checkpoints/latest.pt $f 2>&1 | grep -v "Warning\|torch.load" > report_$f.txt
done
echo finished > checkpoints/chain.done
