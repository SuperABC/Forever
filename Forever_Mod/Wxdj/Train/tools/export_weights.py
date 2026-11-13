"""把检查点导出成：1) 浏览器能直接加载的 ai_weights_pvp.js(Browser/public/下，供 ai.html 用)；2) 游戏里C++推理读的 Cpp/act/ai_weights_pvp.bin。
用法：python export_weights.py [checkpoint.pt]          PvP模型，默认 checkpoints_pvp/latest.pt
      (加 --out_dir 目录 可以输出到别处，不动仓库里的权重文件)
      checkpoint 也可以是 avg_weights.py 产出的权重平均文件(纯state_dict)。"""
import base64
import os
import sys

import numpy as np
import torch

HERE = os.path.dirname(os.path.abspath(__file__))
argv = list(sys.argv[1:])
OUT_DIR = None      # --out_dir DIR：输出到别处(冒烟测试用，不覆盖仓库里的权重文件)
if '--out_dir' in argv:
    i = argv.index('--out_dir')
    OUT_DIR = argv[i + 1]
    del argv[i:i + 2]
args = argv
SUFFIX = '_pvp'
OUT = os.path.join(OUT_DIR or os.path.join(HERE, '..', '..', 'Browser', 'public'), 'ai_weights%s.js' % SUFFIX)

path = args[0] if args else os.path.join(HERE, '..', 'models', 'base', 'base_avg.pt')
ck = torch.load(path, map_location='cpu', weights_only=False)
sd = ck['net'] if 'net' in ck else ck
names = list(sd.keys())
chunks = [sd[n].detach().float().contiguous().numpy().reshape(-1) for n in names]
data = np.concatenate(chunks).astype('<f4').tobytes()
js = 'window.WXDJ_AI_WEIGHTS%s = {\n  source: %r, iter: %s,\n  names: %s,\n  shapes: %s,\n  data: "%s"\n};\n' % (
    SUFFIX.upper(), os.path.basename(path), ck.get('iter', 'null') if isinstance(ck, dict) else 'null',
    [n for n in names], [list(sd[n].shape) for n in names], base64.b64encode(data).decode())
js = js.replace("'", '"')
open(OUT, 'w', encoding='utf-8', newline='\n').write(js)

# 游戏(UE)里的C++推理读这个二进制文件：'WXNN' | int32 版本=1 | int32 张量数 | 每个张量: int32 名字长度, 名字, int32 维数, int32 各维, float32 数据
BIN = os.path.join(OUT_DIR or os.path.join(HERE, '..', '..', 'Cpp', 'act'), 'ai_weights%s.bin' % SUFFIX)
import struct
with open(BIN, 'wb') as f:
    f.write(b'WXNN' + struct.pack('<ii', 1, len(names)))
    for n, c in zip(names, chunks):
        nb = n.encode('utf-8')
        f.write(struct.pack('<i', len(nb)) + nb + struct.pack('<i', len(sd[n].shape)))
        f.write(struct.pack('<%di' % len(sd[n].shape), *sd[n].shape))
        f.write(c.astype('<f4').tobytes())
print('wrote', os.path.abspath(BIN), '%.1f MB' % (os.path.getsize(BIN) / 1e6))
print('wrote', os.path.abspath(OUT), '%.1f MB' % (os.path.getsize(OUT) / 1e6), len(names), 'tensors')
