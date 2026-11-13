"""评测的公共函数(给 eval_report.py 用)：用训练好的网络在镜像对局(双方同一个招式文件、同样的属性)里评估各招式家族。
评测用的招式文件 = 招式表里被选中的家族(WXDJ_SELECT，见 gen_pool.py)在"以该式为最高式的5式窗口"里的招式(每个家族 WINDOW 个招式，家族下标 = 招式下标 // 窗口长度)。
指标：使用率(网络自博弈时每个家族被选的比例)、去掉后胜率(一方把这个家族去掉，对完整招式的胜率，越低=这个家族越重要)。
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                                              # Train
sys.path[:0] = [os.path.join(ROOT, 'train'), os.path.join(ROOT, 'env')]   # train.py、model.py、gen_pool.py 在 train/，wxenv.py 在 env/

import numpy as np
import torch

import gen_pool
import wxenv
from model import Policy
from train import Pool, Batch, M, net_actions

dev = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
FAMS = [f.name for f in gen_pool.REAL]
NF = len(FAMS)      # 被评测的家族数


def load(path):
    n = Policy().to(dev).eval()
    ck = torch.load(path, map_location=dev, weights_only=False)
    n.load_state_dict(ck['net'] if 'net' in ck else ck)
    return n


def setup(top, n_games, tmp_dir):
    """以top为最高式的窗口文件(xiaohua真实家族)。env.win = 窗口长度(每个家族的招式数)。"""
    forms = gen_pool.window_forms(top)
    path = gen_pool.write_real(tmp_dir, [forms])[0]
    pool = Pool([path]); pool.load()
    st = pool.stats[0]
    env = wxenv.Env(n_games, M)
    env.win = len(forms)
    env.stat = [st[0], st[1], st[2], gen_pool.std_agi(st[4]), st[3], 1.0, 0.2, st[4]]
    reset(env)
    return env, Batch(env, dev)


def fam_mask(env, f, value):
    """家族f的整个窗口的招式掩码：只有这些招式为value，其余为(not value)。"""
    m = torch.full((M,), not value, dtype=torch.bool, device=dev)
    m[f * env.win:(f + 1) * env.win] = value
    return m


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
                W = env.win
                record['single'] += np.bincount(m1[cls == 3] // W, minlength=NF)[:NF]
                record['dual'] += np.bincount(m1[cls == 4] // W, minlength=NF)[:NF] + np.bincount(m2[cls == 4] // W, minlength=NF)[:NF]
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
