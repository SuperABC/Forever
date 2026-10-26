#!/bin/sh
# 只训练200轮并导出权重(不评估)
cd "$(dirname "$0")"
python train.py --games 1024 --steps 80 --pool 3000 --hours 3 --max_iters 200 --eval_every 50 > checkpoints/stdout.log 2>&1
cp checkpoints/latest.pt checkpoints/export_src.pt
python export_weights.py checkpoints/export_src.pt > /dev/null 2>&1
sed -i '2s/export_src.pt/latest.pt/' ../../../../Tools/nn_weights.js
echo finished > checkpoints/train.done
