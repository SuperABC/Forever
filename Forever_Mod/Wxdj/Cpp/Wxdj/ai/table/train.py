"""为xiaohua.txt的每一式训练一份对战策略。

第k式的训练表 = 每个家族的第k式各一个(共15个招式)；双方属性 = 第k式所在阶位的标准初始属性。
训练分两步(和游戏里ai_policy.txt当初的做法一致)：第一层(出哪一类招)冻结为seed_policy.txt，
只训练第二层(类别内选哪个家族)，先训40轮再接着训25轮。

用法：python train.py                 训练全部20式
      python train.py 1 5 20          只训练指定的式
      python train.py --iters 10      每步轮数改小(快速试跑)
输出：tables/form_XX.txt(训练用的招式表)、policies/policy_XX.txt(策略)、policies/summary.txt(每式的战斗回合数和招式使用率)。
"""
import math
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ACT = os.path.join(HERE, '..', '..', '..', '..', 'Resource', 'act', 'xiaohua.txt')
TRAINER = os.path.join(HERE, 'trainer.exe')
SEED_POLICY = os.path.join(HERE, 'seed_policy.txt')
TABLES = os.path.join(HERE, 'tables')
POLICIES = os.path.join(HERE, 'policies')

BASELINE = '无招'
# 阶位 -> 包含的式(按xiaohua.txt里的阶位列)
STAGES = [[1], [2], [3], [4], [5], [6], [7, 8], [9, 10], [11, 12], [13, 14], [15, 16, 17], [18, 19, 20]]
# 标准初始属性：基准值 x 尺度 x 缩放^t，见battle/battle.md的"标准初始属性"
BASE = dict(hp=1000, atk=1200, mp=800, rec=20)
SHRINK_HP, SHRINK_ATK, GROW_MP = 0.6, 0.8, 1.2
T_MAX, T_FROM, T_TO = 0.8, 3.0, 12.0


def load_forms(path):
    """返回[(家族名, [第1式的行块, 第2式的行块, ...]), ...]，行块=招式行+紧跟的效果行。"""
    families = []
    lines = open(path, encoding='utf-8').read().split('\n')
    cur = None
    for line in lines:
        if not line.strip() or line.startswith('#'):
            continue
        t = line.split(' ')
        if len(t) == 6 and re.match(r'^\d+-\d+$', t[4]):
            name = t[0][:t[0].index('第')] if '第' in t[0] else t[0]
            if cur is None or cur[0] != name:
                cur = (name, [])
                families.append(cur)
            cur[1].append([line])
        else:
            cur[1][-1].append(line)
    return families


def nice(v):
    step = 10 if v < 10000 else (100 if v < 100000 else 1000)
    return int(round(v / step) * step)


def stage_stats(forms, families):
    base = dict(families)[BASELINE]
    power = [int(b[0].split(' ')[3]) for b in base]
    cost = [int(b[0].split(' ')[2]) for b in base]
    sp = math.exp(sum(math.log(power[k - 1] / power[0]) for k in forms) / len(forms))
    sc = math.exp(sum(math.log(cost[k - 1] / cost[0]) for k in forms) / len(forms))
    mean = sum(forms) / len(forms)
    t = T_MAX * min(1.0, max(0.0, (mean - T_FROM) / (T_TO - T_FROM)))
    hp = BASE['hp'] * sp * math.pow(SHRINK_HP, t)
    atk = BASE['atk'] * sp * math.pow(SHRINK_ATK, t)
    mp = BASE['mp'] * sc * math.pow(GROW_MP, t)
    rec = BASE['rec'] * sc * math.pow(GROW_MP, t)
    return dict(hp=nice(hp), atk=nice(atk), mp=nice(mp), rec=max(1, round(rec)))


def train(table, out, stats, init, iters, seed):
    args = [TRAINER, table, 'learn', '0', '0', '0', '14', str(seed), 'iters=%d' % iters, 'epi=84000', 'K=40',
            'tau1=0.08', 'tau2=0.02', 'out=' + out, 'init=' + init, 'freeze1=1'] + \
           ['%s=%s' % (k, v) for k, v in stats.items()]
    return subprocess.run(args, capture_output=True, text=True, encoding='utf-8', cwd=HERE).stdout


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    argv = sys.argv[1:]
    iters1, iters2 = 40, 25
    if '--iters' in argv:
        i = argv.index('--iters')
        iters1 = iters2 = int(argv[i + 1])
        del argv[i:i + 2]
    forms = [int(x) for x in argv] or list(range(1, 21))

    families = load_forms(ACT)
    assert all(len(b) == 20 for _, b in families), '每个家族都应该有20式'
    os.makedirs(TABLES, exist_ok=True)
    os.makedirs(POLICIES, exist_ok=True)
    summary = []
    for k in forms:
        stage = next(s for s in STAGES if k in s)
        stats = stage_stats(stage, families)
        table = os.path.join(TABLES, 'form_%02d.txt' % k)
        with open(table, 'w', encoding='utf-8', newline='\n') as f:
            for _, blocks in families:
                f.write('\n'.join(blocks[k - 1]) + '\n')
        policy = os.path.join(POLICIES, 'policy_%02d.txt' % k)
        train(table, policy, stats, SEED_POLICY, iters1, 10)
        o = train(table, policy, stats, policy, iters2, 11)
        rounds = re.search(r'selfplay rounds ([\d.]+)', o)
        usage = re.search(r'action usage:(.*)', o)
        line = '第%d式 属性%s 自对弈回合数%s 招式使用率%s' % (k, stats, rounds.group(1) if rounds else '?', usage.group(1).strip() if usage else '?')
        print(line, flush=True)
        summary.append(line)
        with open(os.path.join(POLICIES, 'summary.txt'), 'a', encoding='utf-8') as f:
            f.write(line + '\n')


if __name__ == '__main__':
    main()
