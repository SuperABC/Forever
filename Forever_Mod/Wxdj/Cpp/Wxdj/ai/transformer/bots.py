"""脚本对手：用来做训练陪练和评测基准。都按环境里的数组向量化。

OLD    游戏里原来的敌方AI(3/4概率出单招或积攒/炸弹/跳过，1/4概率出双招，招式随机)
FAMILY 只用一个招式家族(随机的式)，每回合出单招
POWER  每回合出单招，在威力最高的3个招式里随机
TURTLE 一半的回合积攒/放炸弹，其余出随机单招
"""
import numpy as np

OLD, FAMILY, POWER, TURTLE = range(4)
NAMES = ['随机AI', '单家族', '最高威力', '积攒炸弹']


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

    # TURTLE
    r = rng.random(n)
    tt_cls = np.where(r < 0.3, 1, np.where(r < 0.45, 2, 3))

    t = bot_type
    out[:, 0] = np.where(t == OLD, old_cls, np.where(t == TURTLE, tt_cls, 3))
    out[:, 1] = np.where(t == OLD, old_m1, np.where(t == FAMILY, fam_m1, np.where(t == POWER, pw_m1, m1)))
    out[:, 2] = m2
    return out


def pick_family(env, gids, seat, rng):
    """每个游戏随机选一个家族编号(取自这一方的招式)。"""
    fam = env.fam[gids, seat]
    valid = fam >= 0
    idx = _random_valid(rng, valid | (~valid.any(1))[:, None] & (fam != -2))
    return fam[np.arange(len(gids)), idx]
