# 第2步：把 export_traffic_light.py 导出的 retro_traffic_light.obj 拆成框架和红/黄/绿三盏灯，各写一个glb：
#   SM_TrafficLight_Frame.glb  灯杆、灯箱、遮光罩、按钮盒等(用原来的材质和贴图)
#   SM_TrafficLight_Red.glb / _Yellow.glb / _Green.glb   三个灯罩圆片(各自用发光材质，见 import_traffic_light.py)
# 纯python+numpy+PIL，不需要Blender。用法：
#   python Tools/TrafficLight/split_traffic_light.py <导出目录> <输出目录>
#
# 怎么认灯：网格按顶点位置焊接后求连通分量(原模型由5个节点合并而成，零件之间不相连，实测21个分量)。
# 在每个分量三角形的UV中心采样底色贴图，取平均颜色：偏红/偏黄/偏绿、而且是平面圆片的那三个分量就是灯罩；
# 颜色分不清时按高度兜底(上红、中黄、下绿)。其余全部归框架。脚本会打印每个分量的归类，核对用。
#
# 坐标：UE的OBJ导出写的是(X, Z, Y)(UE厘米)，UE导入glTF按 UE=(gx, gz, gy)*100，所以 glTF坐标 = OBJ坐标/100，
# 原点和朝向和原模型完全一样(灯杆底部在原点附近，灯罩朝UE的-X)。OBJ的v朝上，glTF/UE的v朝下：v取1-v。
# OBJ导出不带法线，这里按三角形算平面法线写进去(灯罩是平面，框架是低模，平面法线足够)。
import json
import os
import struct
import sys
from collections import Counter

import numpy as np
from PIL import Image

LAMPS = ("Red", "Yellow", "Green")


def load_obj(path):
    positions, uvs, faces = [], [], []
    for line in open(path):
        if line.startswith("v "):
            positions.append([float(x) for x in line.split()[1:4]])
        elif line.startswith("vt "):
            uvs.append([float(x) for x in line.split()[1:3]])
        elif line.startswith("f "):
            faces.append([[int(p) - 1 for p in token.split("/")[:2]] for token in line.split()[1:4]])
    return np.array(positions), np.array(uvs), np.array(faces)  # faces[t, corner, (position, uv)]


def components(positions, faces):
    key = np.round(positions * 1000).astype(np.int64)
    _, welded = np.unique(key, axis=0, return_inverse=True)
    welded = welded.reshape(-1)
    parent = list(range(welded.max() + 1))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for t in range(len(faces)):
        a, b, c = (find(welded[faces[t, k, 0]]) for k in range(3))
        parent[a] = b
        parent[find(c)] = find(b)
    return np.array([find(welded[faces[t, 0, 0]]) for t in range(len(faces))])


def classify(positions, uvs, faces, comp_of_tri, texture):
    image = np.asarray(Image.open(texture).convert("RGB")).astype(np.float32) / 255.0
    height, width = image.shape[:2]
    info = {}
    for comp in set(comp_of_tri.tolist()):
        tris = np.where(comp_of_tri == comp)[0]
        centers = uvs[faces[tris][:, :, 1]].mean(axis=1)
        xs = np.clip((centers[:, 0] % 1.0 * width).astype(int), 0, width - 1)
        ys = np.clip(((1.0 - centers[:, 1] % 1.0) * height).astype(int), 0, height - 1)
        color = image[ys, xs].mean(axis=0)
        pts = positions[faces[tris][:, :, 0].reshape(-1)]
        extent = pts.max(axis=0) - pts.min(axis=0)
        flat = extent.min() < 0.05 * max(extent.max(), 1e-6)
        info[comp] = {"tris": tris, "color": color, "min": pts.min(axis=0), "max": pts.max(axis=0), "flat": flat}

    def hue_of(c):
        r, g, b = c
        if r > g * 1.6 and r > b * 1.6:
            return "Red"
        if r > b * 1.6 and g > b * 1.6 and abs(r - g) < 0.25 * max(r, g):
            return "Yellow"
        if g > r * 1.4 and g > b * 1.4:
            return "Green"
        return None

    lamps = {}
    for comp, entry in info.items():
        hue = hue_of(entry["color"])
        if entry["flat"] and hue and hue not in lamps:
            lamps[hue] = comp
    if len(lamps) != 3:
        # 颜色分不开：取最扁平且颜色最饱和的三个圆片，按高度(OBJ的y=UE的Z)从上到下分红黄绿。
        flats = [c for c, e in info.items() if e["flat"]]
        flats.sort(key=lambda c: -(info[c]["color"].max() - info[c]["color"].min()))
        chosen = sorted(flats[:3], key=lambda c: -info[c]["max"][1])
        lamps = dict(zip(LAMPS, chosen))
        print("WARNING: lamp colors not distinguishable, fell back to height order")
    return info, lamps


def flat_normals(pos):
    tri = pos.reshape(-1, 3, 3)
    n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-12
    return np.repeat(n, 3, axis=0)


def build(positions, uvs, faces, tris):
    pos = positions[faces[tris][:, :, 0].reshape(-1)] / 100.0
    uv = uvs[faces[tris][:, :, 1].reshape(-1)].copy()
    uv[:, 1] = 1.0 - uv[:, 1]
    return {"pos": pos, "nor": flat_normals(pos), "uv": uv, "idx": np.arange(len(pos))}


def write_glb(path, mesh, material_name):
    pos = mesh["pos"].astype(np.float32)
    nor = mesh["nor"].astype(np.float32)
    uv = mesh["uv"].astype(np.float32)
    idx = mesh["idx"].astype(np.uint32)
    blobs = [pos.tobytes(), nor.tobytes(), uv.tobytes(), idx.tobytes()]
    offsets, total = [], 0
    for blob in blobs:
        offsets.append(total)
        total += len(blob)
    gltf = {
        "asset": {"version": "2.0", "generator": "split_traffic_light.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": material_name}],
        "meshes": [{"name": material_name, "primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0, "mode": 4}]}],
        "materials": [{"name": material_name, "pbrMetallicRoughness": {"metallicFactor": 0.0, "roughnessFactor": 0.8}}],
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
    os.makedirs(out_dir, exist_ok=True)
    positions, uvs, faces = load_obj(os.path.join(src, "retro_traffic_light.obj"))
    comp_of_tri = components(positions, faces)
    info, lamps = classify(positions, uvs, faces, comp_of_tri, os.path.join(src, "retro_traffic_light_texture_0.png"))
    lamp_comps = set(lamps.values())
    print("components: %d, triangles: %d" % (len(info), len(faces)))
    for comp, entry in sorted(info.items(), key=lambda kv: -len(kv[1]["tris"])):
        label = next((name for name, c in lamps.items() if c == comp), "Frame")
        mn, mx, col = entry["min"], entry["max"], entry["color"]
        # 打印成UE坐标(厘米)：UE=(ox, oz, oy)
        print("  %-6s tris %5d  UE min(%7.2f %7.2f %7.2f) max(%7.2f %7.2f %7.2f)  rgb(%.2f %.2f %.2f)%s" % (
            label, len(entry["tris"]), mn[0], mn[2], mn[1], mx[0], mx[2], mx[1], col[0], col[1], col[2], "  flat" if entry["flat"] else ""))

    frame_tris = np.where(~np.isin(comp_of_tri, list(lamp_comps)))[0]
    write_glb(os.path.join(out_dir, "SM_TrafficLight_Frame.glb"), build(positions, uvs, faces, frame_tris), "TrafficLightFrame")
    for name in LAMPS:
        tris = info[lamps[name]]["tris"]
        mesh = build(positions, uvs, faces, tris)
        write_glb(os.path.join(out_dir, "SM_TrafficLight_%s.glb" % name), mesh, "TrafficLamp" + name)
        n = mesh["nor"].mean(axis=0)
        # glTF法线(gx,gy,gz) -> UE(gx, gz, gy)
        print("  lamp %-6s facing UE (%.2f %.2f %.2f), center UE cm (%.1f %.1f %.1f)" % (
            name, n[0], n[2], n[1], mesh["pos"][:, 0].mean() * 100, mesh["pos"][:, 2].mean() * 100, mesh["pos"][:, 1].mean() * 100))
    print("frame triangles: %d" % len(frame_tris))


if __name__ == "__main__":
    main()
