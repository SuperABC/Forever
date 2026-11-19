# 生成住宅家具的低模网格(纯python+numpy，不需要Blender)：
#   <out>/<名字>.glb                 每件一个glb(几何+UV，不带材质/贴图，原点=地面中心，+X=家具正面朝向，单位米)
#   <out>/palette.png                64x64调色板贴图(16x16个色块，每块4x4像素)，所有家具共用，UV指向色块中心
#   <out>/furniture.json             名字 -> 包围盒(米)、面数，核对用
#   Source/Basic/map/room_furniture_catalog.h   (--header指定路径时写)家具尺寸表，room_residence.cpp据此布局
# 用法： python Tools/Furniture/make_furniture.py <out_dir> [--header Source/Basic/map/room_furniture_catalog.h]
# 然后：  UnrealEditor-Cmd.exe Forever.uproject -ExecutePythonScript="<abs>/Tools/Furniture/import_furniture.py" (环境变量 FURNITURE_DIR=<out_dir>)
#
# 坐标约定(家具局部，米)：X=前(家具正面)，Y=右，Z=上；原点在底面中心。每件由若干长方体拼成(12面/件盒)，
# 平面法线。UE导入映射和Tools/split_rpg7.py一致：gltf(x,y,z) = (前, 上, 右)，所以 UE=(gx,gz,gy)*100。
# 材质：所有家具用同一个材质M_Furniture(调色板贴图)，颜色/款式变化靠不同mesh变体(A/B/C)和布局。
import json
import os
import struct
import sys

import numpy as np

# ---------------------------------------------------------------- 调色板
PALETTE = [
    # 木色：蜂蜜橡木 / 胡桃 / 深胡桃 / 白桦
    ("wood_light", (214, 160, 98)), ("wood_mid", (158, 102, 56)), ("wood_dark", (92, 56, 32)), ("wood_birch", (232, 200, 150)),
    ("white", (240, 240, 238)), ("offwhite", (236, 226, 204)), ("grey_light", (196, 198, 204)),
    ("grey", (128, 130, 136)), ("grey_dark", (66, 68, 74)), ("black", (28, 28, 32)), ("metal", (176, 180, 188)),
    # 布艺
    ("fabric_grey", (118, 124, 136)), ("fabric_beige", (218, 188, 140)), ("fabric_blue", (44, 92, 156)),
    ("fabric_green", (60, 122, 76)), ("fabric_red", (172, 44, 46)), ("fabric_brown", (116, 74, 48)),
    ("fabric_mustard", (218, 164, 36)), ("fabric_teal", (36, 128, 132)), ("fabric_plum", (110, 56, 100)),
    # 床品
    ("linen_white", (244, 242, 236)), ("linen_blue", (118, 158, 214)), ("linen_pink", (232, 142, 166)),
    ("linen_green", (136, 190, 146)), ("linen_grey", (186, 188, 196)),
    ("blanket_navy", (36, 60, 112)), ("blanket_rose", (196, 84, 104)), ("blanket_teal", (40, 138, 140)), ("blanket_sage", (130, 160, 110)),
    # 卫浴/植物
    ("ceramic", (248, 248, 246)), ("water", (86, 160, 208)), ("plant_green", (56, 146, 56)), ("plant_dark", (34, 96, 44)), ("pot", (196, 94, 52)),
    # 书/屏幕
    ("book_red", (180, 50, 46)), ("book_blue", (50, 88, 160)), ("book_yellow", (226, 184, 52)), ("book_green", (60, 134, 86)),
    ("tv_screen", (20, 22, 28)),
    # 台面/橱柜/电器
    ("counter_stone", (230, 224, 208)), ("counter_dark", (58, 58, 66)), ("handle", (186, 188, 196)),
    ("cabinet_cream", (236, 222, 188)), ("cabinet_blue", (74, 106, 150)), ("cabinet_green", (110, 150, 120)),
    ("fridge_mint", (150, 206, 180)), ("fridge_cream", (238, 226, 196)),
    # 地毯/灯
    ("rug_red", (158, 42, 48)), ("rug_blue", (44, 76, 140)), ("rug_grey", (152, 152, 156)), ("rug_green", (70, 120, 80)), ("rug_ochre", (196, 128, 60)),
    ("lamp_shade", (250, 226, 150)), ("burner", (50, 50, 54)),
]
COLOR = {name: i for i, (name, _) in enumerate(PALETTE)}


def write_palette(path):
    from PIL import Image
    # 16x16个色块，每块4x4像素：贴图会被块压缩(4x4一块)时相邻色块互相污染，色块<4像素颜色会糊成灰色，
    # 所以色块至少占一个压缩块(导入时同时设为不压缩)。UV仍指向色块中心。
    cell = 4
    img = Image.new("RGB", (16 * cell, 16 * cell), (255, 0, 255))
    for i, (_, rgb) in enumerate(PALETTE):
        for dy in range(cell):
            for dx in range(cell):
                img.putpixel(((i % 16) * cell + dx, (i // 16) * cell + dy), rgb)
    img.save(path)


def swatch_uv(color):
    i = COLOR[color]
    return ((i % 16 + 0.5) / 16.0, (i // 16 + 0.5) / 16.0)


# ---------------------------------------------------------------- 几何
class Mesh:
    def __init__(self):
        self.boxes = []  # (x0,x1,y0,y1,z0,z1,color) 家具局部系

    def box(self, x0, x1, y0, y1, z0, z1, color):
        assert x1 > x0 and y1 > y0 and z1 > z0, (x0, x1, y0, y1, z0, z1)
        assert color in COLOR, color
        self.boxes.append((x0, x1, y0, y1, z0, z1, color))

    def boxc(self, cx, cy, z0, sx, sy, sz, color):  # 中心(cx,cy)、底面高度z0、尺寸
        self.box(cx - sx / 2, cx + sx / 2, cy - sy / 2, cy + sy / 2, z0, z0 + sz, color)

    def bounds(self):
        b = np.array([[x0, y0, z0, x1, y1, z1] for x0, x1, y0, y1, z0, z1, _ in self.boxes])
        return b[:, :3].min(axis=0), b[:, 3:].max(axis=0)

    def recenter(self):
        """把X/Y包围盒中心挪到原点。布局按"原点居中的矩形"摆放，网格不居中(比如马桶水箱在背面)会让背面穿出墙外。"""
        lo, hi = self.bounds()
        cx, cy = (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2
        self.boxes = [(x0 - cx, x1 - cx, y0 - cy, y1 - cy, z0, z1, c) for x0, x1, y0, y1, z0, z1, c in self.boxes]

    def to_arrays(self):
        pos, nor, uv, idx = [], [], [], []
        for x0, x1, y0, y1, z0, z1, color in self.boxes:
            u, v = swatch_uv(color)
            # 6个面，局部(f,r,u)轴 -> gltf(x=f, y=u, z=r)。每面：法线 + 4个角(任意顺序，下面按法线定绕向)。
            faces = [
                ((1, 0, 0), [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)]),
                ((-1, 0, 0), [(x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1)]),
                ((0, 1, 0), [(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)]),
                ((0, -1, 0), [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)]),
                ((0, 0, 1), [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]),
                ((0, 0, -1), [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)]),
            ]
            for n, corners in faces:
                g = lambda p: (p[0], p[2], p[1])  # (f,r,u)->(gx,gy,gz)=(f,u,r)
                gc = [g(c) for c in corners]
                gn = g(n)
                a, b, c = (np.array(gc[0]), np.array(gc[1]), np.array(gc[2]))
                order = [0, 1, 2, 3]
                if np.dot(np.cross(b - a, c - a), gn) < 0:  # 保证从外面看逆时针(glTF约定)
                    order = [0, 3, 2, 1]
                base = len(pos)
                for k in order:
                    pos.append(gc[k]); nor.append(gn); uv.append((u, v))
                idx += [base, base + 1, base + 2, base, base + 2, base + 3]
        return (np.array(pos, np.float32), np.array(nor, np.float32), np.array(uv, np.float32), np.array(idx, np.uint32))


def write_glb(path, mesh, name):
    pos, nor, uv, idx = mesh.to_arrays()
    blobs = [pos.tobytes(), nor.tobytes(), uv.tobytes(), idx.tobytes()]
    offsets, total = [], 0
    for b in blobs:
        offsets.append(total)
        total += len(b)
    gltf = {
        "asset": {"version": "2.0", "generator": "make_furniture.py"},
        "scene": 0, "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": name}],
        "meshes": [{"name": name, "primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0, "mode": 4}]}],
        "materials": [{"name": "M_Furniture", "doubleSided": True,
                       "pbrMetallicRoughness": {"metallicFactor": 0.0, "roughnessFactor": 0.85}}],
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


# ---------------------------------------------------------------- 家具(米；-X=背面靠墙，+X=正面)
def bed(w, frame, linen, blanket, pillow="linen_white", length=2.1):
    """床：床头在-X，床脚朝+X。w=床宽(Y)。"""
    m = Mesh()
    d = length
    m.box(-d / 2, d / 2, -w / 2, w / 2, 0.10, 0.28, frame)                      # 床架
    m.box(-d / 2 + 0.03, d / 2 - 0.03, -w / 2 + 0.03, w / 2 - 0.03, 0.28, 0.50, linen)  # 床垫
    m.box(-d / 2, -d / 2 + 0.07, -w / 2, w / 2, 0.10, 1.05, frame)               # 床头板
    m.box(d / 2 - 0.05, d / 2, -w / 2, w / 2, 0.10, 0.45, frame)                 # 床尾板
    for sy in (-1, 1):                                                           # 床腿
        for sx in (-1, 1):
            m.boxc(sx * (d / 2 - 0.04), sy * (w / 2 - 0.04), 0.0, 0.07, 0.07, 0.10, frame)
    m.box(-0.15, d / 2 - 0.08, -w / 2 + 0.02, w / 2 - 0.02, 0.50, 0.57, blanket)  # 被子
    n = 2 if w > 1.4 else 1
    for i in range(n):                                                           # 枕头
        cy = ((i + 0.5) / n - 0.5) * (w - 0.1)
        m.boxc(-d / 2 + 0.30, cy, 0.50, 0.40, (w - 0.2) / n - 0.06, 0.10, pillow)
    return m


def nightstand(body, top):
    m = Mesh()
    m.boxc(0, 0, 0.05, 0.38, 0.42, 0.40, body)
    m.boxc(0, 0, 0.45, 0.42, 0.46, 0.04, top)
    m.boxc(0.19, 0, 0.30, 0.02, 0.18, 0.03, "handle")
    for sx in (-1, 1):
        for sy in (-1, 1):
            m.boxc(sx * 0.17, sy * 0.19, 0.0, 0.04, 0.04, 0.05, "wood_dark")
    return m


def wardrobe(body, door, w=1.6, h=2.1, d=0.6):
    m = Mesh()
    m.boxc(0, 0, 0.0, d, w, h, body)
    nd = 2 if w < 2.0 else 3
    for i in range(nd):
        cy = ((i + 0.5) / nd - 0.5) * (w - 0.04)
        m.boxc(d / 2 + 0.01, cy, 0.06, 0.02, (w - 0.04) / nd - 0.02, h - 0.12, door)
        hy = cy + (0.07 if i % 2 == 0 else -0.07)
        m.boxc(d / 2 + 0.03, hy, h * 0.45, 0.03, 0.02, 0.22, "handle")
    return m


def desk(top, leg, w=1.2, d=0.6):
    m = Mesh()
    m.boxc(0, 0, 0.72, d, w, 0.04, top)
    for sx in (-1, 1):
        for sy in (-1, 1):
            m.boxc(sx * (d / 2 - 0.04), sy * (w / 2 - 0.04), 0.0, 0.05, 0.05, 0.72, leg)
    m.boxc(-0.02, w / 2 - 0.22, 0.30, d - 0.1, 0.40, 0.42, top)                  # 抽屉柜
    return m


def chair(seat, frame, back_h=0.45):
    m = Mesh()
    m.boxc(0, 0, 0.42, 0.44, 0.44, 0.05, seat)
    m.boxc(-0.20, 0, 0.47, 0.04, 0.42, back_h, seat)
    for sx in (-1, 1):
        for sy in (-1, 1):
            m.boxc(sx * 0.19, sy * 0.19, 0.0, 0.04, 0.04, 0.42, frame)
    return m


def sofa(body, cushion, seats, w_per=0.65):
    m = Mesh()
    w = seats * w_per + 0.30
    d = 0.90
    m.boxc(0, 0, 0.08, d, w, 0.30, body)                                         # 底座
    m.boxc(-d / 2 + 0.10, 0, 0.38, 0.20, w, 0.50, body)                          # 靠背
    for sy in (-1, 1):                                                           # 扶手
        m.boxc(0, sy * (w / 2 - 0.075), 0.38, d - 0.06, 0.15, 0.24, body)
    for i in range(seats):                                                       # 坐垫
        cy = (i - (seats - 1) / 2) * w_per
        m.boxc(0.06, cy, 0.38, d - 0.34, w_per - 0.03, 0.12, cushion)
    for sx in (-1, 1):
        for sy in (-1, 1):
            m.boxc(sx * (d / 2 - 0.06), sy * (w / 2 - 0.06), 0.0, 0.06, 0.06, 0.08, "wood_dark")
    return m


def armchair(body, cushion):
    m = sofa(body, cushion, 1, 0.65)
    return m


def coffee_table(top, leg, w=1.0, d=0.55):
    m = Mesh()
    m.boxc(0, 0, 0.36, d, w, 0.04, top)
    for sx in (-1, 1):
        for sy in (-1, 1):
            m.boxc(sx * (d / 2 - 0.04), sy * (w / 2 - 0.04), 0.0, 0.05, 0.05, 0.36, leg)
    return m


def tv_stand(body, w=1.5):
    m = Mesh()
    m.boxc(0, 0, 0.0, 0.40, w, 0.45, body)
    m.boxc(0.205, 0, 0.18, 0.01, w / 2 - 0.05, 0.14, "wood_dark")
    m.boxc(0, 0, 0.45, 0.20, 0.50, 0.03, "black")                                # 底座
    m.boxc(0, 0, 0.48, 0.05, 0.08, 0.10, "black")
    m.boxc(0.0, 0, 0.58, 0.05, w * 0.62, w * 0.62 * 0.5625, "black")           # 电视(16:9)
    m.boxc(0.026, 0, 0.60, 0.01, w * 0.62 - 0.06, w * 0.62 * 0.5625 - 0.04, "tv_screen")
    return m


def dining_table(top, leg, w=1.5, d=0.9):
    m = Mesh()
    m.boxc(0, 0, 0.72, d, w, 0.05, top)
    for sx in (-1, 1):
        for sy in (-1, 1):
            m.boxc(sx * (d / 2 - 0.06), sy * (w / 2 - 0.06), 0.0, 0.07, 0.07, 0.72, leg)
    return m


def fridge(body, door=None):
    m = Mesh()
    door = door or body
    m.boxc(0, 0, 0.0, 0.70, 0.70, 1.80, body)
    m.boxc(0.355, 0, 1.15, 0.01, 0.68, 0.01, "grey_dark")                          # 上下门缝
    m.boxc(0.36, -0.28, 1.25, 0.03, 0.03, 0.35, "handle")
    m.boxc(0.36, -0.28, 0.60, 0.03, 0.03, 0.40, "handle")
    return m


def counter(body, top, w=1.0):
    m = Mesh()
    m.boxc(0, 0, 0.0, 0.58, w, 0.86, body)
    m.boxc(0.01, 0, 0.86, 0.62, w + 0.01, 0.04, top)
    m.boxc(0.30, 0, 0.62, 0.02, w - 0.1, 0.03, "handle")
    return m


def stove(body):
    m = Mesh()
    m.boxc(0, 0, 0.0, 0.60, 0.60, 0.86, body)
    m.boxc(0.01, 0, 0.86, 0.62, 0.62, 0.04, "grey_dark")
    for sx in (-0.13, 0.13):
        for sy in (-0.13, 0.13):
            m.boxc(sx, sy, 0.90, 0.16, 0.16, 0.02, "burner")
    m.boxc(0.305, 0, 0.30, 0.02, 0.50, 0.40, "grey_dark")                           # 烤箱门
    m.boxc(0.32, 0, 0.62, 0.03, 0.40, 0.03, "handle")
    return m


def sink_counter(body, top):
    m = Mesh()
    m.boxc(0, 0, 0.0, 0.58, 1.2, 0.86, body)
    # 台面分四块围出水槽口(别用整块台面再盖一片薄板：两个面差1cm会闪烁)，槽底在台面下4cm。
    x0, x1, y0, y1 = -0.295, 0.315, -0.605, 0.605           # 台面范围
    hx0, hx1, hy0, hy1 = -0.17, 0.23, -0.50, 0.0            # 槽口
    m.box(hx1, x1, y0, y1, 0.86, 0.90, top)
    m.box(x0, hx0, y0, y1, 0.86, 0.90, top)
    m.box(hx0, hx1, y0, hy0, 0.86, 0.90, top)
    m.box(hx0, hx1, hy1, y1, 0.86, 0.90, top)
    m.box(hx0, hx1, hy0, hy1, 0.86, 0.865, "metal")                                # 槽底
    m.boxc(-0.20, -0.25, 0.90, 0.04, 0.04, 0.25, "metal")                          # 水龙头
    m.boxc(-0.14, -0.25, 1.12, 0.16, 0.03, 0.03, "metal")
    return m


def wall_cabinet(body, w=1.0):
    m = Mesh()
    m.boxc(0, 0, 0.0, 0.35, w, 0.70, body)
    m.boxc(0.18, 0, 0.05, 0.02, w - 0.04, 0.60, "handle")
    return m


def toilet():
    m = Mesh()
    m.boxc(-0.22, 0, 0.0, 0.18, 0.38, 0.78, "ceramic")                             # 水箱
    m.boxc(0.02, 0, 0.0, 0.40, 0.34, 0.40, "ceramic")                              # 座
    m.boxc(0.04, 0, 0.40, 0.38, 0.38, 0.04, "white")
    return m


def washbasin():
    m = Mesh()
    m.boxc(-0.10, 0, 0.0, 0.14, 0.14, 0.75, "ceramic")                             # 立柱
    # 盆：实心底座+四圈盆沿，水面落在盆沿下1.5cm(水面和盆沿顶面同高会闪烁)。
    m.boxc(0, 0, 0.75, 0.46, 0.56, 0.09, "ceramic")
    t = 0.05
    m.boxc(0, -0.28 + t / 2, 0.84, 0.46, t, 0.03, "ceramic")
    m.boxc(0, 0.28 - t / 2, 0.84, 0.46, t, 0.03, "ceramic")
    m.boxc(-0.23 + t / 2, 0, 0.84, t, 0.56 - 2 * t, 0.03, "ceramic")
    m.boxc(0.23 - t / 2, 0, 0.84, t, 0.56 - 2 * t, 0.03, "ceramic")
    m.box(-0.23 + t, 0.23 - t, -0.28 + t, 0.28 - t, 0.84, 0.855, "water")
    m.boxc(-0.20, 0, 0.87, 0.04, 0.04, 0.16, "metal")
    return m


def bathtub():
    m = Mesh()
    d, w, h, t = 0.75, 1.60, 0.55, 0.07
    m.boxc(0, 0, 0.0, d, w, 0.10, "ceramic")
    m.boxc(0, -w / 2 + t / 2, 0.0, d, t, h, "ceramic")
    m.boxc(0, w / 2 - t / 2, 0.0, d, t, h, "ceramic")
    m.boxc(-d / 2 + t / 2, 0, 0.0, t, w, h, "ceramic")
    m.boxc(d / 2 - t / 2, 0, 0.0, t, w, h, "ceramic")
    m.boxc(0, 0, 0.10, d - 2 * t - 0.02, w - 2 * t - 0.02, 0.02, "water")  # 四周缩1cm，不和池壁内面共面
    m.boxc(-d / 2 + 0.01, w / 2 - 0.25, h, 0.04, 0.04, 0.20, "metal")
    return m


def shower():
    m = Mesh()
    m.boxc(0, 0, 0.0, 0.90, 0.90, 0.06, "ceramic")                                 # 盆
    m.boxc(-0.42, 0, 0.06, 0.04, 0.04, 2.00, "metal")                              # 立管
    m.boxc(-0.30, 0, 2.00, 0.30, 0.04, 0.04, "metal")
    m.boxc(-0.25, 0, 1.95, 0.14, 0.14, 0.04, "metal")                              # 花洒
    return m


def washing_machine():
    m = Mesh()
    m.boxc(0, 0, 0.0, 0.60, 0.60, 0.85, "white")
    m.boxc(0.305, 0, 0.30, 0.02, 0.38, 0.38, "grey_dark")
    m.boxc(0.305, 0, 0.74, 0.02, 0.50, 0.08, "grey_light")
    return m


def bookshelf(body, w=0.9, h=1.8, spines=("book_red", "book_blue", "book_yellow", "book_green", "white")):
    m = Mesh()
    d = 0.30
    m.boxc(-d / 2 + 0.01, 0, 0.0, 0.02, w, h, body)                                # 背板
    m.boxc(0, -w / 2 + 0.02, 0.0, d, 0.04, h, body)
    m.boxc(0, w / 2 - 0.02, 0.0, d, 0.04, h, body)
    levels = 5
    for i in range(levels + 1):
        m.boxc(0, 0, i * (h - 0.04) / levels, d, w, 0.04, body)
    k = 0
    for i in range(1, levels):  # 每层3~4块"书堆"(整块着色，不逐本建模，省面数)
        z0 = i * (h - 0.04) / levels + 0.04
        y = -w / 2 + 0.07
        for j in range(3 + (i % 2)):
            bw = min(0.14 + 0.05 * ((k * 7) % 3), w / 2 - 0.07 - y - 0.02)
            if bw < 0.06:
                break
            bh = 0.22 + 0.03 * ((k * 5) % 4)
            m.boxc(0.0, y + bw / 2, z0, 0.2, bw, bh, spines[k % len(spines)])
            y += bw + 0.12 * (w / 0.9)
            k += 1
    return m


def rug(color, w=1.4, d=2.0, edge="offwhite"):
    m = Mesh()
    m.boxc(0, 0, 0.0, d, w, 0.012, color)
    m.boxc(0, 0, 0.012, d - 0.2, w - 0.2, 0.004, edge)
    return m


def plant(leaf, pot="pot"):
    m = Mesh()
    m.boxc(0, 0, 0.0, 0.32, 0.32, 0.30, pot)
    m.boxc(0, 0, 0.30, 0.04, 0.04, 0.35, "wood_dark")
    for i, (dx, dy, z, s) in enumerate([(0, 0, 0.62, 0.34), (0.10, 0.08, 0.80, 0.26), (-0.08, -0.10, 0.92, 0.22), (0.02, -0.04, 1.06, 0.16)]):
        m.boxc(dx, dy, z, s, s, s * 0.7, leaf if i % 2 == 0 else "plant_dark")
    return m


def floor_lamp():
    m = Mesh()
    m.boxc(0, 0, 0.0, 0.26, 0.26, 0.03, "grey_dark")
    m.boxc(0, 0, 0.03, 0.03, 0.03, 1.40, "grey_dark")
    m.boxc(0, 0, 1.40, 0.30, 0.30, 0.26, "lamp_shade")
    return m


def shoe_cabinet(body):
    m = Mesh()
    m.boxc(0, 0, 0.0, 0.32, 0.90, 0.95, body)
    m.boxc(0.165, 0, 0.05, 0.01, 0.84, 0.85, "grey_light")
    m.boxc(0.17, 0.0, 0.50, 0.02, 0.02, 0.20, "handle")
    return m


def dresser(body, top):
    m = Mesh()
    m.boxc(0, 0, 0.0, 0.45, 1.0, 0.85, body)
    m.boxc(0.01, 0, 0.85, 0.48, 1.04, 0.04, top)
    for i in range(3):
        m.boxc(0.23, 0, 0.10 + i * 0.25, 0.02, 0.9, 0.22, "wood_dark")
        m.boxc(0.25, 0, 0.18 + i * 0.25, 0.02, 0.2, 0.03, "handle")
    return m


def build_catalog():
    c = {}
    c["Bed_Double_A"] = bed(1.8, "wood_mid", "linen_white", "blanket_navy")
    c["Bed_Double_B"] = bed(1.8, "wood_light", "linen_white", "blanket_rose", "linen_white")
    c["Bed_Double_C"] = bed(1.5, "wood_dark", "linen_white", "blanket_teal")
    c["Bed_Single_A"] = bed(1.0, "wood_light", "linen_white", "blanket_sage", length=2.0)
    c["Bed_Single_B"] = bed(1.0, "wood_mid", "linen_white", "linen_blue", length=2.0)
    c["Nightstand_A"] = nightstand("wood_light", "wood_mid")
    c["Nightstand_B"] = nightstand("wood_birch", "wood_light")
    c["Wardrobe_A"] = wardrobe("wood_light", "wood_mid")
    c["Wardrobe_B"] = wardrobe("wood_birch", "wood_light", w=2.0)
    c["Wardrobe_C"] = wardrobe("wood_dark", "wood_mid", w=1.2)
    c["Desk_A"] = desk("wood_light", "wood_dark")
    c["Desk_B"] = desk("wood_birch", "wood_dark", w=1.4)
    c["Chair_A"] = chair("fabric_teal", "wood_dark")
    c["Chair_B"] = chair("wood_mid", "wood_dark", back_h=0.40)
    c["Sofa3_A"] = sofa("fabric_teal", "fabric_beige", 3)
    c["Sofa3_B"] = sofa("fabric_blue", "linen_blue", 3)
    c["Sofa3_C"] = sofa("fabric_brown", "fabric_mustard", 3)
    c["Sofa2_A"] = sofa("fabric_plum", "fabric_beige", 2)
    c["Sofa2_B"] = sofa("fabric_green", "fabric_beige", 2)
    c["Armchair_A"] = armchair("fabric_mustard", "fabric_beige")
    c["Armchair_B"] = armchair("fabric_red", "fabric_beige")
    c["CoffeeTable_A"] = coffee_table("wood_light", "wood_dark")
    c["CoffeeTable_B"] = coffee_table("wood_dark", "black", w=1.2, d=0.6)
    c["TvStand_A"] = tv_stand("wood_mid")
    c["TvStand_B"] = tv_stand("wood_dark", w=1.8)
    c["DiningTable_A"] = dining_table("wood_light", "wood_dark")
    c["DiningTable_B"] = dining_table("wood_dark", "black", w=1.8, d=1.0)
    c["DiningChair_A"] = chair("fabric_beige", "wood_mid", back_h=0.50)
    c["DiningChair_B"] = chair("fabric_blue", "wood_dark", back_h=0.50)
    c["Fridge_A"] = fridge("fridge_cream")
    c["Fridge_B"] = fridge("fridge_mint")
    c["Counter_A"] = counter("cabinet_cream", "counter_stone")
    c["Counter_B"] = counter("cabinet_blue", "counter_dark")
    c["Stove_A"] = stove("cabinet_cream")
    c["Stove_B"] = stove("cabinet_blue")
    c["SinkCounter_A"] = sink_counter("cabinet_cream", "counter_stone")
    c["SinkCounter_B"] = sink_counter("cabinet_blue", "counter_dark")
    c["WallCabinet_A"] = wall_cabinet("cabinet_cream")
    c["WallCabinet_B"] = wall_cabinet("cabinet_blue")
    c["Toilet"] = toilet()
    c["Washbasin"] = washbasin()
    c["Bathtub"] = bathtub()
    c["Shower"] = shower()
    c["WashingMachine"] = washing_machine()
    c["Bookshelf_A"] = bookshelf("wood_light")
    c["Bookshelf_B"] = bookshelf("wood_dark", w=1.2)
    c["Rug_A"] = rug("rug_red")
    c["Rug_B"] = rug("rug_blue")
    c["Rug_C"] = rug("rug_ochre")
    c["Rug_D"] = rug("rug_green")
    c["Plant_A"] = plant("plant_green")
    c["Plant_B"] = plant("plant_green", pot="wood_mid")
    c["FloorLamp"] = floor_lamp()
    c["ShoeCabinet_A"] = shoe_cabinet("wood_light")
    c["ShoeCabinet_B"] = shoe_cabinet("wood_dark")
    c["Dresser_A"] = dresser("wood_light", "wood_mid")
    c["Dresser_B"] = dresser("wood_birch", "wood_light")
    for mesh in c.values():
        mesh.recenter()  # 布局当"原点居中的矩形"用，必须居中，否则偏出去的一侧(如马桶水箱)会穿出墙外
    return c


def write_header(path, catalog):
    lines = [
        "#pragma once",
        "",
        "// 由 Tools/Furniture/make_furniture.py 生成，不要手改(改家具就改脚本再重新生成)。",
        "// 家具包围盒尺寸(米)：depth=X(前后，-X背面/+X正面)、width=Y(左右)、height=Z。资产路径 /Game/Asset/Room/Mesh/<name>。",
        "",
        "struct RoomFurnitureDims {",
        "\tconst char* name;",
        "\tfloat depth, width, height;",
        "};",
        "",
        "static const RoomFurnitureDims kRoomFurnitureCatalog[] = {",
    ]
    for name, mesh in catalog.items():
        lo, hi = mesh.bounds()
        # 布局按"以原点为中心的矩形"用：取绝对最大范围，保证原点居中(build_catalog 末尾对所有家具做了 recenter，X/Y包围盒居中)。
        d, w, h = hi[0] - lo[0], hi[1] - lo[1], hi[2]
        lines.append('\t{ "%s", %.3ff, %.3ff, %.3ff },' % (name, d, w, h))
    lines += ["};", ""]
    open(path, "w", encoding="utf-8", newline="\r\n").write("\n".join(lines))


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        sys.exit(1)
    out_dir = args[0]
    header = args[args.index("--header") + 1] if "--header" in args else None
    os.makedirs(out_dir, exist_ok=True)
    catalog = build_catalog()
    report = {}
    for name, mesh in catalog.items():
        write_glb(os.path.join(out_dir, name + ".glb"), mesh, name)
        lo, hi = mesh.bounds()
        report[name] = {"min": [round(float(v), 3) for v in lo], "max": [round(float(v), 3) for v in hi],
                        "triangles": len(mesh.boxes) * 12,
                        "boxes": [list(map(lambda v: round(v, 3) if isinstance(v, float) else v, b)) for b in mesh.boxes]}
    json.dump(report, open(os.path.join(out_dir, "furniture.json"), "w"), indent=1)
    write_palette(os.path.join(out_dir, "palette.png"))
    if header:
        write_header(header, catalog)
    tris = [r["triangles"] for r in report.values()]
    print("%d meshes, triangles min/avg/max = %d/%d/%d" % (len(report), min(tris), sum(tris) // len(tris), max(tris)))
    for name, r in report.items():
        size = [round(r["max"][i] - r["min"][i], 2) for i in range(3)]
        print("  %-16s tris=%3d  size(depth,width,height)=%s  origin_min=%s" % (name, r["triangles"], size, r["min"]))


if __name__ == "__main__":
    main()
