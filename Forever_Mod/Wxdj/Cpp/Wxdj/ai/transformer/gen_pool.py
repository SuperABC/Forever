"""生成训练用的招式文件池(随机化的招式表)。

每个文件是xiaohua.txt格式；第一行注释里带这份文件对应的标准初始属性(# stats hp atk mp rec)。
文件内容 = 真实招式家族的随机子集/随机扰动 + 少量随机合成的家族，式的编号取一个共用的子集，
这样同式双招的约束在每个文件里都有意义。让模型见到各种各样的招式表，才能做到"换了招式文件也能出招"。
"""
import math
import os
import random
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ACT = os.path.join(HERE, '..', '..', '..', '..', 'Resource', 'act', 'xiaohua.txt')
BASELINE = '无招'
ATTRS = ['none', 'metal', 'wood', 'water', 'fire', 'earth', 'all']
RATIO_EFFECTS = ('penetrate', 'physical', 'reduce', 'rebound', 'absorb')
TIMED_EFFECTS = ('dot', 'recover', 'cure', 'rebate')

SHRINK_HP, SHRINK_ATK, GROW_MP = 0.6, 0.8, 1.2
BASE = dict(hp=1000, atk=1200, mp=800, rec=20)


class Family:
    def __init__(self, name, forms):
        self.name = name
        self.forms = forms     # 20个: dict(suffix, attr, mp, power, realm, effects=[[tok...],...])


def load_real(path=ACT):
    fams, cur = [], None
    for line in open(path, encoding='utf-8').read().split('\n'):
        if not line.strip() or line.startswith('#'):
            continue
        t = line.split(' ')
        if len(t) == 6 and re.match(r'^\d+-\d+$', t[4]):
            p = t[0].index('第')
            name, suffix = t[0][:p], t[0][p:]
            if cur is None or cur.name != name:
                cur = Family(name, [])
                fams.append(cur)
            cur.forms.append(dict(suffix=suffix, attr=t[1], mp=int(t[2]), power=int(t[3]), realm=t[4], effects=[]))
        else:
            cur.forms[-1]['effects'].append(t)
    return fams


REAL = load_real()
def realm_max_form(form):
    return [1, 2, 3, 4, 5, 6, 8, 8, 10, 10, 12, 12, 14, 14, 17, 17, 17, 20, 20, 20][min(20, max(1, form)) - 1]


BOMB_MULT = {1: 1.0, 2: 1.2, 3: 1.5, 4: 1.9, 5: 2.4, 6: 3.0, 8: 3.7, 10: 4.5, 12: 5.6, 14: 7.0, 17: 8.5, 20: 10.0}   # 阶位最大式 -> 炸弹积攒倍率


def std_agi(mult):
    """标准敏捷 = 5 + 阶位最大式/4(第1式5.25 ... 第20式10)；炸弹倍率 mult 按阶位给定，取最接近的阶位反推最大式。"""
    mf = min(BOMB_MULT, key=lambda k: abs(BOMB_MULT[k] - mult))
    return 5.0 + mf / 4.0


RULER = [(f['mp'], f['power']) for f in next(x for x in REAL if x.name == BASELINE).forms]
SUFFIX = [f['suffix'] for f in REAL[0].forms]


def stage_stats(forms):
    sp = math.exp(sum(math.log(RULER[k - 1][1] / RULER[0][1]) for k in forms) / len(forms))
    sc = math.exp(sum(math.log(RULER[k - 1][0] / RULER[0][0]) for k in forms) / len(forms))
    t = 0.8 * min(1.0, max(0.0, (sum(forms) / len(forms) - 3.0) / 9.0))
    return dict(hp=BASE['hp'] * sp * SHRINK_HP ** t, atk=BASE['atk'] * sp * SHRINK_ATK ** t,
                mp=BASE['mp'] * sc * GROW_MP ** t, rec=BASE['rec'] * sc * GROW_MP ** t,
                mult=BOMB_MULT[realm_max_form(max(forms))])


def fmt(v):
    return ('%.4f' % v).rstrip('0').rstrip('.')


def scale_effect(tok, rng, em, pm):
    """按强度em缩放比例类效果，按威力倍数pm缩放绝对数值类效果。"""
    kind = tok[0]
    if kind in RATIO_EFFECTS:
        vals = [float(x) for x in tok[1:]]
        # 反弹比例必须小于1：双方反弹互相弹回去，比例>=1会无限递归(栈溢出)
        cap = 0.8 if kind == 'rebound' else 1.5
        return [kind] + [fmt(min(cap, max(0.0, v * em))) for v in vals]
    if kind == 'lock':
        return [kind, fmt(min(0.95, float(tok[1]) * em))]
    if kind in TIMED_EFFECTS:
        a, b, rounds, interval = tok[1:5]
        return [kind, str(max(1, round(int(a) * pm * em))), str(max(1, round(int(b) * pm * em))), rounds, interval]
    return tok


def new_effect_spec(rng):
    """合成家族的一个效果：类型+固定的形状参数，之后按第k式的强度生成每一式的数值。"""
    kind = rng.choice(list(RATIO_EFFECTS) + ['lock'] + list(TIMED_EFFECTS))
    spec = dict(kind=kind, rel=rng.uniform(0.15, 1.0))
    if kind in RATIO_EFFECTS:
        spec['shape'] = [rng.uniform(0.8, 1.2) for _ in range(6)] + [rng.uniform(0.6, 1.0)]
    elif kind in TIMED_EFFECTS:
        spec['rounds'] = rng.randint(2, 7)
        spec['interval'] = rng.choice([1, 2])
        spec['frac'] = rng.uniform(0.03, 0.12)
    return spec


def effect_line(spec, k, power):
    kind, level = spec['kind'], spec['rel'] * k / 20.0
    if kind in RATIO_EFFECTS:
        vals = [min(0.8 if kind == 'rebound' else (1.0 if kind == 'physical' else 1.2), level * s) for s in spec['shape']]
        return [kind] + [fmt(v) for v in vals]
    if kind == 'lock':
        return [kind, fmt(min(0.9, level))]
    amt = power * spec['frac'] * spec['rel']
    return [kind, str(max(1, round(amt * 2))), str(max(1, round(amt))), str(spec['rounds']), str(spec['interval'])]


def make_family(rng, forms, idx, mode):
    """mode: perturbed(真实家族的随机扰动) / synthetic(合成)。返回(家族名, 每式一个块的行文本)。"""
    if mode == 'synthetic':
        name = '新%d' % idx
        attr = rng.choice(ATTRS)
        cm, pm = rng.lognormvariate(0, 0.5), rng.lognormvariate(0, 0.5)
        specs = [new_effect_spec(rng) for _ in range(rng.choice([0, 0, 1, 1, 2]))]
        blocks = []
        for k in forms:
            mp, power = max(1, round(RULER[k - 1][0] * cm)), max(1, round(RULER[k - 1][1] * pm))
            block = ['%s%s %s %d %d 3-1 %d' % (name, SUFFIX[k - 1], attr, mp, power, len(specs))]
            block += [' '.join(effect_line(sp, k, power)) for sp in specs]
            blocks.append(block)
        return name, blocks
    fam = rng.choice(REAL)
    while fam.name == BASELINE and rng.random() < 0.7:
        fam = rng.choice(REAL)
    name = fam.name if rng.random() < 0.5 else fam.name + str(idx)
    cm, pm, em = rng.lognormvariate(0, 0.4), rng.lognormvariate(0, 0.4), rng.lognormvariate(0, 0.5)   # 随机范围加大：让模型对1~20式都通用
    attr = fam.forms[0]['attr']
    if rng.random() < 0.25:
        attr = rng.choice(ATTRS)
    drop = rng.random() < 0.15
    extra = new_effect_spec(rng) if rng.random() < 0.2 else None
    blocks = []
    for k in forms:
        f = fam.forms[k - 1]
        mp, power = max(1, round(f['mp'] * cm)), max(1, round(f['power'] * pm))
        effs = [scale_effect(e, rng, em, pm) for e in f['effects']]
        if drop and effs:
            effs = effs[1:]
        if extra is not None and len(effs) < 2:
            effs.append(effect_line(extra, k, power))
        block = ['%s%s %s %d %d %s %d' % (name, f['suffix'], attr, mp, power, f['realm'], len(effs))]
        block += [' '.join(e) for e in effs]
        blocks.append(block)
    return name, blocks


def make_file(rng, idx, max_moves=60):
    r = rng.random()
    nfam_pool = len(REAL)
    if r < 0.25:                       # 真实招式表(完整15个家族)，式取一个小子集
        mode, nfam = 'real', 15
    elif r < 0.75:
        mode, nfam = 'perturbed', rng.randint(3, 15)
    else:
        mode, nfam = 'mixed', rng.randint(4, 15)
    nforms = rng.choice([1, 1, 2, 2, 3, 4, 5, 6, 8, 10, 12])
    nforms = max(1, min(nforms, max_moves // nfam))
    if rng.random() < 0.5:             # 连续的式(同一阶位)
        start = rng.randint(1, 20 - nforms + 1)
        forms = list(range(start, start + nforms))
    else:
        forms = sorted(rng.sample(range(1, 21), nforms))
    if mode == 'real':
        chosen = [(f.name, [ [ '%s%s %s %d %d %s %d' % (f.name, f.forms[k - 1]['suffix'], f.forms[k - 1]['attr'],
                              f.forms[k - 1]['mp'], f.forms[k - 1]['power'], f.forms[k - 1]['realm'], len(f.forms[k - 1]['effects'])) ]
                            + [' '.join(e) for e in f.forms[k - 1]['effects']] for k in forms ]) for f in REAL]
    else:
        chosen = []
        for i in range(nfam):
            m = 'perturbed' if mode == 'perturbed' else ('synthetic' if rng.random() < 0.5 else 'perturbed')
            chosen.append(make_family(rng, forms, i, m))
    stats = stage_stats(forms)
    lines = ['# stats %s' % ' '.join(fmt(stats[k]) for k in ('hp', 'atk', 'mp', 'rec', 'mult')), '# forms %s' % ' '.join(map(str, forms))]
    for _, blocks in chosen:
        for b in blocks:
            lines.extend(b)
    return '\n'.join(lines) + '\n', stats, forms


def generate(out_dir, count, seed, max_moves=60):
    os.makedirs(out_dir, exist_ok=True)
    rng = random.Random(seed)
    paths = []
    for i in range(count):
        text, stats, forms = make_file(rng, i, max_moves)
        p = os.path.join(out_dir, 'f%05d.txt' % i)
        open(p, 'w', encoding='utf-8', newline='\n').write(text)
        paths.append(p)
    return paths


def read_stats(path):
    first = open(path, encoding='utf-8').readline().split()
    return [float(x) for x in first[2:7]]


if __name__ == '__main__':
    import sys
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    ps = generate(os.path.join(HERE, 'pool_test'), n, 1)
    print(len(ps), 'files; sample:')
    print(open(ps[0], encoding='utf-8').read()[:800])


def real_text(forms):
    """真实招式表(15个家族)在给定式的子集上的文件内容。"""
    stats = stage_stats(forms)
    lines = ['# stats %s' % ' '.join(fmt(stats[k]) for k in ('hp', 'atk', 'mp', 'rec', 'mult')), '# forms %s' % ' '.join(map(str, forms))]
    for f in REAL:
        for k in forms:
            x = f.forms[k - 1]
            lines.append('%s%s %s %d %d %s %d' % (f.name, x['suffix'], x['attr'], x['mp'], x['power'], x['realm'], len(x['effects'])))
            lines += [' '.join(e) for e in x['effects']]
    return '\n'.join(lines) + '\n'


def write_real(out_dir, forms_list):
    os.makedirs(out_dir, exist_ok=True)
    paths = []
    for forms in forms_list:
        p = os.path.join(out_dir, 'real_%s.txt' % '_'.join(map(str, forms)))
        open(p, 'w', encoding='utf-8', newline='\n').write(real_text(forms))
        paths.append(p)
    return paths
