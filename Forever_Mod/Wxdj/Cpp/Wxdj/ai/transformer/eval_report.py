"""招式强度报告：镜像对局(双方同一个招式文件、同样的属性)，只看自由对局，不做单招/双招矩阵。
用法：python eval_report.py [checkpoint] [式] [--games N]   默认 checkpoints_pvp/latest.pt、第20式

1. 自由对局：网络自博弈时每个家族被用作单招/双招的比例(双招两个成员都算)，以及去掉这个家族后的胜率。
2. 固定套路：每回合最常见的出招和全局最常见的出招。
"""
import os
import sys
import time

import numpy as np
import torch

from eval_moves import load, setup, reset, FAMS, NF, dev, M, winrate, run as free_run
from model import Policy
from train import net_actions
from collections import Counter

N_GAMES = 100     # 自由对局打多少盘；命令行 --games N 可改，越大越准越慢
T0 = time.time()
PROG = 'progress.txt'


def progress(msg, frac):
    """进度写到progress.txt：阶段、完成比例、已用/预计剩余时间。"""
    el = time.time() - T0
    eta = el * (1 - frac) / max(frac, 1e-6)
    with open(PROG, 'w', encoding='utf-8') as f:
        f.write('%s  总进度 %.0f%%  已用 %.1f分钟  预计剩余 %.1f分钟' % (msg, 100 * frac, el / 60, eta / 60) + '\n')



@torch.no_grad()
def routines(env, batch, net, rounds=8):
    """自由自博弈时每一回合的典型出招(固定套路)：返回 (按回合的标签计数, 全局的标签计数)。"""
    reset(env)
    N = env.n
    gall = torch.arange(N, device=dev)
    per_round = [Counter() for _ in range(rounds)]
    overall = Counter()
    for t in range(80):
        state, hist, done = env.obs()
        if done.all():
            break
        S, H = torch.from_numpy(state).to(dev), torch.from_numpy(hist).to(dev)
        act = np.zeros((N, 2, 3), dtype=np.int32)
        for s_ in (0, 1):
            c, a, b, *_ = net_actions(net, batch, S[:, s_], H[:, s_], gall, torch.full_like(gall, s_))
            act[:, s_] = torch.stack([c, a, b], 1).cpu().numpy()
        alive = np.where(done == 0)[0]
        for g in alive:
            for s_ in (0, 1):
                c, a, b = act[g, s_]
                if c == 0: lab = '跳过'
                elif c == 1: lab = '积攒'
                elif c == 2: lab = '炸弹'
                elif c == 3: lab = '单:' + FAMS[a]
                else: lab = '双:' + '+'.join(sorted([FAMS[a], FAMS[b]]))
                overall[lab] += 1
                if t < rounds: per_round[t][lab] += 1
        env.step(act)
    return per_round, overall


def main():
    ck = sys.argv[1] if len(sys.argv) > 1 else os.path.join('checkpoints_pvp', 'latest.pt')
    form = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2].isdigit() else 20
    if '--games' in sys.argv:
        i = sys.argv.index('--games')
        global N_GAMES
        N_GAMES = int(sys.argv[i + 1])
        del sys.argv[i:i + 2]
    global PROG, T0
    PROG = 'progress_%d.txt' % form
    T0 = time.time()
    net = load(ck)
    env, batch = setup(form, N_GAMES, os.path.join('pool_eval', 'moves'))
    assert env.nmoves(0, 0) == NF

    rec = dict(cls=np.zeros(5), single=np.zeros(NF), dual=np.zeros(NF))
    progress('自由对局', 0.05)
    base = free_run(env, batch, net, net, record=rec)
    single_use = rec['single'] / max(rec['single'].sum(), 1)
    dual_use = rec['dual'] / max(rec['dual'].sum(), 1)
    base_wr = winrate(env, batch, net, None)
    progress('去掉家族后的胜率', 0.3)
    ablate = []
    for f in range(NF):
        m = torch.ones(M, dtype=torch.bool, device=dev); m[f] = False
        ablate.append(winrate(env, batch, net, m))
        progress('去掉家族后的胜率 %d/%d' % (f + 1, NF), 0.3 + 0.6 * (f + 1) / NF)
    per_round, overall = routines(env, batch, net)
    progress('完成', 1.0)

    print('\n== 第%d式 招式强度报告 (%s) ==' % (form, os.path.basename(ck)))
    print('自由对局 胜/负/平 %.2f/%.2f/%.2f  招类使用率(跳过/积攒/炸弹/单招/双招) %s' % (
        (base == 1).mean(), (base == -1).mean(), (base == 0).mean(), np.round(rec['cls'] / rec['cls'].sum(), 3)))
    print('完整vs完整基线胜率 %.2f' % base_wr[0])
    print('\n%-6s | 自由对局用作单招 用作双招成员 去掉后胜率' % '家族')
    for f in np.argsort(-(single_use + dual_use)):
        print('%-6s | %5.1f%% %5.1f%% %.2f' % (FAMS[f], 100 * single_use[f], 100 * dual_use[f], ablate[f][0]))
    tot = sum(overall.values())
    print('\n== 固定套路：每回合最常见的出招(双方合计) ==')
    for t, cnt in enumerate(per_round):
        n = sum(cnt.values())
        print('第%d回合：' % (t + 1) + '，'.join('%s %.0f%%' % (k, 100 * v / max(n, 1)) for k, v in cnt.most_common(4)))
    print('全局最常见的出招：' + '，'.join('%s %.1f%%' % (k, 100 * v / tot) for k, v in overall.most_common(10)))
    ent = -sum(v / tot * np.log(v / tot) for v in overall.values())
    print('出招集中度：共出现%d种不同的出招，熵 %.2f(越小越固定)' % (len(overall), ent))


if __name__ == '__main__':
    main()
