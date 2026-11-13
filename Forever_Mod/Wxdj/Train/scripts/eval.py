"""仅评测：评测一个模型在最高式 1/6/10/14/17/20 的5式窗口上的表现，结果写到 results/ (每次覆盖上次)。
默认评测 Cpp/act/complete.txt：98个家族超过环境容量，不带 --select 时自动混合抽样25个家族(每种效果至少1个、每种属性至少3个，余下随机，固定种子，见 gen_pool.mixed_sample)。

用法(Train 下，用虚拟环境的python)：
  python scripts/eval.py                                  默认：当前底座 models/base/base_avg.pt(没有就用 runs/ 下最近训完的 final.pt)，评测 complete.txt 里混合抽样的25个家族，每个对局组合1000盘
  python scripts/eval.py runs/resume/final.pt --games 3000
  python scripts/eval.py --forms 20 10                    只看最高式20和10
  python scripts/eval.py --select 火属性                  只评测名字含"火属性"的家族；--table 可换成别的招式表(如 xiaohua.txt，用绝对路径)
模型可以是权重文件(.pt)，也可以是 runs/<name> 目录(用里面的 final.pt)。
评测的家族数有上限(每方125个招式 = 25个家族 x 5式)：--select 选出的家族超过25个会报错；
(名字里含这些片段的家族，逗号分隔，如 火属性、穿透、穿透,减伤)。
这张表没有各阶位三维表时(如 xiaohua.txt)，评测会自动从 complete.txt 补上，三维始终以 complete.txt 里的为准。
输出 results/：report_<式>.txt(每个式的详细报告)、report_all.md(汇总)、table_used.txt(评测用的招式表副本)。
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as C

ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument('model', nargs='?', default=C.default_model(), help='模型(.pt 或 runs/<name> 目录)，默认 models/base/base_avg.pt；没有它就用 runs/ 下最近训完的 final.pt')
ap.add_argument('--games', type=int, default=1000, help='每个对局组合打多少盘(默认1000，越大越准越慢)')
ap.add_argument('--forms', type=int, nargs='+', default=[1, 6, 10, 14, 17, 20], help='评测的最高式(各取以它为最高式的5式窗口)')
ap.add_argument('--table', default=C.EVAL_TABLE, help='评测用的招式表(默认 Cpp/act/complete.txt)')
ap.add_argument('--select', default='all', help='评测哪些家族：名字里含这些片段(逗号分隔)的；默认all=表里的全部(超过25个家族时混合抽样25个)')
ap.add_argument('--no_bomb', '--no-bomb', action='store_true', help='评测时禁用真气炸弹：屏蔽"积攒"和"炸弹"两种行动(双方都禁用)，模型本身不改')
args = ap.parse_args()

if args.no_bomb:
    os.environ['WXDJ_NO_BOMB'] = '1'    # 评测子进程继承；model.py 据此屏蔽这两个行动类别
C.evaluate(args.model, args.table, args.select, args.games, args.forms)
