"""wxenv.dll的ctypes封装：批量并行的对战环境。"""
import ctypes
import os

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
_lib = ctypes.CDLL(os.path.join(HERE, 'wxenv.dll'))

_f32 = np.ctypeslib.ndpointer(dtype=np.float32, flags='C_CONTIGUOUS')
_i32 = np.ctypeslib.ndpointer(dtype=np.int32, flags='C_CONTIGUOUS')

_lib.wx_dims.argtypes = [_i32]
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

_d = np.zeros(4, dtype=np.int32)
_lib.wx_dims(_d)
FM, SD, HK, HD = [int(x) for x in _d]


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

    def reset(self, g, tpl_a, tpl_b, stats_a, stats_b):
        """stats: hp, atk, mp, agi, rec[, 跳过损失敏捷(默认1), 每回合恢复敏捷(默认0.2), 炸弹积攒倍率(默认10)]"""
        pad = lambda st: np.asarray(list(st) + [1.0, 0.2, 10.0][len(st) - 5:], dtype=np.float32)
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
