"""续训：从一个已有的模型出发继续训练(用新的招式表微调)，训完对最后的快照做权重平均，得到 runs/<name>/final.pt。
改了招式表(xiaohua.txt)之后用它，不用重训。

用法(Train 下，用虚拟环境的python)：
  python scripts/resume.py                          从当前底座 models/base/base_avg.pt 出发，续训200轮
  python scripts/resume.py --from runs/resume/final.pt --iters 500     从上次续训的结果接着训500轮
  python scripts/resume.py --lr 5e-5 --name try2
没有列出的参数原样传给 train/train.py(如 --games 512 --eval_every 100)。
输出在 runs/<name>/ (默认 runs/resume)：final.pt、latest.pt、train.log、snaps/、table_used.txt；每次运行会先清空这个目录。
--from 可以是权重文件(.pt)，也可以是检查点；输入层缺的列会自动补零。
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as C

ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument('--from', dest='src', default=C.BASE_MODEL, help='从哪个模型出发(默认 models/base/base_avg.pt)')
ap.add_argument('--iters', type=int, default=200, help='续训多少轮(默认200)')
ap.add_argument('--name', default='resume', help='输出目录名 runs/<name>')
ap.add_argument('--table', default=C.TRAIN_TABLE, help='训练用的招式表(默认 Cpp/act/xiaohua.txt)')
ap.add_argument('--lr', type=float, default=1e-4, help='学习率(微调比从零训练小，默认1e-4)')
ap.add_argument('--seed', type=int, default=1)
ap.add_argument('--hours', type=float, default=0.0, help='最多训练多少小时，0=不限')
args, extra = ap.parse_known_args()

if not os.path.exists(args.src):
    sys.exit('找不到模型 ' + args.src)
C.train(os.path.join(C.RUNS, args.name), args.iters, args.table, extra, init=args.src, lr=args.lr, seed=args.seed, hours=args.hours)
