"""冒烟测试：几分钟内验证 环境/特征/训练循环/导出 在PvP和PvE两种模式下都能跑通，再去开长时间训练(train_pvp.ps1 / train_pve.ps1 会先跑它)。

检查：
  1. PvP环境的维数(83/49)没变；PvE环境在它后面追加了维度，且没有五行/特性的对局里追加维度恒为0、前面的维度和PvP逐位一致；
  2. 随机配置(五行/暴击/特性)下的对局能跑完，所有特征都是有限数；
  3. PvP网络的权重补零列后装进PvE网络，在没有PvE配置的局面上输出和PvP网络完全一致(热启动不会一上来就把PvP的本事丢了)；
  4. train.py 两种模式各跑2轮(含评测和存检查点)，PvE从PvP检查点热启动；
  5. export_weights.py 两种模式都能导出(输出到临时目录，不动仓库里的权重文件)。
用法：python smoke_test.py [--keep]    --keep 保留临时文件
"""
import glob
import os
import shutil
import subprocess
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
os.chdir(HERE)
sys.path.insert(0, HERE)
KEEP = '--keep' in sys.argv


def say(msg):
    print('[smoke] ' + msg, flush=True)


def fail(msg):
    print('[smoke] FAILED: ' + msg, flush=True)
    sys.exit(1)


def run(args):
    r = subprocess.run([sys.executable] + args, cwd=HERE)
    if r.returncode != 0:
        fail(' '.join(args))


def main():
    import gen_pool
    import pve_gen
    import wxenv
    import bots

    paths = gen_pool.generate(os.path.join(HERE, 'pool_smoke'), 24, 4242)
    stats = np.array([gen_pool.read_stats(p) for p in paths])
    n = 48

    def load_pool():
        wxenv.pool_clear()
        for p in paths:
            assert wxenv.pool_add(p) >= 0, p

    def mk(st):
        return [st[0], st[1], st[2], gen_pool.std_agi(st[4]), st[3], 1.0, 0.2, st[4]]

    def games(rng):
        return [(int(rng.integers(0, len(paths))), int(rng.integers(0, len(paths)))) for _ in range(n)]

    # ---- 1. 维数和"无PvE配置时与PvP一致" ----
    import torch
    from model import Policy
    from train import Batch, M
    from pad_checkpoint import pad_to_shapes

    rng = np.random.default_rng(1)
    pairs = games(rng)

    wxenv.set_mode(False)
    if (wxenv.FM, wxenv.SD, wxenv.HK, wxenv.HD) != (83, 49, 8, 59):
        fail('PvP维数变了：%s' % ((wxenv.FM, wxenv.SD, wxenv.HK, wxenv.HD),))
    load_pool()
    env1 = wxenv.Env(n, M)
    for g, (a, b) in enumerate(pairs):
        env1.reset(g, a, b, mk(stats[a]), mk(stats[b]))
    st1, hi1, _ = env1.obs()
    fm1, sd1 = wxenv.FM, wxenv.SD
    torch.manual_seed(0)
    net1 = Policy(d=64).eval()
    moves1, form1 = env1.moves.copy(), env1.form.copy()

    wxenv.set_mode(True)
    if wxenv.FM <= fm1 or wxenv.SD <= sd1:
        fail('PvE维数没有比PvP多：%d/%d' % (wxenv.FM, wxenv.SD))
    say('PvP维数 %d/%d，PvE维数 %d/%d' % (fm1, sd1, wxenv.FM, wxenv.SD))
    load_pool()
    env2 = wxenv.Env(n, M)
    for g, (a, b) in enumerate(pairs):
        env2.reset(g, a, b, mk(stats[a]), mk(stats[b]), pve=None)
    st2, hi2, _ = env2.obs()
    if not (np.array_equal(st2[:, :, :sd1], st1) and np.array_equal(env2.moves[..., :fm1], moves1) and np.array_equal(hi2, hi1)):
        fail('没有PvE配置时，PvE环境的前面维度和PvP不一致')
    if np.abs(st2[:, :, sd1:]).max() != 0 or np.abs(env2.moves[..., fm1:]).max() != 0:
        fail('没有PvE配置时，追加的维度应该全是0')
    say('无配置时 PvE特征 = PvP特征 + 全零追加维度  OK')

    # ---- 3. 补零热启动：输出一致 ----
    torch.manual_seed(1)
    net2 = Policy(d=64).eval()
    sd = dict(net1.state_dict())
    grown = pad_to_shapes(sd, net2.state_dict())
    net2.load_state_dict(sd)
    say('补零的层：%s' % grown)

    def logits(net, env, state, hist):
        b = Batch(env, torch.device('cpu'))
        g = torch.arange(n)
        outs = []
        for seat in (0, 1):
            s = torch.full_like(g, seat)
            es, eo, ms, mo, fs = b.inputs(g, s, net)
            with torch.no_grad():
                h, ctx, v, cl, l1 = net(torch.from_numpy(state[:, seat]), torch.from_numpy(hist[:, seat]), es, eo, ms, mo, fs)
            outs.append((cl, l1, v))
        return outs

    # PvP网络要用PvP的env维数：env1在PvP模式下建的，Batch只读它的数组，所以可以直接用
    o1 = logits(net1, env1, st1, hi1)
    o2 = logits(net2, env2, st2, hi2)
    for (c1, l1, v1), (c2, l2, v2) in zip(o1, o2):
        if not (torch.allclose(c1, c2, atol=1e-4) and torch.allclose(l1, l2, atol=1e-3) and torch.allclose(v1, v2, atol=1e-4)):
            fail('补零后的PvE网络输出和PvP网络不一致(cls最大差 %.2e)' % (c1 - c2).abs().max())
    say('PvP网络补零后装进PvE网络，输出一致  OK')

    # ---- 2. 随机PvE配置下对局能跑完、特征有限 ----
    env3 = wxenv.Env(n, M)
    rng = np.random.default_rng(2)
    extras_seen = 0.0
    for rep in range(3):
        for g, (a, b) in enumerate(games(rng)):
            env3.reset(g, a, b, mk(stats[a]), mk(stats[b]), pve=pve_gen.sample_config(rng, stats[a][1], stats[b][1]))
        extras_seen += np.abs(env3.moves[..., fm1:]).sum()
        for t in range(80):
            state, hist, done = env3.obs()
            if not (np.isfinite(state).all() and np.isfinite(hist).all() and np.isfinite(env3.moves).all()):
                fail('PvE对局里出现了非有限的特征')
            if done.all():
                break
            act = np.zeros((n, 2, 3), dtype=np.int32)
            for s in (0, 1):
                act[:, s] = bots.actions(env3, np.arange(n), s, rng.integers(0, 4, n), bots.pick_family(env3, np.arange(n), s, rng), rng)
            env3.step(act)
    if extras_seen <= 0:
        fail('随机PvE配置下追加的招式特征全是0，配置没生效')
    say('随机五行/暴击/特性配置的对局 %d 盘跑完  OK' % (3 * n))

    # ---- 4. train.py 两种模式 ----
    tiny = ['--games', '32', '--steps', '10', '--pool', '16', '--iters', '2', '--eval_every', '2', '--eval_games', '16', '--d', '64', '--hours', '1']
    run(['train.py', '--ckpt_dir', 'checkpoints_smoke_pvp'] + tiny)
    run(['train.py', '--mode', 'pve', '--init', os.path.join('checkpoints_smoke_pvp', 'latest.pt'), '--ckpt_dir', 'checkpoints_smoke_pve', '--lr', '1.5e-4'] + tiny)
    run(['train.py', '--mode', 'pve', '--resume', '--ckpt_dir', 'checkpoints_smoke_pve'] + tiny[:6] + ['--iters', '1'] + tiny[8:])
    say('train.py PvP / PvE(热启动) / PvE(续训) 各跑通  OK')

    # ---- 5. 导出到临时目录 ----
    tmp = tempfile.mkdtemp(prefix='wxdj_export_')
    run(['export_weights.py', os.path.join('checkpoints_smoke_pvp', 'latest.pt'), '--out_dir', tmp])
    run(['export_weights.py', '--pve', os.path.join('checkpoints_smoke_pve', 'latest.pt'), '--out_dir', tmp])
    for f in ('ai_weights_pvp.js', 'ai_weights_pvp.bin', 'ai_weights_pve.js', 'ai_weights_pve.bin'):
        if not os.path.exists(os.path.join(tmp, f)):
            fail('没有导出 ' + f)
    say('导出权重 OK (%s)' % tmp)

    if not KEEP:
        for d in ('checkpoints_smoke_pvp', 'checkpoints_smoke_pve', 'pool_smoke'):
            shutil.rmtree(os.path.join(HERE, d), ignore_errors=True)
        shutil.rmtree(tmp, ignore_errors=True)
    say('ALL OK')


if __name__ == '__main__':
    main()
