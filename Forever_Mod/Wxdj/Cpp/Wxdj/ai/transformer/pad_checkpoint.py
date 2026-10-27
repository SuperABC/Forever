"""把旧检查点的输入层补零，适配新增的特征维度(招式特征75/81->83、局面特征41->49)。

新增的维度都追加在原来的维度后面，把输入层权重对应的新列补成0，这样旧模型对任意局面的输出和补零前逐位一致
(新特征乘0)，可以直接导出并在游戏/网页里继续用；等重训之后新特征才会起作用。
用法：python pad_checkpoint.py [checkpoint.pt]    默认 checkpoints_pvp/latest.pt，原地覆盖(已经是新维度的会跳过)
"""
import os
import sys

import torch

HERE = os.path.dirname(os.path.abspath(__file__))
NEW = {'move_enc.net.0.weight': 83, 'state_enc.net.0.weight': 49}   # 输入层权重名 -> 新的输入维度


def pad(t, cols):
    if t.shape[1] >= cols:
        return t, False
    extra = torch.zeros(t.shape[0], cols - t.shape[1], dtype=t.dtype, device=t.device)
    return torch.cat([t, extra], dim=1), True


def pad_to_shapes(sd, ref):
    """把sd里两个输入层的权重补零到ref(目标网络的state_dict)的形状；返回补了哪些层。PvP模型 -> PvE模型(特征追加维度)热启动用：
    新增的特征维度在PvP对局里恒为0，补零列之后PvE网络一开始的输出和PvP模型逐位一致。"""
    grown = []
    for name in ('move_enc.net.0.weight', 'state_enc.net.0.weight'):
        if name in sd and name in ref and sd[name].shape[1] < ref[name].shape[1]:
            old = sd[name].shape[1]
            sd[name], _ = pad(sd[name], ref[name].shape[1])
            grown.append('%s %d->%d' % (name, old, ref[name].shape[1]))
    return grown


def pad_state_dict(sd):
    changed = False
    for name, cols in NEW.items():
        if name in sd:
            sd[name], c = pad(sd[name], cols)
            changed |= c
    return changed


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'checkpoints_pvp', 'latest.pt')
    ck = torch.load(path, map_location='cpu', weights_only=False)
    changed = pad_state_dict(ck['net'])
    for snap in ck.get('snaps', []) or []:
        sd = snap['net'] if isinstance(snap, dict) and 'net' in snap else snap
        if isinstance(sd, dict):
            pad_state_dict(sd)
    # 优化器的动量：形状是 (128,75) 和 (128,41) 的就是这两个输入层
    old_cols = {75: 83, 81: 83, 41: 49}
    opt = ck.get('opt')
    if opt and 'state' in opt:
        for st in opt['state'].values():
            for k, v in list(st.items()):
                if torch.is_tensor(v) and v.dim() == 2 and v.shape[0] == 128 and v.shape[1] in old_cols:
                    st[k], _ = pad(v, old_cols[v.shape[1]])
    if not changed:
        print('already padded:', path)
        return
    torch.save(ck, path)
    print('padded', path, {n: tuple(ck['net'][n].shape) for n in NEW})


if __name__ == '__main__':
    main()
