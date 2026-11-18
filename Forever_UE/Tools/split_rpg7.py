# 把Fab下载的rpg7.glb(发射器+火箭弹摆在一个模型里，爆炸图布局)拆成两个只含几何+UV的glb：
#   SM_RPG7_Launcher.glb  发射器(RPG7本体+sight+sight_adjust+hammer)，原点在尾端枪管轴线上、枪口朝UE的+X
#   SM_RPG7_Rocket.glb    火箭弹(弹头+前面一小段发动机圆柱，在CUT_Y处平切并补封口)，原点在尾端轴线上、弹头朝UE的+X
# 不带贴图(材质只留名字RPG7/RPG7Rocket)：导入UE后用 import_rpg7.py 把材质槽指到现有的
# /Game/3rdParty/RPG7/Materials/RPG7 与 RPG7Rocket，不重复导贴图。
#
# 原模型是"爆炸图"：发射器轴线在x=1.6，火箭弹在x=-8.33(-10)，sight/sight_adjust在x=12.5(+10)，hammer在原位。
# 剖面实测(沿y，-y是枪口方向)：火箭弹弹头 y -46.4~-31，颈圈 -29~-28，然后是r≈0.94的直圆柱 -28.3~-5.9(23单位长，
# 是插在发射管里的发动机段)，飞行弹不需要这么长，所以在圆柱的一圈顶点(y≈-18.9)处平切，保留约8.5单位。
# 要更长/更短：改CUT_Y(必须取某一圈顶点的y，可选值 -28.2/-18.9/-17.7)，重跑即可；想整根去掉圆柱用 -28.2。
#
# 用法：python Tools/split_rpg7.py <rpg7.glb> <输出目录>
# 坐标/缩放：见下面 FORWARD_TO_GLTF 和 SCALE 的说明；import_rpg7.py 会打印导入后的UE包围盒供核对。
import json
import math
import struct
import sys

import numpy as np

CUT_Y = -18.9              # 火箭弹平切位置(glTF原始y，见上)
SIGHT_SHIFT_X = -10.0      # 爆炸图里sight/sight_adjust被挪到+10，挪回来
ROCKET_SHIFT_X = 9.93      # 火箭弹被挪到-9.93，挪回发射管轴线(仅用于装配示意，输出时火箭弹自己对齐到原点)
LAUNCHER_AXIS = (1.6, -0.4)  # 发射器枪管轴线(x, z)
LAUNCHER_REAR_Y = 18.8       # 发射器尾端(y最大)；枪口在y=-29.1
ROCKET_AXIS = (-8.33, -0.40)  # 火箭弹轴线(x, z)

# 输出坐标：UE导入glTF时 UE=(gx, gz, gy)*100(米->厘米)，所以要让局部的 前=UE+X、右=UE+Y、上=UE+Z，
# 就得把 前放到glTF的+X、右放到+Z、上放到+Y(已用实测包围盒核对，见import_rpg7.py的日志)。
FORWARD_TO_GLTF = np.array([1.0, 0.0, 0.0])
RIGHT_TO_GLTF = np.array([0.0, 0.0, 1.0])
UP_TO_GLTF = np.array([0.0, 1.0, 0.0])
# 原模型单位按"米"导入会放大100倍(发射器47.9单位=47.9米)，缩到真实尺寸：RPG-7发射器长约0.95米 -> 47.9*0.02*100=96cm。
SCALE = 0.02


def load_glb(path):
    data = open(path, "rb").read()
    json_len = struct.unpack("<I", data[12:16])[0]
    gltf = json.loads(data[20:20 + json_len])
    bin_start = 20 + json_len + 8

    def accessor(index):
        acc = gltf["accessors"][index]
        view = gltf["bufferViews"][acc["bufferView"]]
        comp = {5126: np.float32, 5125: np.uint32, 5123: np.uint16}[acc["componentType"]]
        width = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[acc["type"]]
        offset = bin_start + view.get("byteOffset", 0) + acc.get("byteOffset", 0)
        stride = view.get("byteStride") or np.dtype(comp).itemsize * width
        out = np.empty((acc["count"], width), dtype=comp)
        for i in range(acc["count"]):
            out[i] = np.frombuffer(data, dtype=comp, count=width, offset=offset + i * stride)
        return out

    meshes = {}
    for node in gltf["nodes"]:
        if "mesh" not in node:
            continue
        # 节点名像 sight_RPG7_0 / RPG7_RPG7_0 / rocket_RPG7Rocket_0 ：去掉末尾的_材质名_0
        mesh = gltf["meshes"][node["mesh"]]
        name = node.get("name", mesh.get("name", "")).rsplit("_", 2)[0]
        prim = mesh["primitives"][0]
        meshes[name] = {
            "pos": accessor(prim["attributes"]["POSITION"]).astype(np.float64),
            "nor": accessor(prim["attributes"]["NORMAL"]).astype(np.float64),
            "uv": accessor(prim["attributes"]["TEXCOORD_0"]).astype(np.float64),
            "idx": accessor(prim["indices"]).reshape(-1).astype(np.uint32),
        }
    return meshes


def to_local(points, origin, forward_vec, right_vec, up_vec, is_normal=False):
    """glTF点 -> 以origin为原点的(前, 右, 上)坐标，再映射回glTF轴(使 前=FORWARD_TO_GLTF 方向)。"""
    rel = points if is_normal else points - origin
    f = rel @ forward_vec
    r = rel @ right_vec
    u = rel @ up_vec
    out = np.outer(f, FORWARD_TO_GLTF) + np.outer(r, RIGHT_TO_GLTF) + np.outer(u, UP_TO_GLTF)
    return out * (1.0 if is_normal else SCALE)


def merge(parts):
    pos, nor, uv, idx = [], [], [], []
    base = 0
    for p in parts:
        pos.append(p["pos"]); nor.append(p["nor"]); uv.append(p["uv"])
        idx.append(p["idx"] + base)
        base += len(p["pos"])
    return {"pos": np.vstack(pos), "nor": np.vstack(nor), "uv": np.vstack(uv), "idx": np.concatenate(idx)}


def cut_rocket(rocket, cut_y):
    """丢掉y>cut_y(尾部)的三角形，在切口处补一个朝尾方向(+y)的封口扇形。"""
    pos, idx = rocket["pos"], rocket["idx"].reshape(-1, 3)
    keep = np.all(pos[idx][:, :, 1] <= cut_y + 0.05, axis=1)
    idx = idx[keep]
    used = np.unique(idx)
    remap = np.full(len(pos), -1, dtype=np.int64)
    remap[used] = np.arange(len(used))
    out = {k: rocket[k][used] for k in ("pos", "nor", "uv")}
    out["idx"] = remap[idx].reshape(-1).astype(np.uint32)

    ring = np.where(np.abs(out["pos"][:, 1] - cut_y) < 0.2)[0]
    center = out["pos"][ring].mean(axis=0)
    # 圈上的点按绕轴角度排序，去掉位置重复的(UV接缝处同一位置有多个顶点)
    angles = np.arctan2(out["pos"][ring, 2] - center[2], out["pos"][ring, 0] - center[0])
    order = ring[np.argsort(angles)]
    unique, last = [], None
    for i in order:
        if last is None or np.linalg.norm(out["pos"][i] - out["pos"][last]) > 1e-3:
            unique.append(i); last = i
    cap_center = len(out["pos"])
    out["pos"] = np.vstack([out["pos"], center])
    out["nor"] = np.vstack([out["nor"], [0.0, 1.0, 0.0]])
    out["uv"] = np.vstack([out["uv"], out["uv"][unique[0]]])
    tris = []
    for a, b in zip(unique, unique[1:] + unique[:1]):
        tris.extend([cap_center, b, a])     # 从+y看逆时针=法线朝+y
    out["idx"] = np.concatenate([out["idx"], np.array(tris, dtype=np.uint32)])
    return out, center


def write_glb(path, mesh, material_name):
    pos = mesh["pos"].astype(np.float32)
    nor = mesh["nor"].astype(np.float32)
    uv = mesh["uv"].astype(np.float32)
    idx = mesh["idx"].astype(np.uint32)
    blobs = [pos.tobytes(), nor.tobytes(), uv.tobytes(), idx.tobytes()]
    offsets, total = [], 0
    for b in blobs:
        offsets.append(total)
        total += len(b)
    gltf = {
        "asset": {"version": "2.0", "generator": "split_rpg7.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": material_name}],
        "meshes": [{"name": material_name, "primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0, "mode": 4}]}],
        "materials": [{"name": material_name, "doubleSided": True,
                       "pbrMetallicRoughness": {"metallicFactor": 0.0, "roughnessFactor": 0.8}}],
        "buffers": [{"byteLength": total}],
        "bufferViews": [{"buffer": 0, "byteOffset": offsets[i], "byteLength": len(blobs[i]), "target": 34962 if i < 3 else 34963}
                        for i in range(4)],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(pos), "type": "VEC3",
             "min": pos.min(axis=0).tolist(), "max": pos.max(axis=0).tolist()},
            {"bufferView": 1, "componentType": 5126, "count": len(nor), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": len(uv), "type": "VEC2"},
            {"bufferView": 3, "componentType": 5125, "count": len(idx), "type": "SCALAR"},
        ],
    }
    js = json.dumps(gltf).encode("utf-8")
    js += b" " * ((4 - len(js) % 4) % 4)
    bin_data = b"".join(blobs)
    bin_data += b"\0" * ((4 - len(bin_data) % 4) % 4)
    out = struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(bin_data))
    out += struct.pack("<II", len(js), 0x4E4F534A) + js
    out += struct.pack("<II", len(bin_data), 0x004E4942) + bin_data
    open(path, "wb").write(out)


def main():
    src, out_dir = sys.argv[1], sys.argv[2]
    meshes = load_glb(src)

    # 发射器：sight/sight_adjust从爆炸图位置挪回，再合并。局部坐标：前=-y(枪口方向)，上=+z，右=+x；原点=尾端枪管轴线。
    for name in ("sight", "sight_adjust"):
        meshes[name]["pos"] = meshes[name]["pos"] + np.array([SIGHT_SHIFT_X, 0.0, 0.0])
    launcher = merge([meshes["RPG7"], meshes["sight"], meshes["sight_adjust"], meshes["hammer"]])
    origin = np.array([LAUNCHER_AXIS[0], LAUNCHER_REAR_Y, LAUNCHER_AXIS[1]])
    fwd, right, up = np.array([0, -1.0, 0]), np.array([-1.0, 0, 0]), np.array([0, 0, 1.0])  # 原始坐标：前=-y，上=+z，右=前x上=-x
    launcher_out = {
        "pos": to_local(launcher["pos"], origin, fwd, right, up),
        "nor": to_local(launcher["nor"], origin, fwd, right, up, is_normal=True),
        "uv": launcher["uv"], "idx": launcher["idx"],
    }
    write_glb(out_dir + "/SM_RPG7_Launcher.glb", launcher_out, "RPG7")

    # 火箭弹：平切+封口，原点=切口(尾端)轴线中心，前=-y(弹头方向)。
    rocket, cap_center = cut_rocket(meshes["rocket"], CUT_Y)
    origin = np.array([ROCKET_AXIS[0], CUT_Y, ROCKET_AXIS[1]])
    rocket_out = {
        "pos": to_local(rocket["pos"], origin, fwd, right, up),
        "nor": to_local(rocket["nor"], origin, fwd, right, up, is_normal=True),
        "uv": rocket["uv"], "idx": rocket["idx"],
    }
    write_glb(out_dir + "/SM_RPG7_Rocket.glb", rocket_out, "RPG7Rocket")

    for name, m in (("launcher", launcher_out), ("rocket", rocket_out)):
        print(name, "verts", len(m["pos"]), "tris", len(m["idx"]) // 3,
              "min", m["pos"].min(axis=0).round(2).tolist(), "max", m["pos"].max(axis=0).round(2).tolist())


if __name__ == "__main__":
    main()
