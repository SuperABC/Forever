"""对训练最后的若干个快照做权重平均，得到一个单网络(上一轮实验里它比任何单个模型都更强、更不容易被针对)。
快照来自 train.py 存的 <ckpt_dir>/snaps/snap_XXXXX.pt(fp16，XXXXX 是存它时已训的轮数，每隔 --snap_every 轮一个)。

用法：python avg_weights.py <ckpt_dir> [--count 8] [--stride N] [--out final.pt]
  默认(训练满1000轮以上)：取最近1000轮里每隔100轮的一个快照，共10个(最后一轮、往前100轮、...、往前900轮，各取离它最近的快照)做平均。
  训练不到1000轮(或快照凑不出2个)时退化成旧逻辑：取最后的 count 个快照，每隔 stride 个取一个；不给 --stride 就自动选，
  让这 count 个快照大约覆盖全部快照的最后 20%(200轮的训练 -> 间隔1，覆盖最后40轮；3000轮的训练 -> 间隔15，覆盖最后600轮)。
  明确给了 --count 或 --stride 时也用旧逻辑。
  输出纯 state_dict，可直接给 train.py --init / export_weights.py / eval 用。
"""
import os
import re
import sys

import torch


RECENT, EVERY = 1000, 100      # 最近1000轮，每100轮取一个(共10个)


def pick_recent(ckpt_dir):
    """最近 RECENT 轮里每隔 EVERY 轮一个快照(离目标轮数最近的那个)，共 RECENT//EVERY 个；训练总轮数不到 RECENT 或凑不出2个不同的快照就返回 []。"""
    d = os.path.join(ckpt_dir, 'snaps')
    if not os.path.isdir(d):
        return []
    its = {}
    for f in os.listdir(d):
        m = re.match(r'^snap_(\d+)\.pt$', f)
        if m:
            its[int(m.group(1))] = os.path.join(d, f)
    if not its or max(its) < RECENT:
        return []
    last = max(its)
    chosen = []
    for j in range(RECENT // EVERY):
        target = last - j * EVERY
        near = min(its, key=lambda t: (abs(t - target), -t))
        if its[near] not in chosen:
            chosen.append(its[near])
    return chosen[::-1] if len(chosen) >= 2 else []


def pick_snapshots(ckpt_dir, count=8, stride=0):
    d = os.path.join(ckpt_dir, 'snaps')
    if not os.path.isdir(d):
        return []
    fs = sorted(f for f in os.listdir(d) if f.startswith('snap_') and f.endswith('.pt'))
    if stride <= 0:
        stride = max(1, round(0.2 * len(fs) / count))
    pick = fs[::-1][::stride][:count]
    return [os.path.join(d, f) for f in pick[::-1]]


def average(paths):
    sds = []
    for p in paths:
        ck = torch.load(p, map_location='cpu', weights_only=False)
        sds.append(ck['net'] if isinstance(ck, dict) and 'net' in ck else ck)
    return {k: (sum(sd[k].float() for sd in sds) / len(sds)) if sds[0][k].is_floating_point() else sds[0][k] for k in sds[0]}


def main():
    argv = sys.argv[1:]

    def opt(name, default):
        if name in argv:
            i = argv.index(name)
            v = argv[i + 1]
            del argv[i:i + 2]
            return type(default)(v)
        return default

    explicit = '--count' in argv or '--stride' in argv
    count, stride, out = opt('--count', 8), opt('--stride', 0), opt('--out', 'final.pt')
    if not argv:
        sys.exit(__doc__)
    paths = [] if explicit else pick_recent(argv[0])
    if not paths:      # 训练不到1000轮、快照不够、或明确给了 --count/--stride：旧逻辑
        paths = pick_snapshots(argv[0], count, stride)
    if len(paths) < 2:
        sys.exit('快照不足(找到 %d 个)：%s' % (len(paths), os.path.join(argv[0], 'snaps')))
    torch.save(average(paths), out)
    print('平均了 %d 个快照: %s ... %s -> %s' % (len(paths), os.path.basename(paths[0]), os.path.basename(paths[-1]), out))


if __name__ == '__main__':
    main()
