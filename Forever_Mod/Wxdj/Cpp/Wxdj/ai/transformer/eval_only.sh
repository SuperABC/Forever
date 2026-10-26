#!/bin/sh
# 用法：sh eval_only.sh 1 20   评估checkpoints/latest.pt在指定式上的强度，写 report_<式>.txt
cd "$(dirname "$0")"
for f in "$@"; do
  PYTHONIOENCODING=utf-8 python eval_report.py checkpoints/latest.pt $f 2>&1 | grep -v "Warning\|torch.load" > report_$f.txt
done
echo finished > checkpoints/eval.done
