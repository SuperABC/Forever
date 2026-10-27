#!/bin/sh
# PvP模型重训(从零)：编译环境 -> 冒烟测试 -> 训练 -> 导出权重 -> 评估各式。(Linux/macOS/Git Bash；Windows用train_pvp.ps1，参数含义见那里)
# 用法：sh train_pvp.sh [--iters 2000] [--hours 0] [--eval-every 50] [--eval-games 256] [--report-games 100] [--games 1024] [--steps 80] [--resume] [--skip-eval]
cd "$(dirname "$0")"
ITERS=2000; HOURS=0; EVAL_EVERY=50; EVAL_GAMES=256; REPORT_GAMES=100; GAMES=1024; STEPS=80; RESUME=""; SKIP_EVAL=0
while [ $# -gt 0 ]; do
  case "$1" in
    --iters) ITERS=$2; shift 2;;
    --hours) HOURS=$2; shift 2;;
    --eval-every) EVAL_EVERY=$2; shift 2;;
    --eval-games) EVAL_GAMES=$2; shift 2;;
    --report-games) REPORT_GAMES=$2; shift 2;;
    --games) GAMES=$2; shift 2;;
    --steps) STEPS=$2; shift 2;;
    --resume) RESUME="--resume"; shift;;
    --skip-eval) SKIP_EVAL=1; shift;;
    *) echo "未知参数 $1"; exit 1;;
  esac
done
export PYTHONIOENCODING=utf-8
sh build.sh || exit 1
python smoke_test.py || exit 1
python train.py --games "$GAMES" --steps "$STEPS" --pool 3000 --iters "$ITERS" --hours "$HOURS" --eval_every "$EVAL_EVERY" --eval_games "$EVAL_GAMES" $RESUME || exit 1
python export_weights.py checkpoints_pvp/latest.pt || exit 1
if [ "$SKIP_EVAL" = 0 ]; then
  for f in 1 5 10 15 20; do
    python -W ignore eval_report.py checkpoints_pvp/latest.pt $f --games "$REPORT_GAMES" 2>&1 | grep -v "Warning\|torch.load" > report_$f.txt
  done
  python make_report_all.py
fi
echo "done. checkpoints_pvp/latest.pt, reports: report_*.txt"
