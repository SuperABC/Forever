"""招式强度报告：单招评估 + 双招评估(有些招式本身就是给双招准备的)。镜像对局，双方同一个招式文件、同样的属性。
用法：python eval_report.py [checkpoint] [式]   默认 checkpoints/latest.pt、第20式

1. 自由对局：网络自博弈时每个家族被用作单招/双招的比例(双招两个成员都算)，以及去掉这个家族后的胜率。
2. 单招矩阵：双方各自只能用一个家族，招类限制成 跳过/积攒/炸弹/单招(没有双招)。
3. 双招矩阵：双方各自的双招里必须有一个成员是指定家族，另一个成员网络自己挑(可以搭配任何同式招式)，
   招类限制成 跳过/积攒/炸弹/双招(没有单招)。同时统计每个家族最常搭配的伙伴。
"""
import os
import sys
import time

import numpy as np
import torch

from eval_moves import load, setup, reset, FAMS, dev, M, winrate, run as free_run
from model import Policy
from train import net_actions
from collections import Counter

N_GAMES = 100
T0 = time.time()
PROG = 'progress.txt'


def progress(msg, frac):
    """进度写到progress.txt：阶段、完成比例、已用/预计剩余时间(单招矩阵和双招矩阵各占45%，自由对局占10%)。"""
    el = time.time() - T0
    eta = el * (1 - frac) / max(frac, 1e-6)
    with open(PROG, 'w', encoding='utf-8') as f:
        f.write('%s  总进度 %.0f%%  已用 %.1f分钟  预计剩余 %.1f分钟' % (msg, 100 * frac, el / 60, eta / 60) + '\n')



@torch.no_grad()
def restricted(net, batch, state, hist, g, s, classes, m1_family):
    """限制招类和第一个招式的下一步。classes是允许的招类集合，m1_family是第一个招式必须是的家族下标。"""
    with torch.autocast('cuda', dtype=torch.bfloat16, enabled=dev.type == 'cuda'):
        es, eo, mask_s, mask_o, form_s = batch.inputs(g, s, net, False)
        h, ctx, v, cl, l1 = net(state, hist, es, eo, mask_s, mask_o, form_s)
    cmask = torch.zeros(5, dtype=torch.bool, device=dev)
    cmask[list(classes)] = True
    cl = cl.masked_fill(~cmask, -1e9)
    only = torch.zeros(M, dtype=torch.bool, device=dev)
    only[m1_family] = True
    l1 = l1.masked_fill(~only[None, :], -1e9)
    sample = lambda lg: torch.distributions.Categorical(logits=lg.float()).sample()
    c, m1 = sample(cl), sample(l1)
    l2 = net.second_logits(h, ctx, m1, net.same_form_mask(form_s, mask_s, m1))
    m2 = sample(l2)
    return c, m1, m2


@torch.no_grad()
def play(env, batch, net, ch0, ch1, rec=None):
    """ch0/ch1(state, hist, g, s)->(cls,m1,m2)。返回outcome(+1席位0胜)。"""
    N = env.n
    reset(env)
    gall = torch.arange(N, device=dev)
    outcome = np.zeros(N, dtype=np.int32)
    done_all = np.zeros(N, dtype=bool)
    for t in range(80):
        state, hist, done = env.obs()
        if done.all():
            break
        S, H = torch.from_numpy(state).to(dev), torch.from_numpy(hist).to(dev)
        act = np.zeros((N, 2, 3), dtype=np.int32)
        for s, ch in ((0, ch0), (1, ch1)):
            c, a, b = ch(S[:, s], H[:, s], gall, torch.full_like(gall, s))
            act[:, s] = torch.stack([c, a, b], 1).cpu().numpy()
        if rec is not None:
            alive = done == 0
            for s in range(2):
                cls = act[alive, s, 0]
                dual = cls == 4
                if dual.any():
                    # 双招的两个成员；统计其中一个是fam的时候另一个是谁
                    rec[s]['pairs'].append(np.stack([act[alive, s, 1][dual], act[alive, s, 2][dual]], 1))
                rec[s]['cls'] += np.bincount(cls, minlength=5)
        reward, d, out = env.step(act)
        newly = (d == 1) & ~done_all
        outcome[newly] = out[newly]
        done_all |= d == 1
    return outcome


def matrix(env, batch, net, classes, label, base=0.1, span=0.45):
    W = np.zeros((15, 15))
    partners = np.zeros((15, 15))      # partners[i][j]：i坐镇的双招里，另一个成员是j的次数
    skips = np.zeros(15); counts = np.zeros(15)
    for i in range(15):
        for j in range(i, 15):
            res = []
            for a, b in ((i, j), (j, i)):
                rec = [dict(cls=np.zeros(5), pairs=[]) for _ in range(2)]
                ch0 = lambda S, H, g, s, a=a: restricted(net, batch, S, H, g, s, classes, a)
                ch1 = lambda S, H, g, s, b=b: restricted(net, batch, S, H, g, s, classes, b)
                o = play(env, batch, net, ch0, ch1, rec)
                res.append((a, b, o))
                for seat, fam in ((0, a), (1, b)):
                    skips[fam] += rec[seat]['cls'][0]; counts[fam] += rec[seat]['cls'].sum()
                    for p in rec[seat]['pairs']:
                        for x, y in p:
                            partners[fam][y if x == fam else x] += 1
            o1, o2 = res[0][2], res[1][2]
            wi = ((o1 == 1).sum() + (o2 == -1).sum() + 0.5 * ((o1 == 0).sum() + (o2 == 0).sum())) / (len(o1) + len(o2))
            W[i, j] = wi; W[j, i] = 1 - wi
        done_cells = sum(15 - k for k in range(i + 1))
        progress('%s %d/15' % (label, i + 1), base + span * done_cells / 120)
    return W, partners, skips / np.maximum(counts, 1)


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
    ck = sys.argv[1] if len(sys.argv) > 1 else os.path.join('checkpoints', 'latest.pt')
    form = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2].isdigit() else 20
    nobomb = 'nobomb' in sys.argv      # 矩阵里禁止积攒/炸弹(炸弹套路会让低阶对局几乎全部同归于尽，家族强度测不出来)
    global PROG, T0
    PROG = 'progress_%d.txt' % form
    T0 = time.time()
    net = load(ck)
    env, batch = setup(form, N_GAMES, os.path.join('pool_eval', 'moves'))
    assert env.nmoves(0, 0) == 15

    ablate = [(float('nan'),)] * 15
    single_use = dual_use = np.zeros(15)
    rec = dict(cls=np.zeros(5), single=np.zeros(15), dual=np.zeros(15))
    base = np.zeros(1); base_wr = (float('nan'),)
    per_round = overall = None
    if not nobomb:
        # 1. 自由对局
        progress('自由对局', 0.01)
        base = free_run(env, batch, net, net, record=rec)
        single_use = rec['single'] / max(rec['single'].sum(), 1)
        dual_use = rec['dual'] / max(rec['dual'].sum(), 1)
        base_wr = winrate(env, batch, net, None)
        ablate = []
        for f in range(15):
            m = torch.ones(M, dtype=torch.bool, device=dev); m[f] = False
            ablate.append(winrate(env, batch, net, m))
        per_round, overall = routines(env, batch, net)
        progress('自由对局统计', 0.1)

    # 2/3. 矩阵
    c1, c2 = ((0, 3), (0, 4)) if nobomb else ((0, 1, 2, 3), (0, 1, 2, 4))
    progress('自由对局统计结束', 0.1)
    W1, _, skip1 = matrix(env, batch, net, c1, '单招矩阵', 0.1, 0.45)
    W2, partners, skip2 = matrix(env, batch, net, c2, '双招矩阵', 0.55, 0.45)
    mean1 = (W1.sum(1) - 0.5) / 14
    mean2 = (W2.sum(1) - 0.5) / 14

    print('\n== 第%d式 招式强度报告 (%s)%s ==' % (form, os.path.basename(ck), ' 【矩阵里禁止积攒/炸弹】' if nobomb else ''))
    if not nobomb:
        print('自由对局 胜/负/平 %.2f/%.2f/%.2f  招类使用率(跳过/积攒/炸弹/单招/双招) %s' % (
            (base == 1).mean(), (base == -1).mean(), (base == 0).mean(), np.round(rec['cls'] / rec['cls'].sum(), 3)))
        print('完整vs完整基线胜率 %.2f' % base_wr[0])
    print('\n%-6s | 自由对局用作单招 用作双招成员 去掉后胜率 | 单招矩阵 双招矩阵 平均 | 双招最常搭配' % '家族')
    order = np.argsort(-(mean1 + mean2))
    for f in order:
        top = np.argsort(-partners[f])[:3]
        tot = partners[f].sum()
        ptxt = '、'.join('%s%.0f%%' % (FAMS[p], 100 * partners[f][p] / max(tot, 1)) for p in top if partners[f][p] > 0)
        print('%-6s | %5.1f%% %5.1f%% %.2f | %.2f %.2f %.2f | %s' % (
            FAMS[f], 100 * single_use[f], 100 * dual_use[f], ablate[f][0], mean1[f], mean2[f], (mean1[f] + mean2[f]) / 2, ptxt))
    if not nobomb:
        tot = sum(overall.values())
        print('\n== 固定套路：每回合最常见的出招(双方合计) ==')
        for t, cnt in enumerate(per_round):
            n = sum(cnt.values())
            print('第%d回合：' % (t + 1) + '，'.join('%s %.0f%%' % (k, 100 * v / max(n, 1)) for k, v in cnt.most_common(4)))
        print('全局最常见的出招：' + '，'.join('%s %.1f%%' % (k, 100 * v / tot) for k, v in overall.most_common(10)))
        ent = -sum(v / tot * np.log(v / tot) for v in overall.values())
        print('出招集中度：共出现%d种不同的出招，熵 %.2f(越小越固定)' % (len(overall), ent))
    print('\n单招矩阵里跳过占比 %.1f%%，双招矩阵里跳过占比 %.1f%%' % (100 * skip1.mean(), 100 * skip2.mean()))


if __name__ == '__main__':
    main()
