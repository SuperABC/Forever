"""把检查点导出成：1) 浏览器能直接加载的 ai_weights.js(Online/public/下，供 ai.html 用)；2) 游戏里C++推理读的 Resource/act/ai_weights.bin。
用法：python export_weights.py [checkpoint.pt]    默认 checkpoints/latest.pt"""
import base64
import os
import sys

import numpy as np
import torch

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', '..', '..', '..', 'Online', 'public', 'ai_weights.js')

path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'checkpoints', 'latest.pt')
ck = torch.load(path, map_location='cpu')
sd = ck['net'] if 'net' in ck else ck
names = list(sd.keys())
chunks = [sd[n].detach().float().contiguous().numpy().reshape(-1) for n in names]
data = np.concatenate(chunks).astype('<f4').tobytes()
js = 'window.WXDJ_AI_WEIGHTS = {\n  source: %r, iter: %s,\n  names: %s,\n  shapes: %s,\n  data: "%s"\n};\n' % (
    os.path.basename(path), ck.get('iter', 'null') if isinstance(ck, dict) else 'null',
    [n for n in names], [list(sd[n].shape) for n in names], base64.b64encode(data).decode())
js = js.replace("'", '"')
open(OUT, 'w', encoding='utf-8', newline='\n').write(js)

# 游戏(UE)里的C++推理读这个二进制文件：'WXNN' | int32 版本=1 | int32 张量数 | 每个张量: int32 名字长度, 名字, int32 维数, int32 各维, float32 数据
BIN = os.path.join(HERE, '..', '..', '..', '..', 'Resource', 'act', 'ai_weights.bin')
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
