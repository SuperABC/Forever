"""PvE训练用的随机配置：双方的五行属性、基础暴击、特性(装备/秘诀/丹药)。

PvE模式的训练环境(wxenv.set_mode(True))每盘对局可以带一份配置，字段见 wxenv.Env.reset 的 pve 参数：
    elem    (甲, 乙)  五行属性掩码，第n位=属性值n(金1 木2 水3 火4 土5)，0=无属性
    crit    (暴击率, 暴击伤害)  双方共用的基础暴击(控制器的SetCritBase)；游戏里PvE默认 5% / +50%
    traits  (甲, 乙)  特性文本，每行一条，格式同 battle/modifier.h 的 ParseModifier(也就是网页"特性调试"面板的写法)

配置分布要覆盖：
  - 一部分对局什么都没有(和PvP一样)，保证PvE模型不会忘了怎么打PvP规则；
  - 双方不对等(一方有装备一方没有)，因为PvE的敌人和玩家本来就不对等；
  - 各种五行组合(含五个齐全)、抗性被调高/调低、同属性倍率被改、增伤/易伤/暴击/闪避/护盾等特性。
特性的绝对数值(立即加的护盾)按这一方的武力上限缩放，保证不同阶位都合理。
"""
import numpy as np

ELEM_BITS = [1 << a for a in range(1, 6)]     # 金木水火土
ELEM_NAMES = ['metal', 'wood', 'water', 'fire', 'earth']


def fmt(v, nd=3):
    return ('%.*f' % (nd, v)).rstrip('0').rstrip('.') or '0'


def random_elements(rng):
    """每方的五行属性：35%没有，其余按个数随机(含15%五个齐全)。"""
    r = rng.random()
    if r < 0.35:
        return 0
    if r < 0.50:
        return sum(ELEM_BITS)
    k = int(rng.choice([1, 1, 1, 2, 2, 3, 4]))
    return sum(ELEM_BITS[i] for i in rng.choice(5, size=k, replace=False))


def _attr(rng, elems):
    """特性作用的属性：一个具体元素(偏向自己拥有的)，或者不限制(None)。"""
    if rng.random() < 0.35:
        return None
    owned = [i for i in range(5) if elems & ELEM_BITS[i]]
    if owned and rng.random() < 0.6:
        return ELEM_NAMES[int(rng.choice(owned))]
    return ELEM_NAMES[int(rng.integers(0, 5))]


KIND_P = np.array([8, 5, 8, 6, 4, 4, 3, 3, 3, 3, 3, 1, 4, 2, 2, 2, 2, 6], dtype=np.float64)
KIND_P /= KIND_P.sum()


def random_trait(rng, elems, atk):
    """一条随机特性(文本)。atk=这一方的武力上限，用来缩放绝对数值。"""
    kind = rng.choice(['dmg_dealt', 'dmg_taken', 'resist', 'elem_power', 'base_power', 'crit_rate', 'crit_dmg', 'dodge', 'hit', 'cost',
                       'rebound', 'immune', 'mp_round', 'hp_round', 'atk_round', 'deal_mp', 'take_mp', 'now_shield'],
                      p=KIND_P)
    a = _attr(rng, elems)
    attr = (' attr=' + a) if a else ''
    dur = '' if rng.random() < 0.6 else ' dur=%d' % int(rng.integers(2, 8))
    u = rng.random
    if kind == 'dmg_dealt':
        return 'mod dmg_dealt %s%s%s' % (fmt(0.1 + 1.9 * u() ** 1.5), attr, dur)
    if kind == 'dmg_taken':                                    # 易伤只增不减
        return 'mod dmg_taken %s%s%s' % (fmt(0.1 + 0.9 * u()), attr, dur)
    if kind == 'resist':
        a = a or ELEM_NAMES[int(rng.integers(0, 5))]           # 抗性必须指定元素
        v = rng.uniform(-0.8, 1.5)
        return 'mod resist %s attr=%s%s' % (fmt(v), a, dur)
    if kind == 'elem_power':
        return 'mod elem_power %s%s' % (fmt(rng.uniform(-1.0, 2.0)), dur)
    if kind == 'base_power':
        return 'mod base_power %s%%%s%s' % (fmt(10 + 90 * u(), 1), attr, dur)
    if kind == 'crit_rate':
        return 'mod crit_rate %s%s' % (fmt(0.05 + 0.45 * u()), dur)
    if kind == 'crit_dmg':
        return 'mod crit_dmg %s%s' % (fmt(0.2 + 1.8 * u()), dur)
    if kind == 'dodge':
        return 'mod dodge %s%s' % (fmt(0.1 + 0.4 * u()), dur)
    if kind == 'hit':
        return 'mod hit %s%s' % (fmt(0.1 + 0.4 * u()), dur)
    if kind == 'cost':
        return 'mod cost %s%s%s' % (fmt(rng.uniform(-0.3, 0.3)), attr, dur)
    if kind == 'rebound':
        return 'mod rebound %s%s%s' % (fmt(0.05 + 0.25 * u()), attr, dur)
    if kind == 'immune':
        a = a or ELEM_NAMES[int(rng.integers(0, 5))]
        return 'mod immune 1 attr=%s%s' % (a, dur)
    if kind == 'mp_round':
        return 'mod mp_round %s%% timing=%s%s' % (fmt(0.5 + 3.5 * u(), 2), rng.choice(['start', 'end']), dur)
    if kind == 'hp_round':                                     # 正=回复；负的量很小，不会直接把人扣死
        return 'mod hp_round %s%% timing=end%s' % (fmt(rng.uniform(-0.5, 2.0), 2), dur)
    if kind == 'atk_round':
        return 'mod atk_round %s%% timing=end%s' % (fmt(rng.uniform(-0.5, 2.0), 2), dur)
    if kind == 'deal_mp':
        return 'mod deal_mp %s%%%s%s' % (fmt(1 + 9 * u(), 1), attr, dur)
    if kind == 'take_mp':
        return 'mod take_mp %s%%%s%s' % (fmt(1 + 9 * u(), 1), attr, dur)
    # now_shield：立即加一块盾，强度=武力上限的10%~60%；盾属性随机(一个元素/五个齐全/无属性)
    r = u()
    sh = (' attr=' + ('all' if r < 0.15 else ELEM_NAMES[int(rng.integers(0, 5))])) if r < 0.85 else ''
    d = int(rng.integers(2, 7)) if u() < 0.8 else -1
    return 'mod now_shield %d%s dur=%d' % (max(1, int(atk * (0.1 + 0.5 * u()))), sh, d)


def random_build(rng, atk, level):
    """一方的配置：(五行属性掩码, 特性文本)。level 0=什么都没有 1=轻 2=重。"""
    if level == 0:
        return 0, ''
    elems = random_elements(rng)
    n = int(rng.choice([0, 1, 1, 2, 2, 3])) if level == 1 else int(rng.choice([2, 3, 4, 5, 6]))
    lines = [random_trait(rng, elems, atk) for _ in range(n)]
    return elems, '\n'.join(lines)


def sample_config(rng, atk_a, atk_b, mirror=False):
    """一盘PvE对局的配置。mirror=True时双方完全一样(评测要公平)。"""
    r = rng.random()
    if r < 0.15:                                               # 和PvP一样：无属性、无暴击、无特性
        return dict(elem=(0, 0), crit=(0.0, 0.5), traits=('', ''))
    la = int(rng.choice([0, 1, 1, 2]))
    lb = la if mirror else int(rng.choice([0, 1, 1, 2]))
    ea, ta = random_build(rng, atk_a, la)
    eb, tb = (ea, ta) if mirror else random_build(rng, atk_b, lb)
    crit = (0.0, 0.5) if rng.random() < 0.3 else ((0.05, 0.5) if rng.random() < 0.6 else (float(rng.choice([0.1, 0.2, 0.3])), float(rng.choice([0.5, 1.0]))))
    return dict(elem=(ea, eb), crit=crit, traits=(ta, tb))


# ---------------- 非对称PvE：玩家(席位0，招式多、加成多) 对 敌人(席位1，三维是玩家的十倍百倍、招式少、效果小、不能积攒炸弹) ----------------
_MAXPOWER = {}


def file_max_power(path):
    """招式文件里最强的单招基础威力(缓存)。招式行格式：名字 属性 消耗 威力 阶位 效果数。"""
    v = _MAXPOWER.get(path)
    if v is None:
        v = 1
        for line in open(path, encoding='utf-8'):
            t = line.split(' ')
            if len(t) == 6 and t[4].count('-') == 1 and t[3].isdigit():
                v = max(v, int(t[3]))
        _MAXPOWER[path] = v
    return v


def boost_traits(rng, elems, target):
    """给玩家的"增伤类"特性，让这些乘区的乘积约等于 target(>1)：把 ln(target) 随机分给 增伤/基础威力/暴击 三个乘区，每个乘区再拆成1~3条特性。
    PvE里玩家要靠堆叠加成才打得动十倍百倍血量的敌人，所以单条数值不设上限。返回特性行的列表。"""
    if target <= 1.3:
        return []
    zones = [z for z in ('dmg_dealt', 'base_power', 'crit') if rng.random() < 0.7] or ['dmg_dealt']
    share = rng.dirichlet(np.ones(len(zones)))
    lines = []
    for z, f in zip(zones, share):
        mult = float(np.exp(np.log(target) * f))          # 这个乘区要达到的倍数
        if mult <= 1.02:
            continue
        if z == 'crit':                                   # 期望倍率 = 1 + 暴击率 x 暴击伤害
            lines.append('mod crit_rate %s' % fmt(rng.uniform(0.3, 0.9)))
            lines.append('mod crit_dmg %s' % fmt((mult - 1.0) / 0.6))
            continue
        k = int(rng.integers(1, 4))
        parts = rng.dirichlet(np.ones(k))
        a = _attr(rng, elems) if rng.random() < 0.3 else None
        attr = (' attr=' + a) if a else ''
        for part in parts:
            amount = (mult - 1.0) * part                  # 同乘区内加算
            if z == 'dmg_dealt':
                lines.append('mod dmg_dealt %s%s' % (fmt(amount, 3), attr))
            else:
                lines.append('mod base_power %s%%%s' % (fmt(amount * 100.0, 1), attr))
    return lines


def sample_asym(rng, player_path, player_atk, enemy_atk, enemy_life, k):
    """一盘非对称PvE的配置。
    k=敌人相对标准属性的规模倍数(10~100)，enemy_life=敌人的健康+武力，player_path=玩家的招式文件(用来估计玩家单回合能打多少)。
    玩家：重配置(level 2)的随机特性，再加增伤类特性(boost_traits)，让玩家"堆满加成后"打穿敌人需要的回合数大约是 4~25 回合；
    敌人：轻配置(0~3条特性)，招式在文件里已经是少而弱的；敌人不能积攒炸弹(noacc)。"""
    ea, ta = random_build(rng, player_atk, 2)
    eb, tb = random_build(rng, enemy_atk, int(rng.choice([0, 1, 1])))
    turns = float(np.exp(rng.uniform(np.log(4.0), np.log(25.0))))
    need = enemy_life / max(1.0, file_max_power(player_path) * 0.6) / turns      # 0.6：不是每回合都能打满、有时要躲有时要防
    boost = boost_traits(rng, ea, need)
    if boost:
        ta = (ta + '\n' if ta else '') + '\n'.join(boost)
    crit = (0.05, 0.5) if rng.random() < 0.7 else (0.0, 0.5)
    return dict(elem=(ea, eb), crit=crit, traits=(ta, tb), noacc=(0, 1), need=need, k=k)


if __name__ == '__main__':
    rng = np.random.default_rng(1)
    for _ in range(4):
        print(sample_config(rng, 38900, 38900))
