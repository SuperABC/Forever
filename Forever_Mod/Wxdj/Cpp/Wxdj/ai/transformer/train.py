"""PPO自博弈训练通用对战模型。

每轮：随机抽N盘对局(双方的招式文件和属性各自随机)，对手是 自己(两个席位都学习) / 历史快照 / 脚本机器人，
并行打完 -> 算优势 -> PPO更新。定期评测并存检查点。
用法：python train.py                    从头训练PvP模型(检查点在checkpoints_pvp/)
      python train.py --resume           接着latest.pt训练
      python train.py --iters 500        这次训练500轮(接着训练时是再训500轮)
      python train.py --hours 6          最多训练6小时(--hours和--iters/--max_iters谁先到谁停；--hours默认0=不限时间)
      python train.py --mode pve --init checkpoints_pvp/latest.pt --lr 1.5e-4
                                         PvE微调：双方带随机的五行属性/暴击/特性，特征在PvP维数后面追加，
                                         从PvP检查点补零列热启动，检查点单独存在checkpoints_pve/
"""
import argparse
import copy
import json
import os
import time

import numpy as np
import torch
import torch.nn.functional as F

import bots
import gen_pool
import pve_gen
import wxenv
from model import Policy, param_count
from pad_checkpoint import pad_to_shapes

HERE = os.path.dirname(os.path.abspath(__file__))
POOL_DIR = os.path.join(HERE, 'pool')
EVAL_DIR = os.path.join(HERE, 'pool_eval')
M = 64


def log(msg, f=None):
    print(msg, flush=True)
    if f:
        f.write(msg + '\n'); f.flush()


class Pool:
    """一批招式文件，加载进环境。"""

    def __init__(self, paths):
        self.paths = paths
        self.stats = np.array([gen_pool.read_stats(p) for p in paths], dtype=np.float64)   # hp atk mp rec

    def load(self):
        """环境里的招式文件池是全局的，换用哪个池就要重新载入。"""
        wxenv.pool_clear()
        for p in self.paths:
            assert wxenv.pool_add(p) >= 0, p


def sample_games(pool, n, rng, mirror=0.4, jitter=0.4):
    """随机抽n盘对局的(甲文件, 乙文件, 甲属性, 乙属性)。属性 = hp, atk, mp, agi, rec, 跳过损失敏捷, 每回合恢复敏捷, 炸弹积攒倍率。"""
    ta = rng.integers(0, len(pool.paths), n)
    mir = rng.random(n) < mirror
    tb = np.where(mir, ta, rng.integers(0, len(pool.paths), n))

    mults = [0.5, 1.0, 1.2, 1.5, 1.9, 2.4, 3.0, 3.7, 4.5, 5.6, 7.0, 8.5, 10.0, 12.0]   # 随机倍率范围比标准值(1~10)更宽

    def side(stats, base_agi, wide):
        j = lambda: float(np.exp(rng.normal(0, jitter)))
        agi = float(np.exp(rng.uniform(np.log(3), np.log(30)))) if wide else base_agi
        loss = 1.0 if rng.random() < 0.5 else float(rng.choice([0.5, 1.0, 1.5, 2.0]))
        regen = 0.2 if rng.random() < 0.5 else float(rng.choice([0.1, 0.2, 0.4, 0.6]))
        mult = stats[4] if rng.random() < 0.8 else float(rng.choice(mults))   # 20%的对局随机倍率，让网络学会读这个属性
        return [stats[0] * j(), stats[1] * j(), stats[2] * j(), agi, stats[3], loss, regen, mult]

    sa, sb = [], []
    for g in range(n):
        a, b = pool.stats[ta[g]], pool.stats[tb[g]]
        if mir[g]:
            x = [a[0], a[1], a[2], gen_pool.std_agi(a[4]), a[3], 1.0, 0.2, a[4]]   # 标准敏捷：第1式5.25，每式+0.25，每阶对齐最大式 -> 5+最大式/4
            sa.append(x); sb.append(list(x))
        else:
            wide = rng.random() < 0.4
            sa.append(side(a, gen_pool.std_agi(a[4]), wide))
            sb.append(side(b, float(np.clip(rng.normal(gen_pool.std_agi(b[4]), 2.0), 2, 25)), wide))
    return ta, tb, np.array(sa, dtype=np.float32), np.array(sb, dtype=np.float32)


def reset_games(env, ta, tb, sa, sb, rng, mirror=False):
    """重置所有对局。PvE模式下每盘再随机一份五行属性/暴击/特性配置(见pve_gen.py)；mirror=True时双方配置一样(评测要公平)。"""
    for g in range(env.n):
        cfg = pve_gen.sample_config(rng, sa[g][1], sb[g][1], mirror=mirror) if wxenv.PVE else None
        env.reset(g, ta[g], tb[g], sa[g], sb[g], pve=cfg)


class Batch:
    """把环境里的招式数组放到GPU，并提供按(游戏,席位)取出模型输入的方法。"""

    def __init__(self, env, dev):
        self.env, self.dev = env, dev
        self.refresh()

    def refresh(self):
        e = self.env
        self.moves = torch.from_numpy(e.moves).to(self.dev)
        self.form = torch.from_numpy(e.form).to(self.dev)
        self.valid = self.form != -2
        self.emb = None

    def cache(self, net):
        """提前算好全部招式向量(采样时用，一盘对局里招式不变)。"""
        with torch.no_grad(), torch.autocast('cuda', dtype=torch.bfloat16, enabled=self.dev.type == 'cuda'):
            self.emb = net.move_embed(self.moves).float()

    def inputs(self, g, s, net=None, use_cache=False):
        """g, s: 长度B的LongTensor(GPU)。返回 es, eo, mask_s, mask_o, form_s (es/eo是招式向量)"""
        if use_cache and self.emb is not None:
            es, eo = self.emb[g, s], self.emb[g, 1 - s]
        else:
            es, eo = net.move_embed(self.moves[g, s]), net.move_embed(self.moves[g, 1 - s])
        return (es, eo, self.valid[g, s], self.valid[g, 1 - s], self.form[g, s])


@torch.no_grad()
def net_actions(net, batch, state, hist, g, s, greedy=False, use_cache=False):
    with torch.autocast('cuda', dtype=torch.bfloat16, enabled=batch.dev.type == 'cuda'):
        es, eo, mask_s, mask_o, form_s = batch.inputs(g, s, net, use_cache)
        return net.act(state, hist, es, eo, mask_s, mask_o, form_s, greedy=greedy)


def play(env, batch, net0, net1, bot1, bot_fam1, n_steps, rng, dev, greedy=False, net0_bot=None):
    """评测用：席位0是net0，席位1是net1或脚本机器人。返回outcome数组(+1席位0胜)。"""
    N = env.n
    gall = torch.arange(N, device=dev)
    outcome = np.zeros(N, dtype=np.int32)
    done_all = np.zeros(N, dtype=bool)
    lengths = np.zeros(N, dtype=np.int32)
    for t in range(n_steps):
        state, hist, done = env.obs()
        if done.all():
            break
        S, H = torch.from_numpy(state).to(dev), torch.from_numpy(hist).to(dev)
        act = np.zeros((N, 2, 3), dtype=np.int32)
        c0, a0, b0, *_ = net_actions(net0, batch, S[:, 0], H[:, 0], gall, torch.zeros_like(gall), greedy)
        act[:, 0] = torch.stack([c0, a0, b0], 1).cpu().numpy()
        if net1 is not None:
            c1, a1, b1, *_ = net_actions(net1, batch, S[:, 1], H[:, 1], gall, torch.ones_like(gall), greedy)
            act[:, 1] = torch.stack([c1, a1, b1], 1).cpu().numpy()
        else:
            act[:, 1] = bots.actions(env, np.arange(N), 1, bot1, bot_fam1, rng)
        reward, d, out = env.step(act)
        newly = (d == 1) & ~done_all
        lengths[newly] = t + 1
        outcome[newly] = out[newly]
        done_all |= d == 1
    return outcome, lengths, done_all


EMA = {}


def evaluate(net, eval_pool, dev, rng, n_games=256, tag=''):
    """对每种脚本机器人打n_games盘(双方席位各一半)，返回胜率/败率。"""
    eval_pool.load()
    env = wxenv.Env(n_games, M)
    res = {}
    cases = [(bt, None, bots.NAMES[bt]) for bt in range(3)] + [(bots.ROUTINE, k, r[0]) for k, r in enumerate(bots.ROUTINES)]
    for bt, rk, name in cases:      # 每个固定套路单独评测
        wins = losses = 0
        for swap in (0, 1):
            ta, tb, sa, sb = sample_games(eval_pool, n_games, rng, mirror=1.0)   # 镜像=公平，胜率才反映模型本身的强弱
            reset_games(env, ta, tb, sa, sb, rng, mirror=True)
            if swap == 1:    # 让net坐席位1：用一个包装把两个席位的角色互换
                outcome = play_swapped(env, net, bt, rng, dev, rk)
            else:
                batch = Batch(env, dev)
                fam = bots.pick_family(env, np.arange(n_games), 1, rng)
                if rk is not None:
                    fam = np.full(n_games, rk)
                outcome, _, _ = play(env, batch, net, None, np.full(n_games, bt), fam, 80, rng, dev)
            wins += int((outcome == 1).sum()); losses += int((outcome == -1).sum())
        total = 2 * n_games
        res[name] = (wins / total, losses / total)
    return res


def play_swapped(env, net, bt, rng, dev, rk=None):
    """net坐席位1，机器人坐席位0。返回以net为视角的outcome(+1=net胜)。"""
    N = env.n
    batch = Batch(env, dev)
    gall = torch.arange(N, device=dev)
    fam = bots.pick_family(env, np.arange(N), 0, rng)
    if rk is not None:
        fam = np.full(N, rk)
    outcome = np.zeros(N, dtype=np.int32)
    done_all = np.zeros(N, dtype=bool)
    for t in range(80):
        state, hist, done = env.obs()
        if done.all():
            break
        S, H = torch.from_numpy(state).to(dev), torch.from_numpy(hist).to(dev)
        act = np.zeros((N, 2, 3), dtype=np.int32)
        c1, a1, b1, *_ = net_actions(net, batch, S[:, 1], H[:, 1], gall, torch.ones_like(gall))
        act[:, 1] = torch.stack([c1, a1, b1], 1).cpu().numpy()
        act[:, 0] = bots.actions(env, np.arange(N), 0, np.full(N, bt), fam, rng)
        reward, d, out = env.step(act)
        newly = (d == 1) & ~done_all
        outcome[newly] = -out[newly]
        done_all |= d == 1
    return outcome


def gae(rewards, values, dones, boot, gamma, lam):
    """rewards/values/dones: (T,N,2)；boot: (N,2) 最后一步之后的价值。"""
    T = rewards.shape[0]
    adv = np.zeros_like(rewards)
    last = np.zeros_like(boot)
    for t in reversed(range(T)):
        nextv = values[t + 1] if t + 1 < T else boot
        nonterm = 1.0 - dones[t]
        delta = rewards[t] + gamma * nextv * nonterm - values[t]
        last = delta + gamma * lam * nonterm * last
        adv[t] = last
    return adv


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--games', type=int, default=512)
    ap.add_argument('--steps', type=int, default=56)
    ap.add_argument('--pool', type=int, default=3000)
    ap.add_argument('--lr', type=float, default=3e-4)
    ap.add_argument('--epochs', type=int, default=3)
    ap.add_argument('--mb', type=int, default=2048)
    ap.add_argument('--hours', type=float, default=0.0, help='最多训练多少小时，0=不限时间')
    ap.add_argument('--iters', type=int, default=0, help='这次训练多少轮(接着训练时是"再训多少轮")，0=不限；和--hours/--max_iters谁先到谁停')
    ap.add_argument('--max_iters', type=int, default=10 ** 9, help='训练到第几轮为止(轮数从0起算，续训时包含之前已经训练的轮数)')
    ap.add_argument('--eval_every', type=int, default=50)
    ap.add_argument('--eval_games', type=int, default=256, help='每次评测对每种机器人、每个席位打多少盘')
    ap.add_argument('--resume', action='store_true')
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--d', type=int, default=128)
    ap.add_argument('--mode', choices=['pvp', 'pve'], default='pvp', help='pvp=和游戏里PvP模型一样的特征；pve=双方有五行属性/暴击/特性，特征追加维度')
    ap.add_argument('--init', default=None, help='热启动：从这个检查点读权重(输入层缺的列补零)，常用于从PvP模型出发训练PvE模型')
    ap.add_argument('--ckpt_dir', default=None, help='检查点目录，默认pvp是checkpoints_pvp，pve是checkpoints_pve')
    args = ap.parse_args()

    wxenv.set_mode(args.mode == 'pve')     # 必须在创建Env和模型之前：它决定特征维数
    CKPT = os.path.join(HERE, args.ckpt_dir or ('checkpoints_pve' if args.mode == 'pve' else 'checkpoints_pvp'))
    dev = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    os.makedirs(CKPT, exist_ok=True)
    logf = open(os.path.join(CKPT, 'train.log'), 'a', encoding='utf-8')
    rng = np.random.default_rng(args.seed)
    torch.manual_seed(args.seed)

    eval_paths = gen_pool.generate(EVAL_DIR, 150, 99991) + gen_pool.write_real(
        os.path.join(EVAL_DIR, 'real'), [[1], [3], [6], [10], [14], [17], [20], [18, 19, 20]])
    eval_pool = Pool(eval_paths)
    net = Policy(d=args.d).to(dev)
    opt = torch.optim.Adam(net.parameters(), lr=args.lr, eps=1e-5)
    start_iter, snaps = 0, []
    if args.resume and os.path.exists(os.path.join(CKPT, 'latest.pt')):
        ck = torch.load(os.path.join(CKPT, 'latest.pt'), map_location=dev, weights_only=False)
        net.load_state_dict(ck['net']); opt.load_state_dict(ck['opt']); start_iter = ck['iter']; snaps = ck.get('snaps', [])
    if args.init and start_iter == 0:
        ck = torch.load(args.init, map_location=dev, weights_only=False)
        sd = dict(ck['net'] if 'net' in ck else ck)
        grown = pad_to_shapes(sd, net.state_dict())
        net.load_state_dict(sd)
        log('init from %s%s' % (args.init, '  (输入层补零列：' + ', '.join(grown) + ')' if grown else ''), logf)
    log('mode %s  FM %d SD %d  device %s  params %d  start_iter %d' % (args.mode, wxenv.FM, wxenv.SD, dev, param_count(net), start_iter), logf)

    N, T = args.games, args.steps
    opp_nets = [Policy(d=args.d).to(dev) for _ in range(2)]
    t_begin = time.time()
    total_samples = 0
    ent_coef0 = 0.02
    end_iter = min(args.max_iters, start_iter + args.iters) if args.iters > 0 else args.max_iters
    hours_limit = args.hours if args.hours > 0 else float('inf')
    log('plan: iter %d -> %s, eval every %d iters x %d games/bot/seat, time limit %s' % (
        start_iter, end_iter if end_iter < 10 ** 9 else 'unlimited', args.eval_every, args.eval_games,
        '%.1fh' % hours_limit if hours_limit != float('inf') else 'none'), logf)
    next_it = start_iter
    for it in range(start_iter, end_iter):
        if (time.time() - t_begin) / 3600 > hours_limit:
            break
        # ---- 招式文件池：每隔一段时间换一批新的 ----
        if it == start_iter or it % 40 == 0:
            paths = gen_pool.generate(POOL_DIR, args.pool, 1000 + it)
            pool = Pool(paths)
            pool.load()
            env = wxenv.Env(N, M)
            batch = Batch(env, dev)
            gall = torch.arange(N, device=dev)
        ta, tb, sa, sb = sample_games(pool, N, rng)
        reset_games(env, ta, tb, sa, sb, rng)
        batch.refresh()
        r = rng.random(N)
        # 0自博弈 1快照 2机器人 3套路开局+自博弈(前几回合席位1照脚本出招，之后同0；开局有 护盾金身 和 天动万象+金钟罩) 4随机开局+自博弈(前1~2回合双方都随机出招，之后同0)；
        # 脚本/随机出的招不是网络的决定，不参与学习
        # 套路开局各有自己的占比(bots.OPENINGS的第3项)，其余：自博弈32% 快照20% 机器人12% 随机开局8%
        open_w = np.array([o[2] for o in bots.OPENINGS])
        probs = np.array([0.32, 0.20, 0.12] + list(open_w) + [0.08])
        k_ = np.minimum(np.searchsorted(np.cumsum(probs / probs.sum()), r, side='right'), len(probs) - 1)
        is_open = (k_ >= 3) & (k_ < 3 + len(open_w))
        opp_type = np.where(k_ < 3, k_, np.where(is_open, 3, 4))
        if not snaps:
            opp_type = np.where(opp_type == 1, 0, opp_type)
        slot = rng.integers(0, 2, N)
        for k in range(2):
            if snaps:
                opp_nets[k].load_state_dict(snaps[rng.integers(0, len(snaps))])
        bot_type = rng.choice(4, N, p=[0.2, 0.15, 0.15, 0.5])   # 固定套路占一半：要让网络学会对抗所有已知的人类套路
        bot_fam = bots.pick_family(env, np.arange(N), 1, rng)
        open_id = np.where(is_open, k_ - 3, 0)
        rand_len = rng.integers(1, 3, N)             # 随机开局的回合数
        scripted = np.zeros((T, N, 2), dtype=bool)   # 这一步是脚本/随机出的招(不是网络的决定)
        learner = np.stack([np.ones(N, bool), (opp_type == 0) | (opp_type >= 3)], 1)      # (N,2)
        t0 = time.time()

        # ---- 采样 ----
        obs_state = np.zeros((T, N, 2, wxenv.SD), dtype=np.float32)
        obs_hist = np.zeros((T, N, 2, wxenv.HK, wxenv.HD), dtype=np.float32)
        acts = np.zeros((T, N, 2, 3), dtype=np.int64)
        logps = np.zeros((T, N, 2), dtype=np.float32)
        vals = np.zeros((T, N, 2), dtype=np.float32)
        rews = np.zeros((T, N, 2), dtype=np.float32)
        dones = np.zeros((T, N, 2), dtype=np.float32)
        active = np.zeros((T, N), dtype=bool)
        g2 = torch.cat([gall, gall])
        s2 = torch.cat([torch.zeros_like(gall), torch.ones_like(gall)])
        final_outcome = np.zeros(N, dtype=np.int32)
        game_done = np.zeros(N, dtype=bool)
        steps_used = 0
        net.eval()
        batch.cache(net)
        for t in range(T):
            state, hist, done = env.obs()
            if done.all():
                break
            steps_used = t + 1
            active[t] = done == 0
            obs_state[t], obs_hist[t] = state, hist
            # 只对还没打完的游戏做推理
            sel0 = np.where(done == 0)[0]
            n0 = len(sel0)
            gt = torch.from_numpy(sel0).to(dev)
            gs2 = torch.cat([gt, gt])
            ss2 = torch.cat([torch.zeros_like(gt), torch.ones_like(gt)])
            S = torch.from_numpy(state[sel0]).to(dev).transpose(0, 1).reshape(2 * n0, -1)
            H = torch.from_numpy(hist[sel0]).to(dev).transpose(0, 1).reshape(2 * n0, wxenv.HK, wxenv.HD)
            c, a, b, lp, v = net_actions(net, batch, S, H, gs2, ss2, use_cache=True)
            act = np.zeros((N, 2, 3), dtype=np.int64)
            act[sel0] = torch.stack([c, a, b], 1).cpu().numpy().reshape(2, n0, 3).transpose(1, 0, 2)
            acts[t] = act
            lp_full = np.zeros((N, 2), dtype=np.float32); lp_full[sel0] = lp.cpu().numpy().reshape(2, n0).T
            v_full = np.zeros((N, 2), dtype=np.float32); v_full[sel0] = v.cpu().numpy().reshape(2, n0).T
            logps[t] = lp_full
            vals[t] = v_full
            # 对手席位的实际动作
            for k in range(2):
                sel = np.where((opp_type == 1) & (slot == k) & (done == 0))[0]
                if len(sel):
                    gi = torch.from_numpy(sel).to(dev)
                    one = torch.ones_like(gi)
                    ci, ai, bi, *_ = net_actions(opp_nets[k], batch, torch.from_numpy(state[sel, 1]).to(dev),
                                                 torch.from_numpy(hist[sel, 1]).to(dev), gi, one)
                    act[sel, 1] = torch.stack([ci, ai, bi], 1).cpu().numpy()
            sel = np.where((opp_type == 2) & (done == 0))[0]
            if len(sel):
                act[sel, 1] = bots.actions(env, sel, 1, bot_type[sel], bot_fam[sel], rng)
            sel = np.where((opp_type == 3) & (done == 0))[0]    # 套路开局：脚本没结束的用脚本的招，结束了就是网络自博弈
            if len(sel):
                oa, scr = bots.opening(env, sel, 1, open_id[sel], t + 1, rng)
                act[sel[scr], 1] = oa[scr]
                scripted[t, sel[scr], 1] = True
            sel = np.where((opp_type == 4) & (done == 0) & (t < rand_len))[0]
            if len(sel):
                for k in range(2):
                    act[sel, k] = bots.actions(env, sel, k, np.full(len(sel), bots.OLD), bot_fam[sel], rng)
                scripted[t, sel] = True
            reward, d, out = env.step(act.astype(np.int32))
            rews[t] = reward
            dones[t] = np.repeat(d[:, None], 2, 1)
            newly = (d == 1) & ~game_done
            final_outcome[newly] = out[newly]
            game_done |= d == 1
        # 没打完的游戏用最后观测的价值做自举
        state, hist, done = env.obs()
        S = torch.from_numpy(state).to(dev).transpose(0, 1).reshape(2 * N, -1)
        H = torch.from_numpy(hist).to(dev).transpose(0, 1).reshape(2 * N, wxenv.HK, wxenv.HD)
        with torch.no_grad():
            with torch.autocast('cuda', dtype=torch.bfloat16, enabled=dev.type == 'cuda'):
                es, eo, mask_s, mask_o, form_s = batch.inputs(g2, s2, net, True)
                boot = net(S, H, es, eo, mask_s, mask_o, form_s)[2].float().cpu().numpy().reshape(2, N).T
        boot = boot * (1 - done[:, None])
        Tn = steps_used
        adv = gae(rews[:Tn], vals[:Tn], dones[:Tn], boot, 0.99, 0.95)
        ret = adv + vals[:Tn]
        t_sample = time.time() - t0

        # ---- PPO更新 ----
        use = active[:Tn, :, None] & learner[None]
        use &= ~scripted[:Tn]      # 脚本出的招不是网络的决定，不学
        ts, gs, ss = np.where(use)
        n_samples = len(ts)
        total_samples += n_samples
        net.train()
        t0 = time.time()
        b_state = torch.from_numpy(obs_state[ts, gs, ss]).to(dev)
        b_hist = torch.from_numpy(obs_hist[ts, gs, ss]).to(dev)
        b_act = torch.from_numpy(acts[ts, gs, ss]).to(dev)
        b_lp = torch.from_numpy(logps[ts, gs, ss]).to(dev)
        b_adv = torch.from_numpy(adv[ts, gs, ss]).to(dev)
        b_ret = torch.from_numpy(ret[ts, gs, ss]).to(dev)
        b_g = torch.from_numpy(gs).to(dev); b_s = torch.from_numpy(ss).to(dev)
        b_adv = (b_adv - b_adv.mean()) / (b_adv.std() + 1e-6)
        ent_coef = ent_coef0 * max(0.25, 1.0 - it / 1500.0)
        stats = np.zeros(5)
        nb = 0
        for ep in range(args.epochs):
            perm = torch.randperm(n_samples, device=dev)
            for i in range(0, n_samples, args.mb):
                idx = perm[i:i + args.mb]
                with torch.autocast('cuda', dtype=torch.bfloat16, enabled=dev.type == 'cuda'):
                    es, eo, mask_s, mask_o, form_s = batch.inputs(b_g[idx], b_s[idx], net)
                    h, ctx, v, cl, l1 = net(b_state[idx], b_hist[idx], es, eo, mask_s, mask_o, form_s)
                    a = b_act[idx]
                    lp, ent = net.logprob(cl, l1, a[:, 0], a[:, 1], None, h, ctx, a[:, 2], form_s, mask_s)
                ratio = (lp - b_lp[idx]).exp()
                pg = -torch.min(ratio * b_adv[idx], ratio.clamp(0.8, 1.2) * b_adv[idx]).mean()
                vf = F.smooth_l1_loss(v, b_ret[idx])
                loss = pg + 0.5 * vf - ent_coef * ent.mean()
                opt.zero_grad(set_to_none=True)
                loss.backward()
                torch.nn.utils.clip_grad_norm_(net.parameters(), 0.5)
                opt.step()
                stats += [pg.item(), vf.item(), ent.mean().item(), (ratio - 1).abs().mean().item(), ((ratio - 1).abs() > 0.2).float().mean().item()]
                nb += 1
        stats /= max(nb, 1)
        t_update = time.time() - t0

        if it % 5 == 4:
            snaps.append({k: v.detach().clone() for k, v in net.state_dict().items()})
            snaps = snaps[-30:]
        # ---- 训练指标：按对手类别分的胜率(指数滑动平均，单次迭代每类只有几十盘，噪声大)、动作分布、价值拟合、PPO更新幅度 ----
        groups = [('快照', opp_type == 1)] + [(bots.NAMES[k], (opp_type == 2) & (bot_type == k)) for k in range(3)] + [(r[0], (opp_type == 2) & (bot_type == bots.ROUTINE) & (bot_fam % len(bots.ROUTINES) == k)) for k, r in enumerate(bots.ROUTINES)] +                  [(o[0], (opp_type == 3) & (open_id == k)) for k, o in enumerate(bots.OPENINGS)]
        for name, m in groups:
            if m.any():
                w, l = (final_outcome[m] == 1).mean(), (final_outcome[m] == -1).mean()
                e = EMA.get(name)
                EMA[name] = (w, l) if e is None else (0.9 * e[0] + 0.1 * w, 0.9 * e[1] + 0.1 * l)
        m0 = opp_type == 0
        if m0.any():
            d_ = (final_outcome[m0] == 0).mean()
            EMA['自博弈平局'] = (d_ if '自博弈平局' not in EMA else 0.9 * EMA['自博弈平局'][0] + 0.1 * d_, 0)
        if it % 5 == 0:
            log('it %5d  samples %6d  len %.1f  pg %+.4f vf %.4f ent %.3f dev %.3f clip %.2f | sample %.1fs update %.1fs  total %.2fM  %.2fh' % (
                it, n_samples, steps_used, stats[0], stats[1], stats[2], stats[3], stats[4], t_sample, t_update,
                total_samples / 1e6, (time.time() - t_begin) / 3600), logf)
            ev = 1.0 - np.var(ret[ts, gs, ss] - vals[ts, gs, ss]) / (np.var(ret[ts, gs, ss]) + 1e-8) if n_samples else 0.0
            cls = np.bincount(acts[ts, gs, ss, 0], minlength=5) / max(n_samples, 1)
            log('        价值解释方差 %.2f | 网络动作 跳过%.2f 积攒%.2f 炸弹%.2f 单招%.2f 双招%.2f | 自博弈平局率 %.2f' % (
                ev, cls[0], cls[1], cls[2], cls[3], cls[4:].sum(), EMA.get('自博弈平局', (0, 0))[0]), logf)
            log('        胜/败(滑动平均) ' + '  '.join('%s %.2f/%.2f' % (k, v[0], v[1]) for k, v in EMA.items() if k != '自博弈平局'), logf)
        if it % args.eval_every == args.eval_every - 1:
            net.eval()
            res = evaluate(net, eval_pool, dev, rng, n_games=args.eval_games)
            log('EVAL it %d  ' % it + '  '.join('%s 胜%.2f 败%.2f' % (k, w, l) for k, (w, l) in res.items()), logf)
            # 评测会把环境池换成评测池，重新载入训练池
            pool.load()
            env = wxenv.Env(N, M)
            batch = Batch(env, dev)
            torch.save({'net': net.state_dict(), 'opt': opt.state_dict(), 'iter': it + 1, 'snaps': snaps, 'args': vars(args)},
                       os.path.join(CKPT, 'latest.pt'))
            torch.save(net.state_dict(), os.path.join(CKPT, 'model_%05d.pt' % (it + 1)))
        next_it = it + 1
    torch.save({'net': net.state_dict(), 'opt': opt.state_dict(), 'iter': next_it, 'snaps': snaps, 'args': vars(args)},
               os.path.join(CKPT, 'latest.pt'))
    log('done at iter %d' % next_it, logf)


if __name__ == '__main__':
    main()
