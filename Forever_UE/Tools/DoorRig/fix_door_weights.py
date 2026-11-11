# 按几何连通块重写门骨骼网格的蒙皮权重：包围盒最大的块(门框)→root，其余块(门扇、读卡器等)→door。
# 用法(编辑器必须关闭)：UnrealEditor-Cmd.exe Forever.uproject -run=pythonscript -script="<本文件>" [apply]
# 不带apply只打印各块信息(dry run)，带apply才写回资产。
import sys
import unreal

ASSET = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/SKM_door_with_cardreader"
import os
APPLY = os.environ.get("DOOR_APPLY") == "1"

sk = unreal.load_asset(ASSET)
dm = unreal.DynamicMesh()
opts = unreal.GeometryScriptCopyMeshFromAssetOptions()
lod = unreal.GeometryScriptMeshReadLOD(unreal.GeometryScriptLODType.MAX_AVAILABLE, 0)
dm, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(sk, dm, opts, lod)
unreal.log_warning(str(("copy outcome:", outcome)))

_, tri_list, _ = unreal.GeometryScript_MeshQueries.get_all_triangle_indices(dm, True)
tris = [(t.x, t.y, t.z) for t in unreal.GeometryScript_List.convert_triangle_list_to_array(tri_list)]
_, pos_list, _ = unreal.GeometryScript_MeshQueries.get_all_vertex_positions(dm, True)
pos = list(unreal.GeometryScript_List.convert_vector_list_to_array(pos_list))
nv = len(pos)

parent = list(range(nv))
def find(a):
    while parent[a] != a:
        parent[a] = parent[parent[a]]
        a = parent[a]
    return a
for a, b, c in tris:
    parent[find(a)] = find(b)
    parent[find(b)] = find(c)

islands = {}
for v in range(nv):
    islands.setdefault(find(v), []).append(v)

info = []
for root, vs in islands.items():
    xs = [pos[v].x for v in vs]; ys = [pos[v].y for v in vs]; zs = [pos[v].z for v in vs]
    size = (max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs))
    info.append((size[0] * size[1] * size[2], root, vs, size))
info.sort(reverse=True)
for vol, root, vs, size in info:
    cx = (min(pos[v].x for v in vs) + max(pos[v].x for v in vs)) / 2; cz = (min(pos[v].z for v in vs) + max(pos[v].z for v in vs)) / 2; cy = (min(pos[v].y for v in vs) + max(pos[v].y for v in vs)) / 2
    unreal.log_warning(str(("island", root, "verts", len(vs), "size", tuple(round(s, 1) for s in size), "center", (round(cx, 1), round(cy, 1), round(cz, 1)))))

def is_frame(vs):
    # 门扇盒：x∈[-40.4,40.4], z∈[-0.1,93.4]，厚度<6；超出盒或y向跨度≥6的块算门框(洞口内侧面、外框平面)
    xs = [pos[v].x for v in vs]; ys = [pos[v].y for v in vs]; zs = [pos[v].z for v in vs]
    # 铰链侧(x=+40.3)洞口内侧的白色长条：贴在x=40.3平面、通高、只有4个顶点的块是门框(门扇边缘在这个平面上是336那种12顶点块和z 8~88的短块)
    if len(vs) == 4 and max(xs) - min(xs) < 0.1 and abs(xs[0] - 40.3) < 0.2 and max(zs) - min(zs) > 93.0:
        return True
    if min(xs) < -40.4 and max(xs) > 40.4 or min(xs) < -40.6 or max(xs) > 40.6 or max(zs) > 93.4:
        return True
    return max(ys) - min(ys) >= 6.0 and abs(max(xs) - min(xs)) > 20 or (max(ys) - min(ys) >= 6.0 and abs(max(xs) - min(xs)) < 0.5)

frame_verts = set()
for vol, root, vs, size in info:
    if is_frame(vs):
        frame_verts.update(vs)
unreal.log_warning(str(("frame verts", len(frame_verts), "of", nv)))
seen = {}
for v in range(nv):
    r = unreal.GeometryScript_BoneWeights.get_vertex_bone_weights(dm, v)
    ws = r[1] if isinstance(r, tuple) and len(r) > 1 else []
    key = (v in frame_verts, tuple((w.bone_index, round(w.weight, 2)) for w in ws))
    seen[key] = seen.get(key, 0) + 1
for k, n in sorted(seen.items(), key=lambda kv: -kv[1])[:12]:
    unreal.log_warning(str(("frame?", k[0], "weights", k[1], "verts", n)))

if not APPLY:
    raise SystemExit

ROOT_BONE, DOOR_BONE = 0, 1   # 骨骼顺序：root在前、door在后(dry run后若不符再改)
wr = [unreal.GeometryScriptBoneWeight(ROOT_BONE, 1.0)]
wd = [unreal.GeometryScriptBoneWeight(DOOR_BONE, 1.0)]
for v in range(nv):
    dm, _ = unreal.GeometryScript_BoneWeights.set_vertex_bone_weights(dm, v, wr if v in frame_verts else wd)
wopts = unreal.GeometryScriptCopyMeshToAssetOptions()
wlod = unreal.GeometryScriptMeshWriteLOD(False, 0)
dm, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_skeletal_mesh(dm, sk, wopts, wlod)
unreal.log_warning(str(("write outcome:", outcome)))
unreal.EditorAssetLibrary.save_asset(ASSET)
