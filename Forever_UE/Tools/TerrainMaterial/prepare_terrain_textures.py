# 把地形层贴图设置成运行时能直接拼进地形贴图数组的格式(详见 Tools/TerrainMaterial/README.md)。
# 运行时 TerrainMaterialBuilder 不再读源像素，而是把cook后的压缩块(含mip)直接拷进 Texture2DArray 的切片，所以所有层贴图必须：
#   正方形 1024x1024(最大尺寸限制，源图可以更大，cook时降采样并生成mip)、不流送(整条mip链随资产内联)、统一压缩格式：
#     漫反射(_B / *Diffuse)           TC_BC7，sRGB            -> PF_BC7
#     法线(_N / *Normal)               TC_Normalmap，线性      -> PF_BC5 (贴图自己的 flip_green_channel 在cook时生效)
#     遮罩(_ORM / *Roughness)          TC_BC7，线性            -> PF_BC7 (ORM: R=AO,G=粗糙度,B=金属度；单独的粗糙度图灰度即可)
# 用法(每个含地形层贴图的UE工程在cook前都要跑，已写进各工程的 pak.bat)：
#   UnrealEditor-Cmd.exe <工程.uproject> -run=pythonscript -script="<本脚本绝对路径> <游戏路径1> [<游戏路径2> ...]" -unattended
# 游戏路径是资产目录，如 /Game/3rdParty/Megascans/Surfaces 或 mod插件的 /Test/TerrainLayers；目录不存在就跳过(不算错)。
# 按资产名后缀识别角色，其它贴图不动。已经符合的资产不会被重新保存。有任何问题(源图不是方形2的幂、小于1024等)会抛异常，cook不应继续。
import sys
import unreal

RES = 1024
DIFFUSE_SUFFIXES = ("_B", "_D", "Diffuse")
NORMAL_SUFFIXES = ("_N", "Normal")
MASK_SUFFIXES = ("_ORM", "Roughness")

problems = []


def warn(msg):
    unreal.log_warning("[prepare_terrain_textures] " + msg)


def role_of(name):
    if name.endswith(NORMAL_SUFFIXES):
        return "normal"
    if name.endswith(DIFFUSE_SUFFIXES):
        return "diffuse"
    if name.endswith(MASK_SUFFIXES):
        return "mask"
    return None


def is_pow2(v):
    return v > 0 and (v & (v - 1)) == 0


def wanted(role):
    T = unreal.TextureCompressionSettings
    if role == "diffuse":
        return {"compression_settings": T.TC_BC7, "srgb": True}
    if role == "normal":
        return {"compression_settings": T.TC_NORMALMAP, "srgb": False}
    return {"compression_settings": T.TC_BC7, "srgb": False}


def prepare(directory):
    if not unreal.EditorAssetLibrary.does_directory_exist(directory):
        warn("skip (directory does not exist): %s" % directory)
        return 0
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    changed = 0
    seen = 0
    for data in reg.get_assets_by_path(directory, recursive=True):
        if str(data.asset_class_path.asset_name) != "Texture2D":
            continue
        name = str(data.asset_name)
        role = role_of(name)
        if role is None:
            continue
        tex = data.get_asset()
        seen += 1
        w, h = tex.blueprint_get_size_x(), tex.blueprint_get_size_y()
        if w != h or not is_pow2(w) or w < RES:
            problems.append("%s: source is %dx%d, must be square, power of two and >= %d" % (data.package_name, w, h, RES))
            continue
        edits = dict(wanted(role))
        edits["max_texture_size"] = RES
        edits["never_stream"] = True
        dirty = False
        for key, value in edits.items():
            if tex.get_editor_property(key) != value:
                tex.set_editor_property(key, value)
                dirty = True
        if dirty:
            changed += 1
            if not unreal.EditorAssetLibrary.save_loaded_asset(tex):
                problems.append("%s: could not save" % data.package_name)
            warn("updated %s (%s)" % (data.package_name, role))
    warn("%s: %d terrain layer texture(s), %d updated" % (directory, seen, changed))
    return seen


def main():
    dirs = sys.argv[1:]
    if not dirs:
        problems.append("no directories given (usage: prepare_terrain_textures.py /Game/Some/Dir ...)")
    for d in dirs:
        prepare(d)
    if problems:
        for p in problems:
            unreal.log_error("[prepare_terrain_textures] PROBLEM: " + p)
        raise RuntimeError("prepare_terrain_textures: %d problem(s)" % len(problems))
    warn("OK")


main()
