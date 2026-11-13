"""重训：从零开始训练一个新模型，训完对最后的快照做权重平均，得到 runs/<name>/final.pt。

用法(Train 下，用虚拟环境的python)：
  python scripts/retrain.py                         默认训练200轮
  python scripts/retrain.py --iters 10000 --hours 18.5 --snap_every 10 --eval_every 100 --name base2    长训练(约20小时)
  python scripts/retrain.py --name base2 --iters 500 --snap_every 10 --eval_every 100 --resume     在 runs/base2 已训的基础上再训500轮(不用管之前训了多少轮)
没有列出的参数原样传给 train/train.py(如 --games 512 --eval_every 100 --snap_every 10，完整列表见 python train/train.py --help)。
输出在 runs/<name>/ (默认 runs/retrain)：final.pt(权重平均后的模型)、latest.pt、train.log、snaps/、table_used.txt(训练用的招式表副本)；
不加 --resume 时每次运行会先清空这个目录；加了 --resume 且目录里已有 latest.pt，就接着中断的地方训练(沿用目录里冻结的招式表)，
这时 --iters 是**在已训的基础上再训多少轮**(脚本自己读已训轮数)，--hours 是这一次运行的时间上限。
训练用的招式表默认 Cpp/act/complete.txt：用其中无属性的单效果家族当模板生成训练招式，每个虚拟家族的属性在 无/金/木/水/火/土/五行 里等概率抽(见 train/gen_pool.py)。
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as C

ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument('--iters', type=int, default=200, help='训练多少轮(默认200)；加 --resume 时是在已训的基础上再训多少轮')
ap.add_argument('--name', default='retrain', help='输出目录名 runs/<name>')
ap.add_argument('--table', default=C.TRAIN_TABLE, help='训练用的招式表(默认 Cpp/act/complete.txt)')
ap.add_argument('--seed', type=int, default=1)
ap.add_argument('--hours', type=float, default=0.0, help='最多训练多少小时，0=不限')
ap.add_argument('--resume', action='store_true', help='接着 runs/<name> 里中断的训练继续(不清空目录)')
args, extra = ap.parse_known_args()

C.train(os.path.join(C.RUNS, args.name), args.iters, args.table, extra, seed=args.seed, hours=args.hours, resume=args.resume)
