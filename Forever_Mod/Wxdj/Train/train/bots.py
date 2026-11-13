"""陪练开局：训练时陪练(对手席位)有一定概率按随机开局出前几回合的招，之后交给自博弈/快照，让模型学会应对任意类型的开局。
训练里没有任何脚本对手、基准机器人、人类套路：模型只和自己、自己的历史快照打，套路应该由模型自己在对局里发现；这里只有随机的开局。
"""
import numpy as np

STATIC_IDX = 81     # 招式特征里"带静止增伤效果"的标志位(features.h 的 o[81])

# 开局种类(每盘开局前抽一次)
OPEN_NONE, OPEN_ACC_BOMB, OPEN_STATIC, OPEN_FAMILY, OPEN_DOUBLE = 0, 1, 2, 3, 4


def _random_valid(rng, valid):
    """valid(n,M)布尔，每行随机取一个True的下标"""
    scores = rng.random(valid.shape)
    return np.argmax(np.where(valid, scores, -1.0), axis=1)


def _fam_weight(fam, mask):
    """(n,M)：mask内每个招式的权重=1/所在家族在mask内的招式数，所以每个家族的总权重是1(fam<0的招式各算一个家族)"""
    same = (fam[:, :, None] == fam[:, None, :]) & mask[:, None, :] & (fam[:, :, None] >= 0)
    cnt = np.maximum(same.sum(2), 1)
    return np.where(mask, 1.0 / cnt, 0.0)


def _family_pick(rng, w):
    """按权重w(n,M)每行抽一个下标"""
    r = rng.random(len(w)) * w.sum(1)
    return np.minimum((np.cumsum(w, 1) < r[:, None]).sum(1), w.shape[1] - 1)


def opening_actions(env, gids, seat, rng, kind, turn):
    """返回(n,3)：cls, m1, m2。gids是游戏下标，kind是(n,)的开局种类，turn是第几回合(0或1)。
    开局种类：
      OPEN_ACC_BOMB：第1回合积攒，第2回合放炸弹，第3回合起交给自博弈/快照
      OPEN_STATIC  ：第1回合出静止增伤招(在静止增伤家族里等概率选家族，再在该家族的各式里等概率)，第2回合起交给自博弈/快照；
                     这个席位没有静止增伤招就按 OPEN_FAMILY 处理
      OPEN_FAMILY  ：第1回合出单招(每个家族一个选项等概率，再在该家族的各式里等概率)，第2回合起交给自博弈/快照
      OPEN_DOUBLE  ：第1回合随机双招(第二招必须和第一招同式，招式名里没有"第X式"的不受限制)，第2回合起交给自博弈/快照
    只有 OPEN_ACC_BOMB 在 turn=1 有动作；其余种类 turn=1 不该被调用。"""
    n = len(gids)
    out = np.zeros((n, 3), dtype=np.int32)
    if turn == 1:
        out[:, 0] = 2                                           # 炸弹
        return out

    form = env.form[gids, seat]            # (n,M)
    valid = form != -2
    static = valid & (env.moves[gids, seat, :, STATIC_IDX] > 0.5)
    fam = env.fam[gids, seat]
    ar = np.arange(n)

    # 单招：按家族等概率(每个家族一个选项，选中后在该家族的各式里等概率)
    pick_fam = _family_pick(rng, _fam_weight(fam, valid))
    pick_static = _family_pick(rng, _fam_weight(fam, static))
    single = np.where((kind == OPEN_STATIC) & static.any(1), pick_static, pick_fam)

    # 双招
    m1 = _random_valid(rng, valid)
    f1 = form[ar, m1][:, None]
    same = valid & ((form == f1) | (f1 < 0) | (form < 0))
    m2 = _random_valid(rng, same)

    is_acc = kind == OPEN_ACC_BOMB
    is_double = kind == OPEN_DOUBLE
    out[:, 0] = np.where(is_acc, 1, np.where(is_double, 4, 3))
    out[:, 1] = np.where(is_acc, 0, np.where(is_double, m1, single))
    out[:, 2] = np.where(is_double, m2, 0)
    return out


def sample_open_kinds(rng, n, open_single, open_double):
    """每盘抽一个开局种类：open_single 的概率是单招类开局，三种平分(积攒+炸弹/静止增伤/每个家族平均)；open_double 的概率是双招开局；其余照常"""
    u = rng.random(n)
    third = open_single / 3
    kind = np.full(n, OPEN_NONE, dtype=np.int64)
    kind[u < open_single] = OPEN_FAMILY
    kind[u < 2 * third] = OPEN_STATIC
    kind[u < third] = OPEN_ACC_BOMB
    kind[(u >= open_single) & (u < open_single + open_double)] = OPEN_DOUBLE
    return kind
