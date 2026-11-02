"""UE导出的静态网格FBX -> 带蒙皮的glb(root + 4根Phys_Wheel_*)。

车轮的识别全靠材质+连通性，不手选：
  1. 轮毂(Wheel材质)/轮胎(Tire材质)的面按顶点位置做连通分块；
  2. Tire块里直径最大的是真轮胎；离地的那个(底部明显高于最低点)是备胎，留在root上；
  3. 触地的轮胎按(前后,左右)归到四根轮骨，后桥双胎共用一根(骨骼放在两胎中间)；
  4. 轮毂等小块归最近的轮胎；其它材质落在轮子圆柱里的面(制动鼓/轴头)也跟着转。
朝向：车头方向由Radiator/Headlight材质的位置判断，整体旋转成车头+X，原点放在车身中心的地面上。
"""
import argparse, collections, json, math, struct, sys
from array import array
import fbxparse

ap = argparse.ArgumentParser()
ap.add_argument('src'); ap.add_argument('out')
ap.add_argument('--mesh', default='zil_130_body_lowpoly')
ap.add_argument('--hub-mat', default='ZIL130_Wheel')
ap.add_argument('--tire-mat', default='ZIL130_Tire')
ap.add_argument('--front-mats', default='ZIL130_Radiator,ZIL130_Headlight')
ap.add_argument('--drop-in-wheel', default='', help='逗号分隔的材质名：落在轮子圆柱里的这些材质的面直接删除')
ap.add_argument('--forward', default='auto', help='auto 或 +X/-X/+Y/-Y(UE坐标下车头方向)')
ap.add_argument('--no-recenter', action='store_true', help='不重新定原点/朝向(保持UE里的坐标)')
args = ap.parse_args()

root, ver = fbxparse.parse(args.src)
obj = root.first('Objects')
geoms = obj.find('Geometry')
geom = next((g for g in geoms if g.props[1].startswith(args.mesh + '\x00')), geoms[0])
models = obj.find('Model')
model = next((m for m in models if m.props[1].startswith(args.mesh + '\x00')), None)
V = geom.first('Vertices').props[0]
PV = geom.first('PolygonVertexIndex').props[0]
mats_by_id = {m.props[0]: m.props[1].split('\x00')[0] for m in obj.find('Material')}
order = []
for c in root.first('Connections').children:
    if c.props[0] == 'OO' and c.props[1] in mats_by_id and (model is None or c.props[2] == model.props[0]):
        order.append(mats_by_id[c.props[1]])
matidx = geom.first('LayerElementMaterial').first('Materials').props[0]
normals = geom.first('LayerElementNormal').first('Normals').props[0]
uv_layers = []
for le in geom.find('LayerElementUV')[:3]:
    uv_layers.append((le.first('UV').props[0], le.first('UVIndex').props[0]))

# ---- 多边形 -> 三角形(扇形剖分)，保留每个角在PolygonVertex里的序号(法线/UV按它索引) ----
polys, cur, cur_c = [], [], []
tri_mat = []
corner = 0
poly_i = 0
tris = []   # [(v0,v1,v2),(c0,c1,c2),mat]
for i in PV:
    cur_c.append(corner); corner += 1
    if i < 0:
        cur.append(~i)
        for k in range(1, len(cur) - 1):
            tris.append(((cur[0], cur[k], cur[k + 1]), (cur_c[0], cur_c[k], cur_c[k + 1]), matidx[poly_i]))
        cur, cur_c = [], []
        poly_i += 1
    else:
        cur.append(i)
print('mesh', args.mesh, 'verts', len(V) // 3, 'polys', poly_i, 'tris', len(tris))
print('materials', order)

# ---- UE坐标：FBX的y是UE的-y(UE导出时翻转) ----
def ue(i):
    return (V[3 * i], -V[3 * i + 1], V[3 * i + 2])

xs = [V[3 * i] for i in range(len(V) // 3)]
ys = [-V[3 * i + 1] for i in range(len(V) // 3)]
zs = [V[3 * i + 2] for i in range(len(V) // 3)]
bb = (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs))
print('UE bbox x[%.1f,%.1f] y[%.1f,%.1f] z[%.1f,%.1f]' % bb)

def tri_centroid(t):
    p = [ue(v) for v in t[0]]
    return tuple(sum(q[a] for q in p) / 3 for a in range(3))

# ---- 朝向 ----
fwd = args.forward
if fwd == 'auto':
    fm = set(args.front_mats.split(','))
    pts = [tri_centroid(t) for t in tris if order[t[2]] in fm]
    if not pts:
        sys.exit('找不到前脸材质 %s，用 --forward 指定' % args.front_mats)
    mx, my = (bb[0] + bb[1]) / 2, (bb[2] + bb[3]) / 2
    ax = sum(p[0] for p in pts) / len(pts) - mx
    ay = sum(p[1] for p in pts) / len(pts) - my
    print('front materials centroid offset from center: dx=%.1f dy=%.1f' % (ax, ay))
    fwd = ('+X' if ax > 0 else '-X') if abs(ax) >= abs(ay) else ('+Y' if ay > 0 else '-Y')
print('forward ->', fwd)
ROT = {'+X': lambda x, y: (x, y), '-X': lambda x, y: (-x, -y), '+Y': lambda x, y: (y, -x), '-Y': lambda x, y: (-y, x)}[fwd]

if args.no_recenter:
    rx = ry = 0.0; zbase = 0.0
    cx0 = cy0 = 0.0
else:
    rxy = [ROT(x, y) for x, y in zip(xs, ys)]
    cx0 = (min(p[0] for p in rxy) + max(p[0] for p in rxy)) / 2
    cy0 = (min(p[1] for p in rxy) + max(p[1] for p in rxy)) / 2
    zbase = bb[4]

def T(p):
    x, y = ROT(p[0], p[1])
    return (x - cx0, y - cy0, p[2] - zbase)

def Tn(n):
    x, y = ROT(n[0], -n[1])  # 法线同样先FBX->UE(翻y)再旋转
    return (x, y, n[2])

P = [T(ue(i)) for i in range(len(V) // 3)]
tx = [p[0] for p in P]; ty = [p[1] for p in P]; tz = [p[2] for p in P]
print('after: x[%.1f,%.1f] y[%.1f,%.1f] z[%.1f,%.1f]' % (min(tx), max(tx), min(ty), max(ty), min(tz), max(tz)))

def tcent(t):
    p = [P[v] for v in t[0]]
    return tuple(sum(q[a] for q in p) / 3 for a in range(3))

# ---- 车轮连通块 ----
def key(i):
    return (round(V[3 * i], 2), round(V[3 * i + 1], 2), round(V[3 * i + 2], 2))
parent = {}
def find(a):
    while parent.setdefault(a, a) != a:
        parent[a] = parent[parent[a]]; a = parent[a]
    return a
wheel_tris = [ti for ti, t in enumerate(tris) if order[t[2]] in (args.hub_mat, args.tire_mat)]
for ti in wheel_tris:
    ks = [key(v) for v in tris[ti][0]]
    for k in ks[1:]:
        ra, rb = find(ks[0]), find(k)
        if ra != rb: parent[ra] = rb
comps = collections.defaultdict(list)
for ti in wheel_tris:
    comps[find(key(tris[ti][0][0]))].append(ti)

def bbox_of(tlist):
    vs = set()
    for ti in tlist: vs.update(tris[ti][0])
    a = [P[v] for v in vs]
    return [min(p[k] for p in a) for k in range(3)] + [max(p[k] for p in a) for k in range(3)]

info = []
for rk, tl in comps.items():
    b = bbox_of(tl)
    mats = collections.Counter(order[tris[ti][2]] for ti in tl)
    info.append({'tris': tl, 'bb': b, 'dia': b[5] - b[2], 'ctr': ((b[0] + b[3]) / 2, (b[1] + b[4]) / 2, (b[2] + b[5]) / 2), 'mats': mats})
info.sort(key=lambda c: -len(c['tris']))
print('wheel/tire components:', len(info))
for c in info[:14]:
    print('  %5d tris dia=%.1f ctr=(%.1f,%.1f,%.1f) y[%.1f,%.1f] z[%.1f,%.1f] %s' % (
        len(c['tris']), c['dia'], c['ctr'][0], c['ctr'][1], c['ctr'][2], c['bb'][1], c['bb'][4], c['bb'][2], c['bb'][5], dict(c['mats'])))

# 真轮胎候选：含Tire材质、直径接近最大值
tire_c = [c for c in info if c['mats'].get(args.tire_mat, 0) > 0]
maxdia = max(c['dia'] for c in tire_c)
tire_c = [c for c in tire_c if c['dia'] > 0.7 * maxdia]
zmin = min(tz)
ground = [c for c in tire_c if c['bb'][2] - zmin < 0.15 * maxdia]
spare = [c for c in tire_c if c not in ground]
print('tires: %d on ground, %d off ground (spare)' % (len(ground), len(spare)))
for c in spare:
    print('  spare ctr=(%.1f,%.1f,%.1f) zmin=%.1f' % (c['ctr'] + (c['bb'][2],)))

# 触地轮胎 -> 前/后 x 左/右
xs_g = sorted(set(round(c['ctr'][0], 0) for c in ground))
clusters = []
for x in xs_g:
    if clusters and x - clusters[-1][-1] < 0.5 * maxdia: clusters[-1].append(x)
    else: clusters.append([x])
print('axle x clusters:', clusters)
if len(clusters) != 2:
    sys.exit('预期前后2根轴，实际%d，需要人工看一下' % len(clusters))
front_x = sum(clusters[1]) / len(clusters[1])
def axle_of(c): return 'F' if abs(c['ctr'][0] - front_x) < 0.5 * maxdia else 'B'
def side_of(c): return 'L' if c['ctr'][1] < 0 else 'R'   # UE里车头+X时，左=-Y
groups = collections.defaultdict(list)
for c in ground:
    groups[axle_of(c) + side_of(c)].append(c)
BONE_NAMES = ['Phys_Wheel_FL', 'Phys_Wheel_FR', 'Phys_Wheel_BL', 'Phys_Wheel_BR']
wheel_bone = {}
bone_pos = []
for bi, bn in enumerate(BONE_NAMES):
    g = groups.get(bn[-2:], [])
    if not g: sys.exit('缺少轮位 ' + bn)
    px = sum(c['ctr'][0] for c in g) / len(g)
    py = sum(c['ctr'][1] for c in g) / len(g)
    pz = sum(c['ctr'][2] for c in g) / len(g)
    bone_pos.append((bn, (px, py, pz), max(c['dia'] for c in g) / 2, len(g)))
    for c in g: wheel_bone[id(c)] = bi + 1
print('bones:')
for bn, p, r, n in bone_pos:
    print('  %s ue=(%.1f, %.1f, %.1f) radius=%.1f tires=%d' % (bn, p[0], p[1], p[2], r, n))

# 其余wheel/hub小块：归最近的触地轮胎(含备胎当候选，备胎的归root)
cands = [(c, wheel_bone[id(c)]) for c in ground] + [(c, 0) for c in spare]
bone_of_tri = {}
for c in info:
    if id(c) in wheel_bone:
        for ti in c['tris']: bone_of_tri[ti] = wheel_bone[id(c)]
        continue
    if c in spare:
        for ti in c['tris']: bone_of_tri[ti] = 0
        continue
    best = min(cands, key=lambda cb: (cb[0]['ctr'][0] - c['ctr'][0]) ** 2 + (cb[0]['ctr'][2] - c['ctr'][2]) ** 2 + (cb[0]['ctr'][1] - c['ctr'][1]) ** 2)
    d = math.sqrt((best[0]['ctr'][0] - c['ctr'][0]) ** 2 + (best[0]['ctr'][2] - c['ctr'][2]) ** 2 + (best[0]['ctr'][1] - c['ctr'][1]) ** 2)
    b = best[1] if d < 0.8 * maxdia else 0
    for ti in c['tris']: bone_of_tri[ti] = b

# 其它材质落在触地轮圆柱里的面(制动鼓/轴头)也跟着转
extra = collections.Counter()
dropped = set()
drop_mats = set(m for m in args.drop_in_wheel.split(',') if m)
for ti, t in enumerate(tris):
    if ti in bone_of_tri: continue
    cx, cy, cz = tcent(t)
    for c, bi in cands:
        if bi == 0: continue
        r = c['dia'] / 2
        if math.hypot(cx - c['ctr'][0], cz - c['ctr'][2]) < 0.92 * r and c['bb'][1] - 2 <= cy <= c['bb'][4] + 2:
            if order[t[2]] in drop_mats:
                dropped.add(ti)   # 落在轮子圆柱里的这种材质的面(比如轮拱内衬片)直接删掉
            else:
                bone_of_tri[ti] = bi
                extra[order[t[2]]] += 1
            break
print('dropped tris:', len(dropped))
print('extra tris attached to wheels by material:', dict(extra))
print('tris per bone:', dict(collections.Counter(bone_of_tri.get(ti, 0) for ti in range(len(tris)))))
spare_tris = sum(len(c['tris']) for c in spare)
print('spare tris left on root:', spare_tris)

# ---- glTF ----
blob = bytearray()
buffer_views, accessors, primitives, gl_materials = [], [], [], []
def add_view(data, target=None):
    while len(blob) % 4: blob.append(0)
    off = len(blob); blob.extend(data)
    view = {"buffer": 0, "byteOffset": off, "byteLength": len(data)}
    if target: view["target"] = target
    buffer_views.append(view); return len(buffer_views) - 1
def add_acc(view, comp, count, typ, mn=None, mx=None):
    acc = {"bufferView": view, "componentType": comp, "count": count, "type": typ}
    if mn is not None: acc["min"], acc["max"] = mn, mx
    accessors.append(acc); return len(accessors) - 1

per_mat = collections.defaultdict(list)
for ti, t in enumerate(tris):
    if ti in dropped: continue
    per_mat[t[2]].append(ti)
for m in sorted(per_mat):
    tl = per_mat[m]
    pos, nor, jts, wts = array('f'), array('f'), bytearray(), array('f')
    uvs = [array('f') for _ in uv_layers]
    for ti in tl:
        bone = bone_of_tri.get(ti, 0)
        vs, cs, _ = tris[ti]
        for k in range(3):
            x, y, z = P[vs[k]]
            pos.extend((x / 100.0, z / 100.0, y / 100.0))     # glTF=(ue.x, ue.z, ue.y)/100，和公交那次校准一致
            c = cs[k]
            nx, ny, nz = Tn((normals[3 * c], normals[3 * c + 1], normals[3 * c + 2]))
            nor.extend((nx, nz, ny))
            jts.extend((bone, 0, 0, 0))
            wts.extend((1.0, 0.0, 0.0, 0.0))
            for li, (uv, uvi) in enumerate(uv_layers):
                idx = uvi[c]
                uvs[li].extend((uv[2 * idx], 1.0 - uv[2 * idx + 1]))
    n = len(tl) * 3
    px_, py_, pz_ = pos[0::3], pos[1::3], pos[2::3]
    attrs = {}
    attrs["POSITION"] = add_acc(add_view(pos.tobytes(), 34962), 5126, n, "VEC3", [min(px_), min(py_), min(pz_)], [max(px_), max(py_), max(pz_)])
    attrs["NORMAL"] = add_acc(add_view(nor.tobytes(), 34962), 5126, n, "VEC3")
    for li in range(len(uv_layers)):
        attrs["TEXCOORD_%d" % li] = add_acc(add_view(uvs[li].tobytes(), 34962), 5126, n, "VEC2")
    attrs["JOINTS_0"] = add_acc(add_view(bytes(jts), 34962), 5121, n, "VEC4")
    attrs["WEIGHTS_0"] = add_acc(add_view(wts.tobytes(), 34962), 5126, n, "VEC4")
    ind = array('I', range(n))
    a_idx = add_acc(add_view(ind.tobytes(), 34963), 5125, n, "SCALAR")
    gl_materials.append({"name": order[m], "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1], "metallicFactor": 0.0, "roughnessFactor": 0.8}, "doubleSided": False})
    primitives.append({"attributes": attrs, "indices": a_idx, "material": len(gl_materials) - 1})

BONES = [('root', (0.0, 0.0, 0.0))] + [(bn, p) for bn, p, _, _ in bone_pos]
def to_gltf(u): return [u[0] / 100.0, u[2] / 100.0, u[1] / 100.0]
nodes = []
for name, u in BONES:
    nodes.append({"name": name, "translation": to_gltf(u)})
nodes[0]["children"] = [1, 2, 3, 4]
for n in nodes[1:]:
    n["translation"] = [a - b for a, b in zip(n["translation"], nodes[0]["translation"])]
nodes.append({"name": args.mesh, "mesh": 0, "skin": 0})
ibm = bytearray()
for name, u in BONES:
    t = to_gltf(u)
    ibm += struct.pack("<16f", 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -t[0], -t[1], -t[2], 1)
a_ibm = add_acc(add_view(bytes(ibm)), 5126, len(BONES), "MAT4")

gltf = {
    "asset": {"version": "2.0", "generator": "vehicle skin script"},
    "scene": 0,
    "scenes": [{"nodes": [0, len(nodes) - 1]}],
    "nodes": nodes,
    "skins": [{"joints": list(range(len(BONES))), "inverseBindMatrices": a_ibm, "skeleton": 0}],
    "meshes": [{"name": args.mesh, "primitives": primitives}],
    "materials": gl_materials,
    "accessors": accessors,
    "bufferViews": buffer_views,
    "buffers": [{"byteLength": len(blob)}],
}
js = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
while len(js) % 4: js += b" "
while len(blob) % 4: blob.append(0)
total = 12 + 8 + len(js) + 8 + len(blob)
with open(args.out, "wb") as f:
    f.write(struct.pack("<4sII", b"glTF", 2, total))
    f.write(struct.pack("<I4s", len(js), b"JSON")); f.write(js)
    f.write(struct.pack("<I4s", len(blob), b"BIN\0")); f.write(blob)
print("wrote", args.out, total, "bytes,", len(primitives), "primitives")
