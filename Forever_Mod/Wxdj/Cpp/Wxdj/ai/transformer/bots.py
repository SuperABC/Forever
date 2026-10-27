"""脚本对手：用来做训练陪练和评测基准。都按环境里的数组向量化。

OLD    游戏里原来的敌方AI(3/4概率出单招或积攒/炸弹/跳过，1/4概率出双招，招式随机)
FAMILY 只用一个招式家族(随机的式)，每回合出单招
POWER  每回合出单招，在威力最高的3个招式里随机
ROUTINE 固定套路：照着人类发现的一套固定打法出招(见下面的ROUTINES)，每盘随机选一个套路。
        以前这个位置是"随机积攒/放炸弹"的乌龟机器人——积攒炸弹本身是很差的循环，对它的胜率没有意义，所以换成真正有威胁的套路。

加新套路：写一个 fn(env, gids, seat, st, rng) -> (n,3)数组[cls, m1, m2]，在 ROUTINES 里加一项 (名称, fn) 就行。
st是这些游戏里该席位看到的局面特征(n,SD)，用下面的 S_* 下标取值；env.moves/env.form/env.fam 是该席位的招式特征。
套路只能用对局里看得到的信息(局面特征、招式特征)，不能偷看环境内部状态，和人一样。
"""
import numpy as np

OLD, FAMILY, POWER, ROUTINE = range(4)
NAMES = ['随机AI', '单家族', '最高威力', '固定套路']

# 局面特征(见env.cpp的FillState)：先自己再对手，各18维；然后4维场地、1维回合数
S_MP, S_BOMB = 2, 3             # 自己的真气/炸弹累积量(都除以真气上限)
S_OPP_BOMB = 18 + 3             # 对手的炸弹累积量
S_OPP_LOCKED = 18 + 9           # 对手这一回合被禁锢(1=是)
S_ROUND = 40                    # 回合数/40
# 招式特征(见features.h的MoveFeatures)
M_COST, M_LOCK_PROB, M_HAS_LOCK = 0, 48, 70   # 消耗/真气上限，禁锢概率，是否带禁锢


def _random_valid(rng, valid):
    """valid(n,M)布尔，每行随机取一个True的下标"""
    scores = rng.random(valid.shape)
    return np.argmax(np.where(valid, scores, -1.0), axis=1)


def actions(env, gids, seat, bot_type, bot_fam, rng):
    """返回(n,3)：cls, m1, m2。gids是游戏下标，bot_type/bot_fam是这些游戏的对应数组。"""
    n = len(gids)
    form = env.form[gids, seat]            # (n,M)
    fam = env.fam[gids, seat]
    valid = form != -2
    nm = valid.sum(1)
    out = np.zeros((n, 3), dtype=np.int32)
    m1 = _random_valid(rng, valid)
    f1 = form[np.arange(n), m1][:, None]
    same = valid & ((form == f1) | (f1 < 0) | (form < 0))
    m2 = _random_valid(rng, same)

    # OLD
    u = rng.random(n)
    k = (rng.random(n) * (nm + 6)).astype(np.int64)
    old_cls = np.where(k < nm, 3, np.where(k < nm + 3, 1, np.where(k == nm + 3, 2, 0)))
    old_cls = np.where(u < 0.75, old_cls, 4)
    old_m1 = np.where(u < 0.75, np.minimum(k, nm - 1), m1)

    # FAMILY
    fam_valid = valid & (fam == bot_fam[:, None])
    fam_valid = np.where(fam_valid.any(1)[:, None], fam_valid, valid)
    fam_m1 = _random_valid(rng, fam_valid)

    # POWER：特征第2维是威力/自身武力上限
    power = np.where(valid, env.moves[gids, seat, :, 2], -1e9)
    top = np.argsort(-power, axis=1)[:, :3]
    pick = rng.integers(0, 3, n)
    pw_m1 = top[np.arange(n), np.minimum(pick, np.maximum(nm - 1, 0)).clip(0, 2)]

    t = bot_type
    out[:, 0] = np.where(t == OLD, old_cls, 3)
    out[:, 1] = np.where(t == OLD, old_m1, np.where(t == FAMILY, fam_m1, np.where(t == POWER, pw_m1, m1)))
    out[:, 2] = m2

    # ROUTINE：bot_fam兼作套路编号(取模)，所以每盘随机到哪个套路由调用方的随机family决定
    rs = np.where(t == ROUTINE)[0]
    if len(rs):
        st = env.obs(gids[rs])[0][:, seat]
        which = bot_fam[rs] % len(ROUTINES)
        for k, (_, fn) in enumerate(ROUTINES):
            sub = np.where(which == k)[0]
            if len(sub):
                out[rs[sub]] = fn(env, gids[rs[sub]], seat, st[sub], rng)
    return out


def lock_bomb(env, gids, seat, st, rng):
    """人类套路"禁锢炸弹"：先攒两次炸弹，然后一直放带禁锢的招，直到对手被禁锢住，下一回合立刻释放炸弹(对手被禁锢=没出招，炸弹打0.4B)。
    炸弹放完从头再来：重新攒两次，再继续放禁锢招。放不起禁锢招就跳过回气。
    "已经攒了几次"记在env上(每个游戏一个计数，第1回合清零)，因为局面特征里看不出攒了几次。"""
    n = len(gids)
    out = np.zeros((n, 3), dtype=np.int32)
    gids = np.asarray(gids)
    mem = getattr(env, 'routine_acc', None)
    if mem is None:
        mem = env.routine_acc = np.zeros((env.n, 2), dtype=np.int32)
    rnd = np.rint(st[:, S_ROUND] * 40)
    mem[gids[rnd <= 1], seat] = 0
    acc = mem[gids, seat]
    mv = env.moves[gids, seat]
    valid = env.form[gids, seat] != -2
    cand = valid & (mv[:, :, M_HAS_LOCK] > 0.5) & (mv[:, :, M_COST] <= st[:, S_MP][:, None])
    score = np.where(cand, rng.random(cand.shape) * (mv[:, :, M_LOCK_PROB] + 1e-3), -1.0)
    out[:, 1] = np.argmax(score, axis=1)
    can_lock = cand.any(1)
    prepare = acc < 2
    boom = ~prepare & (st[:, S_OPP_LOCKED] > 0.5) & (st[:, S_BOMB] > 0)
    out[:, 0] = np.where(prepare, 1, np.where(boom, 2, np.where(can_lock, 3, 0)))
    # 禁锢不影响积攒；放了炸弹就从头开始
    mem[gids, seat] = np.where(prepare, acc + 1, np.where(boom, 0, acc))
    return out


# 已知的人类套路。新发现的套路加到这里，训练和评测会自动覆盖。
ROUTINES = [('禁锢炸弹', lock_bomb)]


def pick_family(env, gids, seat, rng):
    """每个游戏随机选一个家族编号(取自这一方的招式)。"""
    fam = env.fam[gids, seat]
    valid = fam >= 0
    idx = _random_valid(rng, valid | (~valid.any(1))[:, None] & (fam != -2))
    return fam[np.arange(len(gids)), idx]


# ---- 套路开局：只照着脚本打前几回合，之后把这一方交给网络自博弈(见train.py的opp_type==3) ----
M_HAS_REDUCE, M_REDUCE_RATIO, M_ATTR_WATER = 67, 27, 9     # 带减伤，减伤比例(对无属性)，属性是水
M_HAS_SHIELD, M_SHIELD_RATIO = 77, 78                      # 带护盾，盾量比例
M_HAS_STATIC, M_STATIC_RATIO = 81, 82                      # 带static(对静止敌人增伤)，增伤比例
M_HAS_REBOUND, M_REBOUND_RATIO, M_ATTR_METAL = 68, 34, 7   # 带反弹，反弹比例(对无属性)，属性是金


def _best(env, gids, seat, st, flag, score_col):
    """带flag效果、付得起的招式里score_col最大的一个。返回(下标, 有没有)。"""
    mv = env.moves[gids, seat]
    cand = (env.form[gids, seat] != -2) & (mv[:, :, flag] > 0.5) & (mv[:, :, M_COST] <= st[:, S_MP][:, None])
    return np.argmax(np.where(cand, mv[:, :, score_col], -1.0), axis=1), cand.any(1)


def shield_golden(env, gids, seat, st, rnd, rng):
    """第1回合出护盾(盾量最大的那个)；对手第1回合攒了炸弹，就一直出海龙金身(水属性的减伤招，减伤最大的那个)直到对手把炸弹放掉；
    对手第1回合没攒炸弹，或者炸弹已经放掉，脚本结束，由网络自博弈接管(返回的scripted=False)。
    招式表里没有对应的招(训练池是随机子集)或付不起时，退而求其次：出付得起的威力最大的单招，再不行就跳过。
    返回(动作(n,3), scripted(n,))。"""
    n = len(gids)
    out = np.zeros((n, 3), dtype=np.int32)
    gids = np.asarray(gids)
    on = getattr(env, 'open_on', None)
    if on is None:
        on = env.open_on = np.zeros((env.n, 2), dtype=bool)
    if rnd <= 1:
        on[gids, seat] = True
    else:   # 对手手里没有炸弹了(没攒，或者放掉了)就交给网络；交出去以后不再接回来
        on[gids, seat] &= st[:, S_OPP_BOMB] > 0
    scripted = on[gids, seat].copy()
    mv = env.moves[gids, seat]
    afford = (env.form[gids, seat] != -2) & (mv[:, :, M_COST] <= st[:, S_MP][:, None])
    fb = np.argmax(np.where(afford, mv[:, :, 2], -1e9), axis=1)    # 退路：威力最高的付得起的招
    sh, has_sh = _best(env, gids, seat, st, M_HAS_SHIELD, M_SHIELD_RATIO)
    water = afford & (mv[:, :, M_HAS_REDUCE] > 0.5) & (mv[:, :, M_ATTR_WATER] > 0.5)
    lg = np.argmax(np.where(water, mv[:, :, M_REDUCE_RATIO], -1.0), axis=1)
    pick = sh if rnd <= 1 else lg
    ok = has_sh if rnd <= 1 else water.any(1)
    out[:, 0] = np.where(ok | afford.any(1), 3, 0)
    out[:, 1] = np.where(ok, pick, fb)
    return out, scripted


def static_open(env, gids, seat, st, rnd, rng):
    """一直出天动万象(带static的招，付得起的里static比例最大的那个)，直到对手手里有了炸弹(积攒了)；
    然后一直出金钟罩(金属属性的反弹招，反弹比例最大的那个)，直到对手把炸弹放掉；之后由网络自博弈接管(scripted=False)，不再接回来。
    目的是抑制"开局就攒炸弹、放炸弹"：天动万象克制攒炸弹，金钟罩挡住随后的炸弹。
    对手一直不攒炸弹时，天动万象最多出STATIC_MAX_ROUNDS回合就交出去，避免整局都在脚本里。
    招式表里没有带static的招时脚本整个不生效；某回合付不起想出的招就退而求其次(金钟罩换任意反弹招，再换威力最大的付得起的单招，都付不起就跳过)。
    返回(动作(n,3), scripted(n,))。"""
    n = len(gids)
    gids = np.asarray(gids)
    out = np.zeros((n, 3), dtype=np.int32)
    ph = getattr(env, 'static_ph', None)       # 0=没在脚本里(没开始或已交出) 1=出天动万象 2=出金钟罩
    if ph is None:
        ph = env.static_ph = np.zeros((env.n, 2), dtype=np.int8)
    mv = env.moves[gids, seat]
    afford = (env.form[gids, seat] != -2) & (mv[:, :, M_COST] <= st[:, S_MP][:, None])
    st_pick, has_st = _best(env, gids, seat, st, M_HAS_STATIC, M_STATIC_RATIO)
    if rnd <= 1:
        ph[gids, seat] = np.where(has_st, 1, 0)
    p = ph[gids, seat].copy()
    opp_bomb = st[:, S_OPP_BOMB] > 0
    p = np.where((p == 1) & (opp_bomb | (rnd > STATIC_MAX_ROUNDS)), np.where(opp_bomb, 2, 0), p)
    p = np.where((p == 2) & ~opp_bomb, 0, p)
    ph[gids, seat] = p
    reb = afford & (mv[:, :, M_HAS_REBOUND] > 0.5)
    score = mv[:, :, M_REBOUND_RATIO] + 10.0 * (mv[:, :, M_ATTR_METAL] > 0.5)     # 金属的反弹招(金钟罩)优先，其次反弹比例最大的
    pick_reb = np.argmax(np.where(reb, score, -1e9), axis=1)
    best = np.argmax(np.where(afford, mv[:, :, 2], -1e9), axis=1)
    static_ok = has_st
    use_reb = reb.any(1)
    pick = np.where(p == 1, np.where(static_ok, st_pick, best), np.where(use_reb, pick_reb, best))
    out[:, 0] = np.where((p == 1) & static_ok | afford.any(1), 3, 0)
    out[:, 1] = pick
    return out, p > 0


STATIC_MAX_ROUNDS = 10


def accumulate_loop(env, gids, seat, st, rnd, rng):
    """循环：积攒2回合 -> 海龙金身(水属性减伤最大的招) -> 释放炸弹 -> 再从头积攒。整局都按这个套路出，不交给网络。
    海龙金身付不起/没有时退而求其次出付得起的威力最大的单招，都付不起就跳过；释放炸弹时手里没炸弹就直接重新积攒。
    返回(动作(n,3), scripted(n,))。"""
    n = len(gids)
    gids = np.asarray(gids)
    out = np.zeros((n, 3), dtype=np.int32)
    cy = getattr(env, 'loop_step', None)        # 循环里的第几步：0,1=积攒 2=海龙金身 3=放炸弹
    if cy is None:
        cy = env.loop_step = np.zeros((env.n, 2), dtype=np.int8)
    if rnd <= 1:
        cy[gids, seat] = 0
    c = cy[gids, seat].copy()
    c = np.where((c == 3) & (st[:, S_BOMB] <= 0), 0, c)    # 没炸弹可放，重新积攒
    mv = env.moves[gids, seat]
    afford = (env.form[gids, seat] != -2) & (mv[:, :, M_COST] <= st[:, S_MP][:, None])
    water = afford & (mv[:, :, M_HAS_REDUCE] > 0.5) & (mv[:, :, M_ATTR_WATER] > 0.5)
    lg = np.argmax(np.where(water, mv[:, :, M_REDUCE_RATIO], -1.0), axis=1)
    fb = np.argmax(np.where(afford, mv[:, :, 2], -1e9), axis=1)
    out[:, 0] = np.where(c <= 1, 1, np.where(c == 3, 2, np.where(afford.any(1), 3, 0)))
    out[:, 1] = np.where(c == 2, np.where(water.any(1), lg, fb), 0)
    cy[gids, seat] = (c + 1) % 4
    return out, np.ones(n, dtype=bool)


# (名称, 函数 fn(env, gids, seat, st, 回合数, rng) -> (动作, scripted), 占全部训练对局的比例)
OPENINGS = [('护盾金身', shield_golden, 0.08), ('天动万象+金钟罩', static_open, 0.15), ('积攒+海龙金身+放炸弹循环', accumulate_loop, 0.15)]


def opening(env, gids, seat, which, rnd, rng):
    """套路开局的出招：which是每个游戏用哪个开局(OPENINGS下标)，rnd是当前回合数(从1开始)。
    返回(动作(n,3), scripted(n,))：scripted=False的游戏这一回合脚本已经结束，调用方该用网络的动作，也该让网络学这一步。"""
    gids = np.asarray(gids)
    st = env.obs(gids)[0][:, seat]
    out = np.zeros((len(gids), 3), dtype=np.int32)
    scr = np.zeros(len(gids), dtype=bool)
    for k, (_, fn, _w) in enumerate(OPENINGS):
        sub = np.where(which == k)[0]
        if len(sub):
            out[sub], scr[sub] = fn(env, gids[sub], seat, st[sub], rnd, rng)
    return out, scr


# ---- PvE：玩家侧的陪练(训练敌人模型用)。玩家有很多招式和加成，这里放几种"人会怎么玩"的脚本 ----
M_HAS_INCREASE, M_FORM = 75, 80       # 带increase(连招增伤)，式号/20
S_SHIELD = 41                          # 局面特征：自己最大那块盾/自己武力上限
PVE_PLAYER_NAMES = ['随机AI', '最高威力', '连招爬升', '护盾龟', '禁锢炸弹']


def _afford(env, gids, seat, st):
    mv = env.moves[gids, seat]
    return mv, (env.form[gids, seat] != -2) & (mv[:, :, M_COST] <= st[:, S_MP][:, None])


def _skip_or(out, ok, pick):
    out[:, 0] = np.where(ok, 3, 0)
    out[:, 1] = np.where(ok, pick, 0)
    return out


def climber(env, gids, seat, st, rng):
    """连招爬升：专挑带increase的招，接着上一回合的同家族往上一式出(第k式、k+1式、k+2式……)，断了(付不起)就从最低式重新开始；
    没有increase招式时出付得起的威力最大的单招。上一回合出的(家族,式号)记在env上。"""
    n = len(gids)
    gids = np.asarray(gids)
    out = np.zeros((n, 3), dtype=np.int32)
    mem = getattr(env, 'climb_mem', None)
    if mem is None:
        mem = env.climb_mem = np.full((env.n, 2, 2), -1, dtype=np.int64)   # [游戏, 席位, (家族, 式号)]
    rnd = np.rint(st[:, S_ROUND] * 40)
    mem[gids[rnd <= 1], seat] = -1
    mv, afford = _afford(env, gids, seat, st)
    fam = env.fam[gids, seat]
    form = np.rint(mv[:, :, M_FORM] * 20).astype(np.int64)
    inc = afford & (mv[:, :, M_HAS_INCREASE] > 0.5)
    last_fam, last_form = mem[gids, seat, 0], mem[gids, seat, 1]
    nxt = inc & (fam == last_fam[:, None]) & (form == (last_form + 1)[:, None])
    start = np.argmin(np.where(inc, form + rng.random(form.shape) * 0.5, 1e9), axis=1)    # 从最低式重新开始(同式随机)
    pick_inc = np.where(nxt.any(1), np.argmax(nxt, axis=1), start)
    best = np.argmax(np.where(afford, mv[:, :, 2], -1e9), axis=1)
    use_inc = inc.any(1)
    pick = np.where(use_inc, pick_inc, best)
    ok = afford.any(1)
    mem[gids, seat, 0] = np.where(ok, fam[np.arange(n), pick], -1)
    mem[gids, seat, 1] = np.where(ok & use_inc, form[np.arange(n), pick], -1)
    return _skip_or(out, ok, pick)


def shield_turtle(env, gids, seat, st, rng):
    """护盾龟：自己身上没盾(或盾很薄)时先开一块盾(盾量最大的那个)，有盾了就出付得起的威力最大的单招。"""
    n = len(gids)
    gids = np.asarray(gids)
    out = np.zeros((n, 3), dtype=np.int32)
    mv, afford = _afford(env, gids, seat, st)
    sh = afford & (mv[:, :, M_HAS_SHIELD] > 0.5)
    pick_sh = np.argmax(np.where(sh, mv[:, :, M_SHIELD_RATIO], -1.0), axis=1)
    best = np.argmax(np.where(afford, mv[:, :, 2], -1e9), axis=1)
    want = sh.any(1) & (st[:, S_SHIELD] < 0.02)
    return _skip_or(out, afford.any(1), np.where(want, pick_sh, best))


def pve_player_actions(env, gids, seat, kind, rng):
    """kind(n,)：PVE_PLAYER_NAMES 的下标。返回(n,3)=[cls,m1,m2]。"""
    gids = np.asarray(gids)
    kind = np.asarray(kind)
    out = np.zeros((len(gids), 3), dtype=np.int32)
    fam = pick_family(env, gids, seat, rng)
    for k in range(len(PVE_PLAYER_NAMES)):
        sub = np.where(kind == k)[0]
        if not len(sub):
            continue
        g = gids[sub]
        if k in (0, 1):
            out[sub] = actions(env, g, seat, np.full(len(g), OLD if k == 0 else POWER), fam[sub], rng)
            continue
        st = env.obs(g)[0][:, seat]
        fn = {2: climber, 3: shield_turtle, 4: lock_bomb}[k]
        out[sub] = fn(env, g, seat, st, rng)
    return out
