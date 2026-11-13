"""训练评测：训练完接着评测。不给 --from 就是重训(从零)，给了 --from 就是续训(从那个模型出发)。
训练用 complete.txt 里的无属性单效果家族当模板生成的招式池(家族属性随机)；评测默认评测 complete.txt 里混合抽样的25个家族(和 eval.py 一样)。

用法(Train 下，用虚拟环境的python)：
  python scripts/train_eval.py                                  重训200轮，再评测(1000盘)
  python scripts/train_eval.py --from models/base/base_avg.pt   续训200轮，再评测
  python scripts/train_eval.py --iters 500 --games 2000 --forms 20
训练参数同 retrain.py / resume.py；评测参数同 eval.py；没有列出的参数原样传给 train/train.py。
注意：这里的 --games 是评测盘数；训练每轮采样多少盘用 --train_games。
--table 是训练用的招式表(默认 complete.txt)，--eval_table 是评测用的(默认 complete.txt)，--select 选评测的家族(默认all=混合抽样25个)。
训练输出在 runs/<name>/ (默认 runs/train_eval)；评测结果在 results/ (覆盖上次)。评测的模型是训练得到的 runs/<name>/final.pt。
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as C

ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument('--from', dest='src', default=None, help='从哪个模型出发续训；不给=从零重训')
ap.add_argument('--iters', type=int, default=200, help='训练多少轮(默认200)')
ap.add_argument('--name', default='train_eval', help='训练输出目录名 runs/<name>')
ap.add_argument('--table', default=C.TRAIN_TABLE, help='训练用的招式表(默认 Cpp/act/complete.txt)')
ap.add_argument('--lr', type=float, default=None, help='学习率；续训默认1e-4，重训用train.py的默认3e-4')
ap.add_argument('--seed', type=int, default=1)
ap.add_argument('--hours', type=float, default=0.0, help='最多训练多少小时，0=不限')
ap.add_argument('--games', type=int, default=1000, help='评测：每个对局组合打多少盘(默认1000)')
ap.add_argument('--train_games', type=int, default=None, help='训练：每轮采样多少盘(不给=train.py的默认1024)')
ap.add_argument('--forms', type=int, nargs='+', default=[1, 6, 10, 14, 17, 20], help='评测：最高式')
ap.add_argument('--eval_table', default=C.EVAL_TABLE, help='评测用的招式表(默认 Cpp/act/complete.txt)')
ap.add_argument('--select', default='all', help='评测：哪些家族(名字片段，逗号分隔；默认all=表里的全部)')
args, extra = ap.parse_known_args()
if args.train_games:
    extra += ['--games', str(args.train_games)]

lr = args.lr if args.lr is not None else (1e-4 if args.src else None)
final = C.train(os.path.join(C.RUNS, args.name), args.iters, args.table, extra, init=args.src, lr=lr, seed=args.seed, hours=args.hours)
C.evaluate(final, args.eval_table, args.select, args.games, args.forms)
