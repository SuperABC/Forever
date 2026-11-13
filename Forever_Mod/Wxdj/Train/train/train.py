"""PPO自博弈训练通用对战模型。

每轮：随机抽N盘对局(双方的招式文件和属性各自随机)，对手是 自己(两个席位都学习) / 历史快照(没有任何脚本对手；陪练的前两回合可能随机出招，见 bots.py)，
并行打完 -> 算优势 -> PPO更新。定期评测并存检查点。招式表由 gen_pool.py 按 WXDJ_ACT 指定的招式表(默认 Cpp/act/complete.txt)生成。
用法(一般用 scripts/ 里的脚本；直接用也行)：
  python train.py --ckpt_dir ../runs/x                         从头训练(默认200轮，--iters 改)
  python train.py --ckpt_dir ../runs/x --init ../models/base/base_avg.pt --lr 1e-4    从已有模型续训/微调
  python train.py --ckpt_dir ../runs/x --resume                接着这个目录的 latest.pt 训练
  python train.py --hours 6                                     最多训练6小时(和--iters谁先到谁停)
"""
import argparse
import os
import re
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                         # Train
sys.path.insert(0, os.path.join(ROOT, 'env'))        # wxenv.py 和 wxenv.dll 在 env/ 里

import numpy as np
import torch
import torch.nn.functional as F

import bots
import gen_pool
import wxenv
from model import Policy, param_count
from pad_checkpoint import pad_to_shapes

M = 128      # 环境里每方最多容纳的招式数(每方最多 gen_pool.MAX_MOVES=125 个：5式窗口 x 最多25个家族)


def log(msg, f=None):
    print(msg, flush=True)
    if f:
        f.write(msg + '\n'); f.flush()


class Pool:
    """一批招式文件，加载进环境。"""

    def __init__(self, paths):
        self.paths = paths
        self.stats = np.array([gen_pool.read_stats(p) for p in paths], dtype=np.float64)   # hp atk mp rec
        self.top = np.array([gen_pool.read_top(p) for p in paths])                           # 每个文件的最高式
        self.by_top = {int(t): np.where(self.top == t)[0] for t in np.unique(self.top)}      # 最高式 -> 文件下标
        self._effect_mat = None

    def effect_matrix(self):
        """(文件数, 128, 1+效果类型数+7)的0/1数组：文件里第j个招式(文件顺序=环境里的招式顺序)是否带某个效果；第0列是"无效果"，
        接着按 gen_pool.EFFECT_NAMES 的顺序，最后7列是招式的属性(按 gen_pool.ATTRS 的顺序：无/金/木/水/火/土/五行)。
        只给训练日志统计单招用，第一次用时才解析文件。"""
        if self._effect_mat is None:
            col = {k: i + 1 for i, k in enumerate(gen_pool.EFFECT_NAMES)}
            mat = np.zeros((len(self.paths), 128, 1 + len(col) + len(gen_pool.ATTRS)), dtype=np.uint8)
            for p, path in enumerate(self.paths):
                j = -1
                for line in open(path, encoding='utf-8').read().split('\n'):
                    line = line.rstrip('\r')
                    if not line.strip() or line.startswith('#'):
                        continue
                    t = line.split(' ')
                    if len(t) == 6 and re.match(r'^\d+-\d+$', t[4]):
                        j += 1
                        if j < 128:
                            if int(t[5]) == 0:
                                mat[p, j, 0] = 1
                            if t[1] in gen_pool.ATTRS:
                                mat[p, j, 1 + len(col) + gen_pool.ATTRS.index(t[1])] = 1
                    elif 0 <= j < 128 and t[0] in col:
                        mat[p, j, col[t[0]]] = 1
            self._effect_mat = mat
        return self._effect_mat

    def load(self):
        """环境里的招式文件池是全局的，换用哪个池就要重新载入。"""
        wxenv.pool_clear()
        for p in self.paths:
            assert wxenv.pool_add(p) >= 0, p


def _side(stats, f):
    """一方的属性 = hp, atk, mp, agi, rec, 跳过损失敏捷, 每回合恢复敏捷, 炸弹积攒倍率。
    三维(hp, atk, mp) = 所属阶位的标准值 x f(三个扰动系数，f=(1,1,1)就是标准值)；
    其余四个(敏捷、跳过损失敏捷、每回合恢复敏捷、炸弹积攒倍率)是PvP，全部取最高式对应的标准值，不随机。"""
    return [stats[0] * f[0], stats[1] * f[1], stats[2] * f[2], gen_pool.std_agi(stats[4]), stats[3], 1.0, 0.2, stats[4]]


def sample_games(pool, n, rng, pool_probs=(0.5, 0.4, 0.1), stat_probs=(0.4, 0.4, 0.2), jitter=0.4):
    """训练用：随机抽n盘对局的(甲文件, 乙文件, 甲属性, 乙属性)。招式库的抽法和三维的抽法是**互相独立**的两个随机：
    招式库(pool_probs 依次是三种的比例)：
      0.5  随机抽一个文件，双方都用它；
      0.4  双方各自随机抽一个文件，但两个文件的最高式相同；
      0.1  双方各自纯随机抽文件，不限制最高式。
    三维 hp/atk/mp (stat_probs 依次是三种的比例)：
      0.4  双方都用标准值；
      0.4  三维都乘上扰动系数(对数正态，sigma=jitter)，双方用同一组系数(同阶位时数值就完全一样)；
      0.2  双方各自独立扰动。
    敏捷、跳过损失敏捷、每回合恢复敏捷、炸弹积攒倍率是PvP的标准值(取最高式对应的值)，不随机。"""
    P = len(pool.paths)
    u = rng.random(n)
    ta = rng.integers(0, P, n)
    tb = rng.integers(0, P, n)                                     # 纯随机
    tb = np.where(u < pool_probs[0], ta, tb)                       # 同一个文件
    for g in np.where((u >= pool_probs[0]) & (u < pool_probs[0] + pool_probs[1]))[0]:
        tb[g] = rng.choice(pool.by_top[int(pool.top[ta[g]])])      # 最高式相同的另一个(也可能抽到同一个)文件
    j3 = lambda: tuple(float(np.exp(rng.normal(0, jitter))) for _ in range(3))
    sa, sb = [], []
    for g in range(n):
        a, b = pool.stats[ta[g]], pool.stats[tb[g]]
        r = rng.random()
        if r < stat_probs[0]:
            fa = fb = (1.0, 1.0, 1.0)
        elif r < stat_probs[0] + stat_probs[1]:
            fa = fb = j3()
        else:
            fa, fb = j3(), j3()
        sa.append(_side(a, fa))
        sb.append(_side(b, fb))
    return ta, tb, np.array(sa, dtype=np.float32), np.array(sb, dtype=np.float32)


def sample_mirror_games(pool, n, rng):
    """评测用：镜像局(双方同一个文件、同样的标准属性)，胜率才反映模型本身的强弱。训练不用它。"""
    ta = rng.integers(0, len(pool.paths), n)
    sa, sb = [], []
    for g in range(n):
        a = pool.stats[ta[g]]
        x = _side(a, (1.0, 1.0, 1.0))                                      # 标准敏捷：第1式5.25，每式+0.25，每阶对齐最大式 -> 5+最大式/4
        sa.append(x); sb.append(list(x))
    return ta, ta.copy(), np.array(sa, dtype=np.float32), np.array(sb, dtype=np.float32)


def reset_games(env, ta, tb, sa, sb, rng, mirror=False):
    """重置所有对局。"""
    for g in range(env.n):
        env.reset(g, ta[g], tb[g], sa[g], sb[g])


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


def play(env, batch, net0, net1, n_steps, dev, greedy=False):
    """评测用：席位0是net0，席位1是net1。返回(outcome数组(+1席位0胜), 回合数, 是否打完)。"""
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
        for seat, net in ((0, net0), (1, net1)):
            c, a, b, *_ = net_actions(net, batch, S[:, seat], H[:, seat], gall, torch.full_like(gall, seat), greedy)
            act[:, seat] = torch.stack([c, a, b], 1).cpu().numpy()
        reward, d, out = env.step(act)
        newly = (d == 1) & ~done_all
        lengths[newly] = t + 1
        outcome[newly] = out[newly]
        done_all |= d == 1
    return outcome, lengths, done_all


EMA = {}


def evaluate(net, refs, eval_pool, dev, rng, n_games=256):
    """没有任何脚本对手：只和"自己的历史版本"(refs: 名字 -> 网络)打，看有没有进步。每个参照打n_games盘x2个席位(镜像局，公平)，
    返回 {名字: (胜率, 败率)}。注意自博弈会绕圈，胜率不是单调上升的，只作进度参考；真正看收敛要用 diag.py。"""
    eval_pool.load()
    env = wxenv.Env(n_games, M)
    res = {}
    for name, ref in refs.items():
        wins = losses = 0
        for swap in (0, 1):
            ta, tb, sa, sb = sample_mirror_games(eval_pool, n_games, rng)   # 评测用镜像局=公平，胜率才反映模型本身的强弱
            reset_games(env, ta, tb, sa, sb, rng, mirror=True)
            batch = Batch(env, dev)
            outcome, _, _ = play(env, batch, *((net, ref) if swap == 0 else (ref, net)), 80, dev)
            if swap == 1:
                outcome = -outcome
            wins += int((outcome == 1).sum()); losses += int((outcome == -1).sum())
        res[name] = (wins / (2 * n_games), losses / (2 * n_games))
    return res


def eval_refs(ckpt_dir, dev, d):
    """参照网络：这次训练第一次评测存下的模型、上一次评测存下的模型(从检查点目录的 model_*.pt 读，续训也有效)。"""
    fs = sorted(f for f in os.listdir(ckpt_dir) if f.startswith('model_') and f.endswith('.pt'))
    pick = {}
    if fs:
        pick['第一次评测'] = fs[0]
        if len(fs) > 1:
            pick['上一次评测'] = fs[-1]
    refs = {}
    for name, f in pick.items():
        n = Policy(d=d).to(dev).eval()
        n.load_state_dict(torch.load(os.path.join(ckpt_dir, f), map_location=dev, weights_only=False))
        refs[name] = n
    return refs


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
    ap.add_argument('--ckpt_dir', default=os.path.join(ROOT, 'runs', 'train'), help='输出目录：latest.pt、model_XXXXX.pt、snaps/(历史快照)、train.log、work/(招式池临时文件)')
    ap.add_argument('--iters', type=int, default=200, help='这次训练多少轮(续训时是"再训多少轮")；0=不限，和--hours/--max_iters谁先到谁停')
    ap.add_argument('--hours', type=float, default=0.0, help='最多训练多少小时，0=不限时间')
    ap.add_argument('--max_iters', type=int, default=10 ** 9, help='训练到第几轮为止(轮数从0起算，续训时包含之前已经训练的轮数)')
    ap.add_argument('--init', default=None, help='从这个检查点/权重文件读权重开始训练(输入层缺的列补零)，用于改了招式表或特征之后的续训/微调')
    ap.add_argument('--resume', action='store_true', help='接着 ckpt_dir 里的 latest.pt 训练(含优化器和历史快照)')
    ap.add_argument('--games', type=int, default=1024, help='每轮采样多少盘')
    ap.add_argument('--steps', type=int, default=80, help='每盘最多多少回合')
    ap.add_argument('--pool', type=int, default=3000, help='招式文件池大小(每40轮换一批)')
    ap.add_argument('--lr', type=float, default=3e-4)
    ap.add_argument('--epochs', type=int, default=3)
    ap.add_argument('--mb', type=int, default=2048)
    ap.add_argument('--eval_every', type=int, default=50, help='每多少轮评测一次并存检查点')
    ap.add_argument('--eval_games', type=int, default=128, help='每次评测对每个参照模型(第一次评测/上一次评测存下的自己)、每个席位打多少盘')
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--d', type=int, default=128)
    # ---- 防止自博弈绕圈的配方(上一轮实验的结论)：保留全部历史快照并从中均匀抽对手、快照对手占比高、熵系数不降太低 ----
    ap.add_argument('--snap_every', type=int, default=5, help='每多少轮存一个历史快照(fp16，存到 ckpt_dir/snaps/)')
    ap.add_argument('--snap_prob', type=float, default=0.45, help='对手是历史快照的对局占比权重(自博弈0.32，归一化)')
    ap.add_argument('--open_single', type=float, default=0.4, help='陪练用单招类开局的概率(这些局里三种各1/3：积攒+炸弹(前两回合)、静止增伤(第1回合)、每个单招家族平均(第1回合)，之后自博弈/快照接管，见 bots.py)')
    ap.add_argument('--open_double', type=float, default=0.1, help='陪练第1回合随机出双招(同式)的概率；其余照常由网络/快照出招')
    ap.add_argument('--opp_slots', type=int, default=4, help='每轮同时抽几个不同的快照当对手')
    ap.add_argument('--ent_coef', type=float, default=0.02, help='熵系数初值')
    ap.add_argument('--ent_floor', type=float, default=0.5, help='熵系数衰减到初值的多少倍后不再降(1.0=不衰减)')
    ap.add_argument('--ent_decay_iters', type=float, default=0, help='熵系数在多少轮里线性衰减到下限；0=自动(这次计划轮数的3/4)')
    args = ap.parse_args()

    CKPT = os.path.abspath(args.ckpt_dir)
    dev = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    os.makedirs(CKPT, exist_ok=True)
    pool_dir = os.path.join(CKPT, 'work', 'pool')
    eval_dir = os.path.join(CKPT, 'work', 'pool_eval')
    snap_dir = os.path.join(CKPT, 'snaps')
    seed_off = 0 if args.seed == 1 else args.seed * 100003     # 不同种子用不同的招式池序列
    logf = open(os.path.join(CKPT, 'train.log'), 'a', encoding='utf-8')
    rng = np.random.default_rng(args.seed)
    torch.manual_seed(args.seed)

    eval_paths = gen_pool.generate(eval_dir, 150, 99991) + gen_pool.write_real_windows(
        os.path.join(eval_dir, 'real'), [20, 15, 10, 5, 3], gen_pool.eval_families())      # 留出的合成窗口 + 招式表里真实家族的窗口(最高式20/15/10/5/3)
    eval_pool = Pool(eval_paths)
    net = Policy(d=args.d).to(dev)
    opt = torch.optim.Adam(net.parameters(), lr=args.lr, eps=1e-5)
    start_iter, snaps = 0, []
    if args.resume and os.path.exists(os.path.join(CKPT, 'latest.pt')):
        ck = torch.load(os.path.join(CKPT, 'latest.pt'), map_location=dev, weights_only=False)
        net.load_state_dict(ck['net']); opt.load_state_dict(ck['opt']); start_iter = ck['iter']
        if os.path.isdir(snap_dir):
            snaps = [torch.load(os.path.join(snap_dir, f), map_location='cpu') for f in sorted(os.listdir(snap_dir)) if f.endswith('.pt')]
    if args.init and start_iter == 0:
        ck = torch.load(args.init, map_location=dev, weights_only=False)
        sd = dict(ck['net'] if 'net' in ck else ck)
        grown = pad_to_shapes(sd, net.state_dict())
        net.load_state_dict(sd)
        log('init from %s%s' % (args.init, '  (输入层补零列：' + ', '.join(grown) + ')' if grown else ''), logf)
    log('FM %d SD %d  device %s  params %d  start_iter %d' % (wxenv.FM, wxenv.SD, dev, param_count(net), start_iter), logf)

    N, T = args.games, args.steps
    NS = args.opp_slots
    opp_nets = [Policy(d=args.d).to(dev) for _ in range(NS)]
    t_begin = time.time()
    total_samples = 0
    ent_coef0 = args.ent_coef
    end_iter = min(args.max_iters, start_iter + args.iters) if args.iters > 0 else args.max_iters
    ent_decay = args.ent_decay_iters if args.ent_decay_iters > 0 else max(1.0, 0.75 * (end_iter if end_iter < 10 ** 9 else 1500.0))      # 自动：计划到的总轮数的3/4
    log('table %s | snap every %d prob %.2f slots %d | ent %.3f floor %.2f decay %g iters | seed %d' % (
        gen_pool.ACT, args.snap_every, args.snap_prob, NS, args.ent_coef, args.ent_floor, ent_decay, args.seed), logf)
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
            paths = gen_pool.generate(pool_dir, args.pool, 1000 + it + seed_off)
            pool = Pool(paths)
            pool.load()
            env = wxenv.Env(N, M)
            batch = Batch(env, dev)
            gall = torch.arange(N, device=dev)
        ta, tb, sa, sb = sample_games(pool, N, rng)
        reset_games(env, ta, tb, sa, sb, rng)
        batch.refresh()
        r = rng.random(N)
        # 0自博弈 1快照；两种对局里陪练(席位1)的前两回合都可能被换成随机开局(见下面 open_u)。
        # 随机出的招不是网络的决定，不参与学习。没有任何脚本对手/人类套路：套路应该由网络自己在自博弈里发现
        # 占比权重：自博弈0.32 快照snap_prob(默认0.45)，归一化
        probs = np.array([0.32, args.snap_prob])
        k_ = np.minimum(np.searchsorted(np.cumsum(probs / probs.sum()), r, side='right'), len(probs) - 1)
        opp_type = k_
        if not snaps:
            opp_type = np.where(opp_type == 1, 0, opp_type)
        slot = rng.integers(0, NS, N)
        for k in range(NS):
            if snaps:
                opp_nets[k].load_state_dict(snaps[rng.integers(0, len(snaps))])
        open_kind = bots.sample_open_kinds(rng, N, args.open_single, args.open_double)   # 每盘抽一个陪练开局种类(见 bots.py)，其余照常
        scripted = np.zeros((T, N, 2), dtype=bool)   # 这一步是随机出的招(不是网络的决定)
        learner = np.stack([np.ones(N, bool), opp_type == 0], 1)      # (N,2)：席位1只有自博弈时才学习
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
            for k in range(NS):
                sel = np.where((opp_type == 1) & (slot == k) & (done == 0))[0]
                if len(sel):
                    gi = torch.from_numpy(sel).to(dev)
                    one = torch.ones_like(gi)
                    ci, ai, bi, *_ = net_actions(opp_nets[k], batch, torch.from_numpy(state[sel, 1]).to(dev),
                                                 torch.from_numpy(hist[sel, 1]).to(dev), gi, one)
                    act[sel, 1] = torch.stack([ci, ai, bi], 1).cpu().numpy()
            if t < 2:    # 陪练开局：只替换席位1，席位0永远是网络自己的决定；只有"积攒+炸弹"开局要出到第2回合
                sel = np.where((done == 0) & ((open_kind == bots.OPEN_ACC_BOMB) if t == 1 else (open_kind != bots.OPEN_NONE)))[0]
                if len(sel):
                    act[sel, 1] = bots.opening_actions(env, sel, 1, rng, open_kind[sel], t)
                    scripted[t, sel, 1] = True
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
        ent_coef = ent_coef0 * max(args.ent_floor, 1.0 - it / ent_decay)
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

        if it % args.snap_every == args.snap_every - 1:
            sd = {k: (v.detach().cpu().half() if v.is_floating_point() else v.detach().cpu().clone()) for k, v in net.state_dict().items()}
            snaps.append(sd)
            os.makedirs(snap_dir, exist_ok=True)
            torch.save(sd, os.path.join(snap_dir, 'snap_%05d.pt' % (it + 1)))
        # ---- 训练指标：按对手类别分的胜率(指数滑动平均，单次迭代每类只有几十盘，噪声大)、动作分布、价值拟合、PPO更新幅度 ----
        groups = [('快照', opp_type == 1)]
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
            # 单招按效果类型展开：选单招的样本里，所选招式带某个效果的占比(一个招式带多个效果就每个都算一次，所以加起来可能超过1)
            sing = acts[ts, gs, ss, 0] == 3
            if sing.any():
                tbl = np.where(ss[sing] == 0, ta[gs[sing]], tb[gs[sing]])
                frac = pool.effect_matrix()[tbl, acts[ts, gs, ss, 1][sing]].mean(0)
                ne = 1 + len(gen_pool.EFFECT_NAMES)
                log('        ' + ' '.join(
                    '%s%.2f' % (nm, f) for nm, f in zip(['无效果'] + list(gen_pool.EFFECT_NAMES.values()), frac[:ne])), logf)
                log('        属性 ' + ' '.join('%s%.2f' % (nm, f) for nm, f in zip(['无', '金', '木', '水', '火', '土', '五行'], frac[ne:])), logf)
            log('        胜/败(滑动平均) ' + '  '.join('%s %.2f/%.2f' % (k, v[0], v[1]) for k, v in EMA.items() if k != '自博弈平局'), logf)
        if it % args.eval_every == args.eval_every - 1:
            net.eval()
            refs = eval_refs(CKPT, dev, args.d)
            res = evaluate(net, refs, eval_pool, dev, rng, n_games=args.eval_games)
            log('EVAL it %d  ' % it + ('  '.join('对%s 胜%.2f 败%.2f' % (k, w, l) for k, (w, l) in res.items()) or '(第一次评测，存为参照)'), logf)
            # 评测会把环境池换成评测池，重新载入训练池
            pool.load()
            env = wxenv.Env(N, M)
            batch = Batch(env, dev)
            torch.save({'net': net.state_dict(), 'opt': opt.state_dict(), 'iter': it + 1, 'args': vars(args)},
                       os.path.join(CKPT, 'latest.pt'))
            torch.save(net.state_dict(), os.path.join(CKPT, 'model_%05d.pt' % (it + 1)))
        next_it = it + 1
    torch.save({'net': net.state_dict(), 'opt': opt.state_dict(), 'iter': next_it, 'args': vars(args)},
               os.path.join(CKPT, 'latest.pt'))
    log('done at iter %d' % next_it, logf)


if __name__ == '__main__':
    main()
