"""PvE非对称训练：两个模型，敌人模型(席位1，游戏里真正用的) 和 玩家模型(席位0，只当陪练和平衡测试)。

PvE是不对称的：玩家招式多、加成多、三维小；敌人三维是玩家的十倍百倍、招式少、效果小、不能积攒炸弹。
一个网络同时扮演两边，两边的决策分布差得太远，所以拆成两个网络：席位固定，0=玩家，1=敌人，各自一个网络、一个优化器，
只用自己席位的样本做PPO。游戏里发布的是敌人模型(导出成 ai_weights_pve)，玩家模型只在训练里当陪练、以及网页里测平衡。

每轮的对局类型(按席位固定，所以敌人遇到的对手是"各种各样的玩家")：
  0 玩家当前版 vs 敌人当前版             两个模型都学         35%
  1 玩家历史快照 vs 敌人当前版           只有敌人学           20%
  2 玩家当前版 vs 敌人历史快照           只有玩家学           15%
  3 脚本玩家 vs 敌人当前版               只有敌人学           30%   (脚本：随机AI/最高威力/连招爬升/护盾龟/禁锢炸弹，见bots.PVE_PLAYER_NAMES)

每盘的配置(pve_gen.sample_asym)：玩家用训练池里的招式文件(招式多)，特性重配置并加增伤类特性，让它"堆满加成后"打穿敌人需要4~25回合；
敌人用 gen_pool.generate_enemy 的招式文件(2~9个招式，效果0.3~0.7倍)，三维乘规模倍数 k=10^U(kmin,kmax)，轻配置，不能积攒炸弹。

用法：
  python train_pve2.py --iters 300                      从 checkpoints_pve\\latest.pt(没有就用 checkpoints_pvp\\latest.pt) 补零列热启动，训300轮
  python train_pve2.py --iters 300 --resume             接着 checkpoints_pve_enemy / checkpoints_pve_player 再训
  python train_pve2.py --iters 2 --games 64 --eval_every 2 --eval_games 32 --ckpt_suffix _smoke    冒烟
"""
import argparse
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
import train as T

HERE = T.HERE
M = T.M
log = T.log
POOL_DIR = os.path.join(HERE, 'pool_pve')
EVAL_P = os.path.join(HERE, 'pool_eval', 'pve_player')
EVAL_E = os.path.join(HERE, 'pool_eval', 'pve_enemy')
MULTS = [0.5, 1.0, 1.2, 1.5, 1.9, 2.4, 3.0, 3.7, 4.5, 5.6, 7.0, 8.5, 10.0, 12.0]


def sample_asym_games(pool, nP, n, rng, klog_lo, klog_hi, jitter=0.4):
    """pool里前nP个是玩家的招式文件，后面是敌人的。返回 ta, tb, sa, sb, cfgs, ks。属性 = hp, atk, mp, agi, rec, 跳过损失敏捷, 每回合恢复敏捷, 炸弹积攒倍率。"""
    nE = len(pool.paths) - nP
    ta = rng.integers(0, nP, n)
    tb = nP + rng.integers(0, nE, n)
    ks = 10.0 ** rng.uniform(klog_lo, klog_hi, n)
    sa, sb, cfgs = [], [], []
    j = lambda: float(np.exp(rng.normal(0, jitter)))
    for g in range(n):
        a, b = pool.stats[ta[g]], pool.stats[tb[g]]
        wide = rng.random() < 0.4
        agi_a = float(np.exp(rng.uniform(np.log(3), np.log(30)))) if wide else gen_pool.std_agi(a[4])
        agi_b = float(np.clip(rng.normal(gen_pool.std_agi(b[4]), 2.0), 2, 25))
        loss = lambda: 1.0 if rng.random() < 0.5 else float(rng.choice([0.5, 1.0, 1.5, 2.0]))
        regen = lambda: 0.2 if rng.random() < 0.5 else float(rng.choice([0.1, 0.2, 0.4, 0.6]))
        mult = lambda s: s[4] if rng.random() < 0.8 else float(rng.choice(MULTS))
        pa = [a[0] * j(), a[1] * j(), a[2] * j(), agi_a, a[3], loss(), regen(), mult(a)]
        k = float(ks[g])
        pb = [b[0] * j() * k, b[1] * j() * k, b[2] * j() * k, agi_b, b[3] * k, loss(), regen(), mult(b)]
        sa.append(pa); sb.append(pb)
        cfgs.append(pve_gen.sample_asym(rng, pool.paths[ta[g]], pa[1], pb[1], pb[0] + pb[1], k))
    return ta, tb, np.array(sa, dtype=np.float32), np.array(sb, dtype=np.float32), cfgs, ks


def reset_asym(env, ta, tb, sa, sb, cfgs):
    for g in range(env.n):
        env.reset(g, int(ta[g]), int(tb[g]), sa[g], sb[g], pve=cfgs[g])


def load_pools(player_paths, enemy_paths):
    pool = T.Pool(list(player_paths) + list(enemy_paths))
    pool.load()
    return pool, len(player_paths)


def make_net(args, dev):
    return Policy(d=args.d).to(dev)


def init_net(net, path, tag, logf):
    ck = torch.load(path, map_location=net.latents.device, weights_only=False)
    sd = dict(ck['net'] if 'net' in ck else ck)
    grown = pad_to_shapes(sd, net.state_dict())
    net.load_state_dict(sd)
    log('init %s from %s%s' % (tag, path, '  (输入层补零列：' + ', '.join(grown) + ')' if grown else ''), logf)


def ppo_update(net, opt, batch, seat, obs_state, obs_hist, acts, logps, adv, ret, use, it, args, dev):
    """用席位seat上use(T,N)为真的样本更新net。返回(样本数, pg, vf, ent, dev)。"""
    ts, gs = np.where(use)
    n = len(ts)
    if n == 0:
        return 0, 0.0, 0.0, 0.0, 0.0
    net.train()
    b_state = torch.from_numpy(obs_state[ts, gs, seat]).to(dev)
    b_hist = torch.from_numpy(obs_hist[ts, gs, seat]).to(dev)
    b_act = torch.from_numpy(acts[ts, gs, seat]).to(dev)
    b_lp = torch.from_numpy(logps[ts, gs, seat]).to(dev)
    b_adv = torch.from_numpy(adv[ts, gs, seat]).to(dev)
    b_ret = torch.from_numpy(ret[ts, gs, seat]).to(dev)
    b_g = torch.from_numpy(gs).to(dev)
    b_s = torch.full_like(b_g, seat)
    b_adv = (b_adv - b_adv.mean()) / (b_adv.std() + 1e-6)
    ent_coef = 0.02 * max(0.25, 1.0 - it / 1500.0)
    stats = np.zeros(4)
    nb = 0
    for ep in range(args.epochs):
        perm = torch.randperm(n, device=dev)
        for i in range(0, n, args.mb):
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
            stats += [pg.item(), vf.item(), ent.mean().item(), (ratio - 1).abs().mean().item()]
            nb += 1
    stats /= max(nb, 1)
    return (n, *stats)


@torch.no_grad()
def play_asym(env, bP, bE, netP, netE, kind, rng, dev, steps=80):
    """评测：席位1是敌人模型；席位0是玩家模型(kind=None)或脚本玩家(kind=bots.PVE_PLAYER_NAMES的下标)。返回outcome(+1玩家胜 -1敌人胜 0没分出)。"""
    N = env.n
    gall = torch.arange(N, device=dev)
    outcome = np.zeros(N, dtype=np.int32)
    done_all = np.zeros(N, dtype=bool)
    for t in range(steps):
        state, hist, done = env.obs()
        if done.all():
            break
        S, H = torch.from_numpy(state).to(dev), torch.from_numpy(hist).to(dev)
        act = np.zeros((N, 2, 3), dtype=np.int32)
        if kind is None:
            c, a, b, *_ = T.net_actions(netP, bP, S[:, 0], H[:, 0], gall, torch.zeros_like(gall))
            act[:, 0] = torch.stack([c, a, b], 1).cpu().numpy()
        else:
            act[:, 0] = bots.pve_player_actions(env, np.arange(N), 0, np.full(N, kind), rng)
        c, a, b, *_ = T.net_actions(netE, bE, S[:, 1], H[:, 1], gall, torch.ones_like(gall))
        act[:, 1] = torch.stack([c, a, b], 1).cpu().numpy()
        _, d, out = env.step(act)
        newly = (d == 1) & ~done_all
        outcome[newly] = out[newly]
        done_all |= d == 1
    return outcome


def evaluate(netP, netE, dev, rng, eval_pool, nP, n_games):
    """敌人模型对每种玩家(脚本玩家 + 玩家模型)、每个规模档(k=10/30/100)的胜率。返回 {(k, 名字): (敌人胜率, 玩家胜率)}。"""
    eval_pool.load()
    env = wxenv.Env(n_games, M)
    bE, bP = T.Batch(env, dev), T.Batch(env, dev)
    res = {}
    for k in (10, 30, 100):
        for kind in [None] + list(range(len(bots.PVE_PLAYER_NAMES))):
            lg = np.log10(k)
            ta, tb, sa, sb, cfgs, _ = sample_asym_games(eval_pool, nP, n_games, rng, lg, lg)
            reset_asym(env, ta, tb, sa, sb, cfgs)
            bE.refresh(); bP.refresh()
            out = play_asym(env, bP, bE, netP, netE, kind, rng, dev)
            res[(k, '玩家模型' if kind is None else bots.PVE_PLAYER_NAMES[kind])] = ((out == -1).mean(), (out == 1).mean())
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--games', type=int, default=1024)
    ap.add_argument('--steps', type=int, default=80)
    ap.add_argument('--pool', type=int, default=1500, help='玩家招式文件数；敌人招式文件数是它的一半')
    ap.add_argument('--lr', type=float, default=1.5e-4)
    ap.add_argument('--epochs', type=int, default=3)
    ap.add_argument('--mb', type=int, default=2048)
    ap.add_argument('--hours', type=float, default=0.0)
    ap.add_argument('--iters', type=int, default=0)
    ap.add_argument('--eval_every', type=int, default=50)
    ap.add_argument('--eval_games', type=int, default=128, help='评测：每个(规模档,对手)格子打多少盘')
    ap.add_argument('--kmin', type=float, default=0.5, help='敌人规模倍数的下限(log10)，训练初期从这里起步')
    ap.add_argument('--kmax', type=float, default=2.0, help='敌人规模倍数的上限(log10)：2=100倍')
    ap.add_argument('--k_warm', type=int, default=200, help='前多少轮把规模上限从1.0(10倍)慢慢升到kmax')
    ap.add_argument('--resume', action='store_true')
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--d', type=int, default=128)
    ap.add_argument('--init', default=None, help='两个模型共同的热启动检查点，默认 checkpoints_pve\\latest.pt，没有就用 checkpoints_pvp\\latest.pt')
    ap.add_argument('--ckpt_suffix', default='', help='检查点目录后缀(冒烟测试用 _smoke)')
    args = ap.parse_args()

    wxenv.set_mode(True)
    dev = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    CK = {s: os.path.join(HERE, 'checkpoints_pve_%s%s' % (s, args.ckpt_suffix)) for s in ('enemy', 'player')}
    for d in CK.values():
        os.makedirs(d, exist_ok=True)
    logf = open(os.path.join(CK['enemy'], 'train.log'), 'a', encoding='utf-8')
    rng = np.random.default_rng(args.seed)
    torch.manual_seed(args.seed)

    ep = gen_pool.generate(EVAL_P, 100, 99991) + gen_pool.write_real(os.path.join(EVAL_P, 'real'), [[1], [6], [12], [20], [18, 19, 20]])
    ee = gen_pool.generate_enemy(EVAL_E, 60, 99992)
    eval_pool, eval_nP = load_pools(ep, ee)

    nets = {'player': make_net(args, dev), 'enemy': make_net(args, dev)}
    opts = {k: torch.optim.Adam(n.parameters(), lr=args.lr, eps=1e-5) for k, n in nets.items()}
    snaps = {'player': [], 'enemy': []}
    start_iter = 0
    for k in nets:
        path = os.path.join(CK[k], 'latest.pt')
        if args.resume and os.path.exists(path):
            ck = torch.load(path, map_location=dev, weights_only=False)
            nets[k].load_state_dict(ck['net']); opts[k].load_state_dict(ck['opt'])
            start_iter = max(start_iter, ck['iter']); snaps[k] = ck.get('snaps', [])
    if start_iter == 0:
        init = args.init
        if init is None:
            for cand in ('checkpoints_pve', 'checkpoints_pvp'):
                p = os.path.join(HERE, cand, 'latest.pt')
                if os.path.exists(p):
                    init = p
                    break
        if init:
            for k in nets:
                init_net(nets[k], init, k, logf)
    log('mode pve2  FM %d SD %d  device %s  params %d x2  start_iter %d' % (wxenv.FM, wxenv.SD, dev, param_count(nets['enemy']), start_iter), logf)

    N, Tn_max = args.games, args.steps
    opp = {k: [make_net(args, dev) for _ in range(2)] for k in nets}      # 历史快照的对手网络(每个模型2个槽位)
    t_begin = time.time()
    end_iter = start_iter + args.iters if args.iters > 0 else 10 ** 9
    hours_limit = args.hours if args.hours > 0 else float('inf')
    log('plan: iter %d -> %s, eval every %d iters x %d games/cell' % (start_iter, end_iter if end_iter < 10 ** 9 else 'unlimited', args.eval_every, args.eval_games), logf)
    next_it = start_iter
    for it in range(start_iter, end_iter):
        if (time.time() - t_begin) / 3600 > hours_limit:
            break
        if it == start_iter or it % 40 == 0:
            pp = gen_pool.generate(os.path.join(POOL_DIR + args.ckpt_suffix, 'player'), args.pool, 5000 + it)
            pe = gen_pool.generate_enemy(os.path.join(POOL_DIR + args.ckpt_suffix, 'enemy'), max(8, args.pool // 2), 7000 + it)
            pool, nP = load_pools(pp, pe)
            env = wxenv.Env(N, M)
            bP, bE = T.Batch(env, dev), T.Batch(env, dev)
            gall = torch.arange(N, device=dev)
        kmax = args.kmax if it >= args.k_warm else 1.0 + (args.kmax - 1.0) * it / max(1, args.k_warm)
        ta, tb, sa, sb, cfgs, ks = sample_asym_games(pool, nP, N, rng, min(args.kmin, kmax), kmax)
        reset_asym(env, ta, tb, sa, sb, cfgs)
        bP.refresh(); bE.refresh()
        r = rng.random(N)
        typ = np.where(r < 0.35, 0, np.where(r < 0.55, 1, np.where(r < 0.70, 2, 3)))
        if not snaps['player']:
            typ = np.where(typ == 1, 0, typ)
        if not snaps['enemy']:
            typ = np.where(typ == 2, 0, typ)
        slot = rng.integers(0, 2, N)
        for k in nets:
            for s in range(2):
                if snaps[k]:
                    opp[k][s].load_state_dict(snaps[k][rng.integers(0, len(snaps[k]))])
        bot_kind = rng.integers(0, len(bots.PVE_PLAYER_NAMES), N)
        learn = np.stack([(typ == 0) | (typ == 2), (typ == 0) | (typ == 1) | (typ == 3)], 1)     # (N,2) 每个席位是不是在学
        t0 = time.time()

        obs_state = np.zeros((Tn_max, N, 2, wxenv.SD), dtype=np.float32)
        obs_hist = np.zeros((Tn_max, N, 2, wxenv.HK, wxenv.HD), dtype=np.float32)
        acts = np.zeros((Tn_max, N, 2, 3), dtype=np.int64)
        logps = np.zeros((Tn_max, N, 2), dtype=np.float32)
        vals = np.zeros((Tn_max, N, 2), dtype=np.float32)
        rews = np.zeros((Tn_max, N, 2), dtype=np.float32)
        dones = np.zeros((Tn_max, N, 2), dtype=np.float32)
        active = np.zeros((Tn_max, N), dtype=bool)
        final_outcome = np.zeros(N, dtype=np.int32)
        game_done = np.zeros(N, dtype=bool)
        steps_used = 0
        for n_ in nets.values():
            n_.eval()
        bP.cache(nets['player']); bE.cache(nets['enemy'])
        for t in range(Tn_max):
            state, hist, done = env.obs()
            if done.all():
                break
            steps_used = t + 1
            active[t] = done == 0
            obs_state[t], obs_hist[t] = state, hist
            sel0 = np.where(done == 0)[0]
            gt = torch.from_numpy(sel0).to(dev)
            act = np.zeros((N, 2, 3), dtype=np.int64)
            for seat, key, batch in ((0, 'player', bP), (1, 'enemy', bE)):
                S = torch.from_numpy(state[sel0, seat]).to(dev)
                H = torch.from_numpy(hist[sel0, seat]).to(dev)
                sv = torch.full_like(gt, seat)
                c, a, b, lp, v = T.net_actions(nets[key], batch, S, H, gt, sv, use_cache=True)
                act[sel0, seat] = torch.stack([c, a, b], 1).cpu().numpy()
                logps[t, sel0, seat] = lp.cpu().numpy()
                vals[t, sel0, seat] = v.cpu().numpy()
            acts[t] = act
            # 不是当前版本网络出招的席位：换成快照/脚本的动作(这些席位不学习)
            for s in range(2):
                sel = np.where((typ == 1) & (slot == s) & (done == 0))[0]      # 席位0用玩家快照
                if len(sel):
                    gi = torch.from_numpy(sel).to(dev)
                    ci, ai, bi, *_ = T.net_actions(opp['player'][s], bP, torch.from_numpy(state[sel, 0]).to(dev),
                                                   torch.from_numpy(hist[sel, 0]).to(dev), gi, torch.zeros_like(gi))
                    act[sel, 0] = torch.stack([ci, ai, bi], 1).cpu().numpy()
                sel = np.where((typ == 2) & (slot == s) & (done == 0))[0]      # 席位1用敌人快照
                if len(sel):
                    gi = torch.from_numpy(sel).to(dev)
                    ci, ai, bi, *_ = T.net_actions(opp['enemy'][s], bE, torch.from_numpy(state[sel, 1]).to(dev),
                                                   torch.from_numpy(hist[sel, 1]).to(dev), gi, torch.ones_like(gi))
                    act[sel, 1] = torch.stack([ci, ai, bi], 1).cpu().numpy()
            sel = np.where((typ == 3) & (done == 0))[0]                         # 席位0用脚本玩家
            if len(sel):
                act[sel, 0] = bots.pve_player_actions(env, sel, 0, bot_kind[sel], rng)
            reward, d, out = env.step(act.astype(np.int32))
            rews[t] = reward
            dones[t] = np.repeat(d[:, None], 2, 1)
            newly = (d == 1) & ~game_done
            final_outcome[newly] = out[newly]
            game_done |= d == 1
        # 没打完的游戏用最后观测的价值做自举
        state, hist, done = env.obs()
        boot = np.zeros((N, 2), dtype=np.float32)
        with torch.no_grad(), torch.autocast('cuda', dtype=torch.bfloat16, enabled=dev.type == 'cuda'):
            for seat, key, batch in ((0, 'player', bP), (1, 'enemy', bE)):
                sv = torch.full_like(gall, seat)
                es, eo, mask_s, mask_o, form_s = batch.inputs(gall, sv, nets[key], True)
                boot[:, seat] = nets[key](torch.from_numpy(state[:, seat]).to(dev), torch.from_numpy(hist[:, seat]).to(dev),
                                          es, eo, mask_s, mask_o, form_s)[2].float().cpu().numpy()
        boot = boot * (1 - done[:, None])
        Tn = steps_used
        adv = T.gae(rews[:Tn], vals[:Tn], dones[:Tn], boot, 0.99, 0.95)
        ret = adv + vals[:Tn]
        t_sample = time.time() - t0

        t0 = time.time()
        res = {}
        for seat, key, batch in ((0, 'player', bP), (1, 'enemy', bE)):
            use = active[:Tn] & learn[None, :, seat]
            res[key] = ppo_update(nets[key], opts[key], batch, seat, obs_state[:Tn], obs_hist[:Tn], acts[:Tn], logps[:Tn], adv, ret, use, it, args, dev)
        t_update = time.time() - t0

        if it % 5 == 4:
            for k in nets:
                snaps[k].append({n: v.detach().clone() for n, v in nets[k].state_dict().items()})
                snaps[k] = snaps[k][-30:]
        if it % 5 == 0:
            e_win = lambda m: (final_outcome[m] == -1).mean() if m.any() else 0.0
            lo, mid, hi = ks < 10, (ks >= 10) & (ks < 30), ks >= 30
            log('it %5d  E: n %6d pg %+.4f vf %.4f ent %.3f | P: n %6d pg %+.4f vf %.4f ent %.3f | 敌胜率 自对弈 %.2f(k<10 %.2f, 10-30 %.2f, >30 %.2f) 对脚本 %.2f 对玩家快照 %.2f | '
                '长度 %.1f kmax %.2f | sample %.1fs update %.1fs %.2fh' % (
                    it, res['enemy'][0], res['enemy'][1], res['enemy'][2], res['enemy'][3], res['player'][0], res['player'][1], res['player'][2], res['player'][3],
                    e_win(typ == 0), e_win((typ == 0) & lo), e_win((typ == 0) & mid), e_win((typ == 0) & hi), e_win(typ == 3), e_win(typ == 1),
                    steps_used, kmax, t_sample, t_update, (time.time() - t_begin) / 3600), logf)
        if it % args.eval_every == args.eval_every - 1:
            for n_ in nets.values():
                n_.eval()
            res_e = evaluate(nets['player'], nets['enemy'], dev, rng, eval_pool, eval_nP, args.eval_games)
            for k in (10, 30, 100):
                log('EVAL it %d  k=%3d  ' % (it, k) + '  '.join('%s 敌胜%.2f/玩家胜%.2f' % (nm, w, l) for (kk, nm), (w, l) in res_e.items() if kk == k), logf)
            pool.load()
            env = wxenv.Env(N, M)
            bP, bE = T.Batch(env, dev), T.Batch(env, dev)
            for k in nets:
                torch.save({'net': nets[k].state_dict(), 'opt': opts[k].state_dict(), 'iter': it + 1, 'snaps': snaps[k], 'args': vars(args)},
                           os.path.join(CK[k], 'latest.pt'))
        next_it = it + 1
    for k in nets:
        torch.save({'net': nets[k].state_dict(), 'opt': opts[k].state_dict(), 'iter': next_it, 'snaps': snaps[k], 'args': vars(args)},
                   os.path.join(CK[k], 'latest.pt'))
    log('done at iter %d' % next_it, logf)


if __name__ == '__main__':
    main()
