"""wxenv.dll(Linux/macOS是libwxenv.so)的ctypes封装：批量并行的对战环境。

两种模式：PvP(默认，维数FM/SD和游戏里的PvP模型一致)、PvE(双方有五行属性/暴击/特性，特征在PvP维数后面追加)。
模式必须在创建Env之前用 set_mode 设定，设定后 FM/SD 才是对应模式的维数(别的模块要用 wxenv.FM 而不是 from wxenv import FM)。
"""
import ctypes
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
_lib = ctypes.CDLL(os.path.join(HERE, os.environ.get('WXENV_DLL') or ('wxenv.dll' if sys.platform == 'win32' else 'libwxenv.so')))   # WXENV_DLL：训练占着wxenv.dll时，另编一份(build.ps1 -Out)测试用

_f32 = np.ctypeslib.ndpointer(dtype=np.float32, flags='C_CONTIGUOUS')
_i32 = np.ctypeslib.ndpointer(dtype=np.int32, flags='C_CONTIGUOUS')

_lib.wx_dims.argtypes = [_i32]
_lib.wx_set_mode.argtypes = [ctypes.c_int]
_lib.wx_pve_set.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_float, ctypes.c_float, ctypes.c_char_p, ctypes.c_char_p]
_lib.wx_pve_noacc.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
_lib.wx_pool_add.argtypes = [ctypes.c_char_p]
_lib.wx_pool_add.restype = ctypes.c_int
_lib.wx_pool_size.restype = ctypes.c_int
_lib.wx_pool_moves.argtypes = [ctypes.c_int]
_lib.wx_pool_moves.restype = ctypes.c_int
_lib.wx_init.argtypes = [ctypes.c_int]
_lib.wx_reset.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, _f32, _f32]
_lib.wx_nmoves.argtypes = [ctypes.c_int, ctypes.c_int]
_lib.wx_nmoves.restype = ctypes.c_int
_lib.wx_moves.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, _f32, _i32, _i32]
_lib.wx_obs_batch.argtypes = [ctypes.c_int, _i32, _f32, _f32, _i32]
_lib.wx_step_batch.argtypes = [ctypes.c_int, _i32, _i32, _f32, _i32, _i32]

PVE = False
FM = SD = HK = HD = 0


def _read_dims():
    global FM, SD, HK, HD
    d = np.zeros(4, dtype=np.int32)
    _lib.wx_dims(d)
    FM, SD, HK, HD = [int(x) for x in d]


def set_mode(pve):
    """pve=True切到PvE模式(特征维数增加)。要在创建Env和模型之前调用。"""
    global PVE
    PVE = bool(pve)
    _lib.wx_set_mode(1 if PVE else 0)
    _read_dims()


_read_dims()


def pool_clear():
    _lib.wx_pool_clear()


def pool_add(path):
    return _lib.wx_pool_add(path.encode('utf-8'))


class Env:
    def __init__(self, n_games, max_moves):
        self.n, self.M = n_games, max_moves
        _lib.wx_init(n_games)
        self.moves = np.zeros((n_games, 2, max_moves, FM), dtype=np.float32)
        self.form = np.full((n_games, 2, max_moves), -2, dtype=np.int32)
        self.fam = np.full((n_games, 2, max_moves), -2, dtype=np.int32)
        self.gids = np.arange(n_games, dtype=np.int32)

    def reset(self, g, tpl_a, tpl_b, stats_a, stats_b, pve=None):
        """stats: hp, atk, mp, agi, rec[, 跳过损失敏捷(默认1), 每回合恢复敏捷(默认0.2), 炸弹积攒倍率(默认10)]
        pve(只在PvE模式下用，见pve_gen.py)：dict(elem=(甲,乙)五行属性掩码, crit=(暴击率, 暴击伤害), traits=(甲文本, 乙文本))，None=无属性无暴击无特性"""
        pad = lambda st: np.asarray(list(st) + [1.0, 0.2, 10.0][len(st) - 5:], dtype=np.float32)
        if PVE:
            c = pve or {}
            e, cr, tr = c.get('elem', (0, 0)), c.get('crit', (0.0, 0.5)), c.get('traits', ('', ''))
            _lib.wx_pve_set(g, int(e[0]), int(e[1]), float(cr[0]), float(cr[1]), tr[0].encode('utf-8'), tr[1].encode('utf-8'))
            na = c.get('noacc', (0, 0))
            _lib.wx_pve_noacc(g, int(na[0]), int(na[1]))
        _lib.wx_reset(g, tpl_a, tpl_b, pad(stats_a), pad(stats_b))
        for f in range(2):
            _lib.wx_moves(g, f, self.M, self.moves[g, f], self.form[g, f], self.fam[g, f])

    def nmoves(self, g, f):
        return _lib.wx_nmoves(g, f)

    def obs(self, gids=None):
        gids = self.gids if gids is None else np.ascontiguousarray(gids, dtype=np.int32)
        n = len(gids)
        state = np.zeros((n, 2, SD), dtype=np.float32)
        hist = np.zeros((n, 2, HK, HD), dtype=np.float32)
        done = np.zeros(n, dtype=np.int32)
        _lib.wx_obs_batch(n, gids, state, hist, done)
        return state, hist, done

    def step(self, act, gids=None):
        """act: (n,2,3) int32 [cls, move1, move2]"""
        gids = self.gids if gids is None else np.ascontiguousarray(gids, dtype=np.int32)
        n = len(gids)
        act = np.ascontiguousarray(act, dtype=np.int32)
        reward = np.zeros((n, 2), dtype=np.float32)
        done = np.zeros(n, dtype=np.int32)
        outcome = np.zeros(n, dtype=np.int32)
        _lib.wx_step_batch(n, gids, act, reward, done, outcome)
        return reward, done, outcome
