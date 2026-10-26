"""把检查点导出成浏览器能直接加载的 nn_weights.js(Tools/Local/下，供 wxdj_simulator.html 用)。
用法：python export_weights.py [checkpoint.pt]    默认 checkpoints/latest.pt"""
import base64
import os
import sys

import numpy as np
import torch

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', '..', '..', '..', 'Tools', 'Local', 'nn_weights.js')

path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'checkpoints', 'latest.pt')
ck = torch.load(path, map_location='cpu')
sd = ck['net'] if 'net' in ck else ck
names = list(sd.keys())
chunks = [sd[n].detach().float().contiguous().numpy().reshape(-1) for n in names]
data = np.concatenate(chunks).astype('<f4').tobytes()
js = 'window.WXDJ_NN_WEIGHTS = {\n  source: %r, iter: %s,\n  names: %s,\n  shapes: %s,\n  data: "%s"\n};\n' % (
    os.path.basename(path), ck.get('iter', 'null') if isinstance(ck, dict) else 'null',
    [n for n in names], [list(sd[n].shape) for n in names], base64.b64encode(data).decode())
js = js.replace("'", '"')
open(OUT, 'w', encoding='utf-8', newline='\n').write(js)
print('wrote', os.path.abspath(OUT), '%.1f MB' % (os.path.getsize(OUT) / 1e6), len(names), 'tensors')
