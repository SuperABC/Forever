# 核对：import_traffic_light.py 导入后又导出的 check/*.obj 和原模型导出的 retro_traffic_light.obj 逐个三角形比对，
# 确认拆分+导入没有改变几何：每个三角形的三个顶点位置一致、绕序一致(绕序反了面会朝里)、UV一致。
# 用法：python Tools/TrafficLight/check_traffic_light.py <导出目录(有retro_traffic_light.obj)> <split输出目录(有check/)>
import os
import sys

import numpy as np


def load_obj(path):
    positions, uvs, faces = [], [], []
    for line in open(path):
        if line.startswith("v "):
            positions.append([float(x) for x in line.split()[1:4]])
        elif line.startswith("vt "):
            uvs.append([float(x) for x in line.split()[1:3]])
        elif line.startswith("f "):
            faces.append([[int(p) - 1 for p in token.split("/")[:2]] for token in line.split()[1:4]])
    return np.array(positions), np.array(uvs), np.array(faces)


def triangles(path):
    positions, uvs, faces = load_obj(path)
    out = {}
    for f in faces:
        pts = [tuple(np.round(positions[c[0]], 2)) for c in f]
        uv = [tuple(np.round(uvs[c[1]], 3)) for c in f]
        k = min(range(3), key=lambda i: pts[i])  # 循环移位到最小点开头：绕序保留，起点无关
        key = tuple(pts[k:] + pts[:k])
        out.setdefault(key, []).append(tuple(uv[k:] + uv[:k]))
    return out, len(faces)


def main():
    src, split_dir = sys.argv[1], sys.argv[2]
    original, original_count = triangles(os.path.join(src, "retro_traffic_light.obj"))
    flipped_original = {}
    for key in original:
        a, b, c = key
        k = min(range(3), key=lambda i: (a, c, b)[i])
        rev = (a, c, b)
        flipped_original[tuple(rev[k:] + rev[:k])] = True
    print("original: %d triangles, %d distinct" % (original_count, len(original)))
    total = 0
    for name in ("Frame", "Red", "Yellow", "Green"):
        tris, count = triangles(os.path.join(split_dir, "check", "SM_TrafficLight_%s.obj" % name))
        total += count
        same = sum(1 for k in tris if k in original)
        flipped = sum(1 for k in tris if k not in original and k in flipped_original)
        missing = count - same - flipped
        uv_ok = sum(1 for k, uvs in tris.items() if k in original and any(u in original[k] for u in uvs))
        print("%-6s %5d triangles: same winding %d, flipped %d, not in original %d, uv match %d" % (name, count, same, flipped, missing, uv_ok))
    print("imported total %d vs original %d" % (total, original_count))


if __name__ == "__main__":
    main()
