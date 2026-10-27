#!/bin/sh
# PvE模型训练：从PvP模型热启动(输入层补零列)，双方带随机的五行属性/暴击/特性。先跑train_pvp.sh得到checkpoints_pvp/latest.pt。
# 检查点单独存在checkpoints_pve/，不会覆盖PvP的。(Linux/macOS/Git Bash；Windows用train_pve.ps1，参数含义见那里)
# 用法：sh train_pve.sh [--iters 600] [--hours 0] [--eval-every 50] [--eval-games 256] [--lr 1.5e-4] [--init checkpoints_pvp/latest.pt] [--games 1024] [--steps 80] [--resume]
cd "$(dirname "$0")"
ITERS=600; HOURS=0; EVAL_EVERY=50; EVAL_GAMES=256; LR=1.5e-4; INIT=checkpoints_pvp/latest.pt; GAMES=1024; STEPS=80; RESUME=0
while [ $# -gt 0 ]; do
  case "$1" in
    --iters) ITERS=$2; shift 2;;
    --hours) HOURS=$2; shift 2;;
    --eval-every) EVAL_EVERY=$2; shift 2;;
    --eval-games) EVAL_GAMES=$2; shift 2;;
    --lr) LR=$2; shift 2;;
    --init) INIT=$2; shift 2;;
    --games) GAMES=$2; shift 2;;
    --steps) STEPS=$2; shift 2;;
    --resume) RESUME=1; shift;;
    *) echo "未知参数 $1"; exit 1;;
  esac
done
export PYTHONIOENCODING=utf-8
sh build.sh || exit 1
python smoke_test.py || exit 1
COMMON="--mode pve --games $GAMES --steps $STEPS --pool 3000 --iters $ITERS --hours $HOURS --lr $LR --eval_every $EVAL_EVERY --eval_games $EVAL_GAMES"
if [ "$RESUME" = 1 ]; then
  python train.py $COMMON --resume || exit 1
else
  [ -f "$INIT" ] || { echo "找不到PvP检查点 $INIT：先跑train_pvp.sh(检查点是Git LFS文件，记得git lfs pull)"; exit 1; }
  python train.py $COMMON --init "$INIT" || exit 1
fi
python export_weights.py --pve checkpoints_pve/latest.pt || exit 1
echo "done. checkpoints_pve/latest.pt"
