"""生成训练用的招式文件池(随机化的招式表)。

每个文件是xiaohua.txt格式；第一行注释里带这份文件对应的标准初始属性(# stats hp atk mp rec mult)，第二行是式的列表(# forms)。
一个文件 = 一场对局：所有家族共用同一个连续的WINDOW(5)式窗口(最高式M往下5个式，M<5时是1..M)，里面有10~25个虚拟家族。
虚拟家族由"效果模板"(表里每个无属性的单效果家族)重组而来，数值全部由参数推出来，让模型学会"换了招式表、换了数值也能出招"。
每个虚拟家族的属性从 ATTRS(无/金/木/水/火/土/五行)里等概率抽一个(各1/7)：单属性威力 x0.7、五行 x0.5(attr_factor)，消耗和效果参数不变，
与 complete.txt 里每种属性每个效果一个家族的定价一致；玩家本身仍无五行属性(PvP)，属性只体现在招式上(克制、元素反应、效果里对各属性的比例)。

虚拟家族的生成规律(见 sample_family_params / build_family)：
  1. 每个文件有且只有一个无效果家族，其余家族挂 1/2/3 个效果，比例 0.6/0.3/0.1；同一招不重复同类效果。
  2. 每个效果采样一个统一倍率 m ~ N(1, 0.3)(截断到 [0.3, 效果自身上限])，m 乘到该效果的全部数值参数上；
     然后用 m 反向补偿基础消耗/威力："偏离量 x m"：对每一式k，比值 = 1 + m x (模板比值(k) - 1)，
     模板比值 = 模板家族的消耗(或威力) / 无属性无效果同式的对应值(即表里这个效果的"定价")。m=1 正好还原表里的数。
     多个效果按先后顺序把各自的比值连乘到标尺(无属性无效果)的消耗/威力上。
  3. 再加三类小噪声(都是 N(1, 0.1)，截断到[0.6, 1.4]，每个家族采样一次、各式共用，不参与补偿)：消耗一个、威力一个、
     效果的每个数值参数各一个。
  持续类效果(dot/rebate/recover/cure)的轮数、间隔照模板不变；比例类效果的各属性分量、lock概率、increase/static/shield比例都直接乘倍率。
"""
import math
import os
import random
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ACT = os.environ.get('WXDJ_ACT') or os.path.join(HERE, '..', '..', 'Cpp', 'act', 'complete.txt')   # WXDJ_ACT：指定招式表(训练脚本用它冻结一份副本，避免训练途中招式表被改)
BASELINE_NAMES = ('无招', '无属性无效果')     # 数值标尺家族(无属性、无效果)：旧表叫无招，complete.txt里叫无属性无效果
ATTRS = ['none', 'metal', 'wood', 'water', 'fire', 'earth', 'all']
RATIO_EFFECTS = ('penetrate', 'physical', 'reduce', 'rebound', 'absorb')
TIMED_EFFECTS = ('dot', 'recover', 'cure', 'rebate')
WINDOW = 5               # 一场对局里每个家族有几个连续的式

SHRINK_HP, SHRINK_ATK, GROW_MP = 0.6, 0.8, 1.2
BASE = dict(hp=1000, atk=1200, mp=800, rec=20)
REC_FRACTION = 0.05      # 每回合自然回复真气占真气上限的比例


class Family:
    def __init__(self, name, forms):
        self.name = name
        self.forms = forms     # 20个: dict(suffix, attr, mp, power, realm, effects=[[tok...],...])


def load_real(path=ACT):
    fams, cur = [], None
    for line in open(path, encoding='utf-8').read().split('\n'):
        line = line.rstrip('\r')
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


ALL = load_real()                                      # 招式表里的全部家族(训练用的效果模板、数值标尺都从它来)
BASELINE = next((n for n in BASELINE_NAMES if any(f.name == n for f in ALL)), None)
if BASELINE is None:
    raise SystemExit('招式表里没有数值标尺家族(%s)：%s' % ('/'.join(BASELINE_NAMES), ACT))
# REAL = 被评测的家族。WXDJ_SELECT(逗号分隔的名字片段，家族名包含任意一个就选中)选子集；不设就是全部。
# complete.txt有98个家族x5式=490个招式，远超环境容量(MAX_MOVES)，所以评测时必须用WXDJ_SELECT选一个子集(如"无效果"、"火属性"、"穿透")。
_SELECT = [x for x in os.environ.get('WXDJ_SELECT', '').split(',') if x]
REAL = [f for f in ALL if any(x in f.name for x in _SELECT)] if _SELECT else ALL
MAX_MOVES = 125            # 每方最多招式数(环境容量128)
MAX_FAMS = MAX_MOVES // 5  # 5式窗口下最多家族数


def realm_max_form(form):
    return [1, 2, 3, 4, 5, 6, 8, 8, 10, 10, 12, 12, 14, 14, 17, 17, 17, 20, 20, 20][min(20, max(1, form)) - 1]


BOMB_MULT = {1: 2.0, 2: 2.5, 3: 3.0, 4: 3.5, 5: 4.0, 6: 4.5, 8: 5.0, 10: 6.0, 12: 7.0, 14: 8.0, 17: 9.0, 20: 10.0}   # 阶位最大式 -> 炸弹积攒倍率


def std_agi(mult):
    """标准敏捷 = 5 + 阶位最大式/4(第1式5.25 ... 第20式10)；炸弹倍率 mult 按阶位给定，取最接近的阶位反推最大式。"""
    mf = min(BOMB_MULT, key=lambda k: abs(BOMB_MULT[k] - mult))
    return 5.0 + mf / 4.0


RULER = [(f['mp'], f['power']) for f in next(x for x in ALL if x.name == BASELINE).forms]
SUFFIX = [f['suffix'] for f in ALL[0].forms]


# 第1式的健康/武力/真气依次改过两次(每次第20式都保持不变)：先改成 FIRST_STEPS[0]，再改成 FIRST_STEPS[1]。
# 每次改动的比例 = 这次第1式的值 / 上一次第1式的值，随阶位最大式从第1式线性插值到第20式的1，各次的比例相乘
FIRST_STEPS = [dict(hp=1000, atk=1200, mp=800), dict(hp=600, atk=1000, mp=800), dict(hp=600, atk=800, mp=1000)]   # 第一项是原始基准


def first_form_ratio(key, max_form):
    r = 1.0
    for prev, cur in zip(FIRST_STEPS, FIRST_STEPS[1:]):
        r1 = cur[key] / prev[key]
        r *= r1 + (1.0 - r1) * (max_form - 1) / 19.0
    return r


def load_stage_table(path=ACT):
    """招式表开头注释里的"各阶位标准三维"表：每行 "# 阶位 包含的式 健康 武力 真气"(如 "# 4-6   18-20   25800 38900 24800")。
    返回 [(式下限, 式上限, 健康, 武力, 真气)]；表里没有这张表就返回空(用下面stage_stats里的公式)。手改了表里的数就会直接生效。"""
    rows = []
    for line in open(path, encoding='utf-8').read().split('\n'):
        m = re.match(r'^#\s+(\d-\d)\s+(\d+)(?:-(\d+))?\s+(\d+)\s+(\d+)\s+(\d+)\s*$', line.rstrip('\r'))
        if m:
            lo = int(m.group(2))
            rows.append((lo, int(m.group(3) or lo), int(m.group(4)), int(m.group(5)), int(m.group(6))))
    if rows and sorted(f for lo, hi, *_ in rows for f in range(lo, hi + 1)) != list(range(1, 21)):
        raise SystemExit('阶位三维表没有恰好覆盖第1~20式(每式一行且不重叠)：%s' % rows)
    return rows


STAGES = load_stage_table()


def stage_stats(forms):
    mf = realm_max_form(max(forms))
    sp = math.exp(sum(math.log(RULER[k - 1][1] / RULER[0][1]) for k in forms) / len(forms))
    sc = math.exp(sum(math.log(RULER[k - 1][0] / RULER[0][0]) for k in forms) / len(forms))
    t = 0.8 * min(1.0, max(0.0, (sum(forms) / len(forms) - 3.0) / 9.0))
    st = dict(hp=BASE['hp'] * first_form_ratio('hp', mf) * sp * SHRINK_HP ** t, atk=BASE['atk'] * first_form_ratio('atk', mf) * sp * SHRINK_ATK ** t,
              mp=BASE['mp'] * first_form_ratio('mp', mf) * sc * GROW_MP ** t, rec=BASE['rec'] * sc * GROW_MP ** t,
              mult=BOMB_MULT[realm_max_form(max(forms))])
    for lo, hi, hp, atk, mp in STAGES:      # 招式表注释里的三维优先(以最高式所在阶位为准)；每回合回复真气和炸弹倍率仍按公式
        if lo <= max(forms) <= hi:
            st['hp'], st['atk'], st['mp'] = hp, atk, mp
    st['rec'] = REC_FRACTION * st['mp']     # 每回合自然回复真气 = 真气上限的5%(2026-11-04起，取代按消耗比的公式)
    return st


def fmt(v):
    return ('%.4f' % v).rstrip('0').rstrip('.')


def attr_factor(attr):
    """同式同消耗下，该属性的威力相对无属性的比例：无属性1.0，单属性0.7，五行0.5。"""
    return 1.0 if attr == 'none' else (0.5 if attr == 'all' else 0.7)


# ---- 效果模板：表里每个"只带一个无属性效果"的家族 ----
M_SIGMA, M_LO, M_MAX = 0.3, 0.3, 2.5               # 每个效果统一的倍率 m ~ N(1, 0.3)，截断到 [0.3, min(2.5, 效果自身上限)]
NOISE_SIGMA, NOISE_LO, NOISE_HI = 0.1, 0.6, 1.4    # 消耗、威力、效果每个数值参数各自的小噪声 N(1, 0.1)，不参与反向补偿
RATIO_FLOOR = 0.05                                 # 反向补偿后的消耗/威力比值下限(比值<<1的效果在大m时 1+m(r-1) 会变负)
HARD_CAP = {'rebound': 0.8, 'lock': 0.95, 'shield': 0.6}   # 效果数值的硬上限：反弹必须<1(双方反弹互相弹回去会无限递归)，禁锢是概率，盾量别超过武力上限的60%
EFFECT_COUNT_WEIGHTS = [0.6, 0.3, 0.1]             # 带效果的家族挂 1/2/3 个效果；每个文件另外有且只有一个无效果家族(见 make_file)


class Template:
    def __init__(self, fam):
        self.name = fam.name
        self.attr = fam.forms[0]['attr']
        self.kind = fam.forms[0]['effects'][0][0]
        f = attr_factor(self.attr)
        self.rc = [x['mp'] / RULER[k][0] for k, x in enumerate(fam.forms)]                  # 消耗比(相对无属性无效果同式)
        self.rp = [x['power'] / RULER[k][1] / f for k, x in enumerate(fam.forms)]           # 威力比(相对无属性无效果同式，已去掉属性系数)
        self.eff = [x['effects'][0] for x in fam.forms]                                     # 每式的效果行(令牌列表)
        first = self.eff[0]
        # 效果行里前 n_params 个数值令牌是"效果参数"(乘倍率的对象)：比例类是各属性分量；持续类是立即量和每段量；其余是1个比例/概率
        self.n_params = len(first) - 1 if self.kind in RATIO_EFFECTS else (2 if self.kind in TIMED_EFFECTS else 1)
        vmax = max(float(e[i]) for e in self.eff for i in range(1, 1 + self.n_params))
        cap = HARD_CAP.get(self.kind)
        # m 的上限：不让倍率本身把效果参数推过硬上限(否则效果被截断，和补偿量对不上)
        self.m_hi = M_MAX if cap is None or vmax <= 0 else max(1.0, min(M_MAX, cap / vmax))


def build_templates(fams=None):
    out = []
    for fam in (fams or ALL):
        if fam.name == BASELINE or len(fam.forms) != 20 or fam.forms[0]['attr'] != 'none':      # 只要无属性的
            continue
        if all(len(x['effects']) == 1 for x in fam.forms) and len({x['effects'][0][0] for x in fam.forms}) == 1:
            out.append(Template(fam))
    return out


TEMPLATES = build_templates()
# 效果类型(效果行第一个令牌) -> 显示名，训练日志里"单招按效果类型占比"用；顺序就是日志里的顺序
EFFECT_NAMES = {t.kind: t.name.replace('无属性', '') for t in TEMPLATES}


def scale_effect(tmpl, k, m, jx):
    """第k式的效果行：前 n_params 个数值参数各乘 m 和自己的小噪声 jx[i]；轮数、间隔等其余令牌照模板。"""
    tok = tmpl.eff[k - 1]
    cap = HARD_CAP.get(tmpl.kind)
    out = [tok[0]]
    for i in range(1, len(tok)):
        if i > tmpl.n_params:
            out.append(tok[i])
            continue
        v = float(tok[i]) * m * jx[i - 1]
        if tmpl.kind in TIMED_EFFECTS:
            out.append(str(max(1, round(v))))
        else:
            out.append(fmt(max(0.0, v) if cap is None else min(cap, max(0.0, v))))
    return out


def window_forms(top, n=WINDOW):
    """以top为最高式的窗口：top往下n个式(不足就从第1式起)。"""
    return list(range(max(1, top - n + 1), top + 1))


def sample_window(rng):
    if rng.random() < 0.05:
        return list(range(1, rng.randint(1, WINDOW - 1) + 1))     # 最高式M<5：窗口就是1..M
    s = rng.randint(1, 20 - WINDOW + 1)
    return list(range(s, s + WINDOW))


def sample_family_params(rng, must=None, n=None):
    """一个虚拟家族的全部随机量：效果模板(有先后顺序)、每个效果的倍率m、消耗/威力噪声、每个效果每个数值参数的噪声。
    must：必须带上的效果模板(至少1个效果；它在效果顺序里的位置也是随机的)。
    n：效果个数；不给就按 EFFECT_COUNT_WEIGHTS 抽 1/2/3 个，n=0 是无效果家族。"""
    if n is None:
        n = rng.choices([1, 2, 3], weights=EFFECT_COUNT_WEIGHTS)[0]
    tmpls, kinds = [], set()
    cands = TEMPLATES[:]
    rng.shuffle(cands)
    if must is not None:
        n = max(n, 1)
        cands = [must] + [t for t in cands if t.kind != must.kind]
    for t in cands:
        if len(tmpls) >= n:
            break
        if t.kind not in kinds:              # 同一个招式里不重复同类效果
            kinds.add(t.kind)
            tmpls.append(t)
    if must is not None:
        rng.shuffle(tmpls)
    noise = lambda: min(NOISE_HI, max(NOISE_LO, rng.gauss(1.0, NOISE_SIGMA)))
    ms = [min(t.m_hi, max(M_LO, rng.gauss(1.0, M_SIGMA))) for t in tmpls]
    return dict(tmpls=tmpls, ms=ms, jc=noise(), jp=noise(), jxs=[[noise() for _ in range(t.n_params)] for t in tmpls], attr=rng.choice(ATTRS))


def build_family(forms, idx, tmpls, ms, jc=1.0, jp=1.0, jxs=None, realm='3-1', attr='none'):   # attr：家族属性，威力再乘 attr_factor(attr)
    """按给定的随机量构造一个家族(不含随机)，返回每式一个块的行文本。
    基础消耗(威力) = 标尺(k) x 连乘[max(RATIO_FLOOR, 1 + m x (模板比值(k) - 1))] x 噪声jc(jp)；效果行见 scale_effect。"""
    jxs = jxs or [[1.0] * t.n_params for t in tmpls]
    name = '新%d' % idx
    blocks = []
    for k in forms:
        rc = rp = 1.0
        for t, m in zip(tmpls, ms):               # 多个效果按先后顺序叠乘
            rc *= max(RATIO_FLOOR, 1.0 + m * (t.rc[k - 1] - 1.0))
            rp *= max(RATIO_FLOOR, 1.0 + m * (t.rp[k - 1] - 1.0))
        mp = max(1, round(RULER[k - 1][0] * rc * jc))
        power = max(1, round(RULER[k - 1][1] * rp * jp * attr_factor(attr)))
        block = ['%s%s %s %d %d %s %d' % (name, SUFFIX[k - 1], attr, mp, power, realm, len(tmpls))]
        block += [' '.join(scale_effect(t, k, m, jx)) for t, m, jx in zip(tmpls, ms, jxs)]
        blocks.append(block)
    return blocks


def make_virtual_family(rng, forms, idx, must=None):
    return build_family(forms, idx, **sample_family_params(rng, must))


def mixed_sample(fams, n=MAX_FAMS, seed=0):
    """混合抽样(固定种子)：保证①每种效果(含无效果)至少出现一次，②每种属性至少 n//7 个家族，余下的随机补；
    返回按表里原顺序排列的 n 个家族。做法：先给每种效果抽一个家族(属性轮流分配，各属性尽量平均)，再把每种属性补到 n//7 个，最后随机补满 n 个。"""
    rng = random.Random(seed)
    attr_of = lambda f: f.forms[0]['attr']
    eff_of = lambda f: f.name.split('属性', 1)[-1]
    effects = sorted({eff_of(f) for f in fams})
    seq = ATTRS * (len(effects) // len(ATTRS) + 1)
    rng.shuffle(seq)
    pick = []
    for e, a in zip(effects, seq):
        cand = [f for f in fams if eff_of(f) == e and attr_of(f) == a and f not in pick] or [f for f in fams if eff_of(f) == e and f not in pick]
        pick.append(rng.choice(cand))
    for a in ATTRS:
        have = sum(attr_of(f) == a for f in pick)
        mine = [f for f in fams if attr_of(f) == a and f not in pick]
        pick += rng.sample(mine, max(0, min(len(mine), n // len(ATTRS) - have)))
    rest = [f for f in fams if f not in pick]
    pick += rng.sample(rest, max(0, min(len(rest), n - len(pick))))
    return [f for f in fams if f in pick]


if not _SELECT and len(REAL) > MAX_FAMS:    # 没指定子集、表里的家族又超过容量(如 complete.txt)：评测混合抽样的25个家族(每种效果至少1个、每种属性至少3个，余下随机，固定种子)
    REAL = mixed_sample(ALL)


def eval_families():
    """评测用的家族：选中的全部；如果太多(超过环境容量)就固定抽一个子集(训练中的评测池用)。"""
    return REAL if len(REAL) <= MAX_FAMS else mixed_sample(ALL)


def real_text(forms, fams=None):
    """招式表里的家族(默认是被选中的REAL)在给定式上的文件内容。"""
    fams = REAL if fams is None else fams
    if len(fams) * len(forms) > MAX_MOVES:
        raise ValueError('招式太多：%d个家族 x %d式 = %d > 环境容量%d；请用 WXDJ_SELECT / --select 选一个子集(如 无效果、火属性、穿透)' % (
            len(fams), len(forms), len(fams) * len(forms), MAX_MOVES))
    stats = stage_stats(forms)
    lines = ['# stats %s' % ' '.join(fmt(stats[k]) for k in ('hp', 'atk', 'mp', 'rec', 'mult')), '# forms %s' % ' '.join(map(str, forms))]
    for f in fams:
        for k in forms:
            x = f.forms[k - 1]
            lines.append('%s%s %s %d %d %s %d' % (f.name, x['suffix'], x['attr'], x['mp'], x['power'], x['realm'], len(x['effects'])))
            lines += [' '.join(e) for e in x['effects']]
    return '\n'.join(lines) + '\n'


def make_file(rng, idx):
    """一个训练文件(一场对局的招式表)。返回(文本, stats, forms)。"""
    forms = sample_window(rng)
    stats = stage_stats(forms)
    lines = ['# stats %s' % ' '.join(fmt(stats[k]) for k in ('hp', 'atk', 'mp', 'rec', 'mult')), '# forms %s' % ' '.join(map(str, forms))]
    n = rng.randint(10, MAX_FAMS)
    # 每个文件有且只有一个无效果家族(位置随机)，其余家族挂 1/2/3 个效果(比例 EFFECT_COUNT_WEIGHTS)
    zero = rng.randrange(n)
    params = [sample_family_params(rng, n=0 if i == zero else None) for i in range(n)]
    # 每个训练文件必须有静止增伤招：正常抽完以后，如果整个文件都没有，就随机挑一个带效果的家族重抽成带静止增伤的
    static = [t for t in TEMPLATES if t.kind == 'static']
    if static and not any(t.kind == 'static' for p in params for t in p['tmpls']):
        params[rng.choice([i for i in range(n) if i != zero])] = sample_family_params(rng, static[0])
    for i, p in enumerate(params):
        for b in build_family(forms, i, **p):
            lines.extend(b)
    return '\n'.join(lines) + '\n', stats, forms


def generate(out_dir, count, seed):
    if not TEMPLATES:
        raise SystemExit('招式表里没有无属性的单效果家族，没法生成训练用的招式：%s' % ACT)
    os.makedirs(out_dir, exist_ok=True)
    rng = random.Random(seed)
    paths = []
    for i in range(count):
        text, stats, forms = make_file(rng, i)
        p = os.path.join(out_dir, 'f%05d.txt' % i)
        open(p, 'w', encoding='utf-8', newline='\n').write(text)
        paths.append(p)
    return paths


def read_stats(path):
    first = open(path, encoding='utf-8').readline().split()
    return [float(x) for x in first[2:7]]


def read_top(path):
    """文件的最高式(窗口里最大的式)，来自第二行 "# forms 16 17 18 19 20"。"""
    with open(path, encoding='utf-8') as f:
        f.readline()
        return max(int(x) for x in f.readline().split()[2:])


def write_real(out_dir, forms_list, fams=None):
    os.makedirs(out_dir, exist_ok=True)
    paths = []
    for forms in forms_list:
        p = os.path.join(out_dir, 'real_%s.txt' % '_'.join(map(str, forms)))
        open(p, 'w', encoding='utf-8', newline='\n').write(real_text(forms, fams))
        paths.append(p)
    return paths


def write_real_windows(out_dir, tops, fams=None):
    """以tops里每个式为最高式的5式窗口，各写一个真实家族的文件(评测用)。"""
    return write_real(out_dir, [window_forms(t) for t in tops], fams)


if __name__ == '__main__':
    import sys
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    ps = generate(os.path.join(HERE, 'pool_test'), n, 1)
    print(len(ps), 'files; sample:')
    print(open(ps[0], encoding='utf-8').read()[:800])
