"""用训练好的网络评估各招式家族的强度(镜像对局：双方同一个招式文件、同样的属性)。

三种指标：
  使用率  网络自博弈时每个家族被选的比例(它自己觉得哪个好)
  去掉后  一方把这个家族从可用招式里去掉，对另一方(完整招式)的胜率变化 —— 越低=这个家族越重要
  单独用  一方只能用这个家族的招式(外加积攒/炸弹/跳过)，对另一方(完整招式)的胜率 —— 越高=这个家族单独越强
用法：python eval_moves.py [checkpoint] [式]   默认 checkpoints_pvp/latest.pt、第20式
"""
import os
import sys

import numpy as np
import torch

import gen_pool
import wxenv
from model import Policy
from train import Pool, Batch, M, net_actions

dev = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
FAMS = [f.name for f in gen_pool.REAL]
NF = len(FAMS)      # 真实家族数(现在17个：含火之高兴、安如磐石)


def load(path):
    n = Policy().to(dev).eval()
    ck = torch.load(path, map_location=dev, weights_only=False)
    n.load_state_dict(ck['net'] if 'net' in ck else ck)
    return n


def setup(form, n_games, tmp_dir):
    path = gen_pool.write_real(tmp_dir, [[form]])[0]
    pool = Pool([path]); pool.load()
    st = pool.stats[0]
    env = wxenv.Env(n_games, M)
    env.stat = [st[0], st[1], st[2], gen_pool.std_agi(st[4]), st[3], 1.0, 0.2, st[4]]
    reset(env)
    return env, Batch(env, dev)


def reset(env):
    for g in range(env.n):
        env.reset(g, 0, 0, env.stat, env.stat)


@torch.no_grad()
def run(env, batch, net0, net1, mask0=None, mask1=None, record=None):
    """席位0用net0(可限制可用招式mask0)，席位1用net1(mask1)。返回outcome(+1席位0胜)。"""
    N = env.n
    reset(env)
    gall = torch.arange(N, device=dev)
    valid = batch.valid.clone()
    if mask0 is not None: batch.valid[:, 0, :] &= mask0
    if mask1 is not None: batch.valid[:, 1, :] &= mask1
    outcome = np.zeros(N, dtype=np.int32); done_all = np.zeros(N, dtype=bool)
    for t in range(80):
        state, hist, done = env.obs()
        if done.all(): break
        S, H = torch.from_numpy(state).to(dev), torch.from_numpy(hist).to(dev)
        act = np.zeros((N, 2, 3), dtype=np.int32)
        for s, net in ((0, net0), (1, net1)):
            c, a, b, *_ = net_actions(net, batch, S[:, s], H[:, s], gall, torch.full_like(gall, s))
            act[:, s] = torch.stack([c, a, b], 1).cpu().numpy()
        if record is not None:
            alive = done == 0
            for s in range(2):
                cls, m1, m2 = act[alive, s, 0], act[alive, s, 1], act[alive, s, 2]
                record['cls'] += np.bincount(cls, minlength=5)
                record['single'] += np.bincount(m1[cls == 3], minlength=NF)[:NF]
                record['dual'] += np.bincount(m1[cls == 4], minlength=NF)[:NF] + np.bincount(m2[cls == 4], minlength=NF)[:NF]
        reward, d, out = env.step(act)
        newly = (d == 1) & ~done_all
        outcome[newly] = out[newly]
        done_all |= d == 1
    batch.valid = valid
    return outcome


def winrate(env, batch, net, maskA=None):
    """A(网络，可限制招式)对 B(网络，完整招式)，席位各打一半。返回 A 胜/败/平 比例。"""
    o1 = run(env, batch, net, net, mask0=maskA)
    o2 = run(env, batch, net, net, mask1=maskA)       # A坐席位1
    w = (o1 == 1).sum() + (o2 == -1).sum(); l = (o1 == -1).sum() + (o2 == 1).sum()
    n = len(o1) + len(o2)
    return w / n, l / n, (n - w - l) / n


def main():
    games = 600     # 每个对局组合打多少盘；命令行 --games N 可改
    if '--games' in sys.argv:
        i = sys.argv.index('--games')
        games = int(sys.argv[i + 1])
        del sys.argv[i:i + 2]
    ck = sys.argv[1] if len(sys.argv) > 1 else os.path.join('checkpoints_pvp', 'latest.pt')
    form = int(sys.argv[2]) if len(sys.argv) > 2 else 20
    net = load(ck)
    env, batch = setup(form, games, os.path.join('pool_eval', 'moves'))
    nm = env.nmoves(0, 0)
    assert nm == NF

    rec = dict(cls=np.zeros(5), single=np.zeros(NF), dual=np.zeros(NF))
    base = run(env, batch, net, net, record=rec)
    print('== 第%d式，%s ==' % (form, os.path.basename(ck)))
    print('自博弈：胜负平 %.2f/%.2f/%.2f 招类使用率(跳过/积攒/炸弹/单招/双招) %s' % (
        (base == 1).mean(), (base == -1).mean(), (base == 0).mean(), np.round(rec['cls'] / rec['cls'].sum(), 3)))
    use = rec['single'] + rec['dual']
    use = use / use.sum()

    rows = []
    full = winrate(env, batch, net, None)
    for f in range(NF):
        mask_wo = torch.ones(M, dtype=torch.bool, device=dev); mask_wo[f] = False
        mask_only = torch.zeros(M, dtype=torch.bool, device=dev); mask_only[f] = True
        wo = winrate(env, batch, net, mask_wo)
        only = winrate(env, batch, net, mask_only)
        rows.append((FAMS[f], use[f], wo, only))
        print('  %-6s 使用率%5.1f%%  去掉后胜率%.2f(败%.2f)  单独用胜率%.2f(败%.2f)' % (FAMS[f], use[f] * 100, wo[0], wo[1], only[0], only[1]), flush=True)
    print('基线(完整vs完整) 胜率%.2f 败%.2f' % (full[0], full[1]))


if __name__ == '__main__':
    main()
