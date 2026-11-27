# 第1步：把原红绿灯模型(只有一个材质槽，三盏灯没分开)和它的贴图导出成文件，交给 split_traffic_light.py 拆分。
#   网格  -> <out>/retro_traffic_light.obj   (UE自带的OBJ导出，坐标是UE厘米)
#   贴图  -> <out>/<贴图名>.png
#   日志里打印原模型的包围盒(UE厘米)，拆分后导入时用来核对坐标没有转错。
# 运行(先关掉编辑器)：
#   set TRAFFIC_LIGHT_DIR=<输出目录>
#   UnrealEditor-Cmd.exe Forever.uproject -run=pythonscript -script=<绝对路径>/Tools/TrafficLight/export_traffic_light.py -unattended
# 命令行里print不显示，日志用unreal.log_warning(前缀TRAFFICLIGHT)。
import os

import unreal

MESH = "/Game/3rdParty/Retro_traffic_light/StaticMeshes/retro_traffic_light"
TEXTURE_DIR = "/Game/3rdParty/Retro_traffic_light/Textures"
out_dir = os.environ.get("TRAFFIC_LIGHT_DIR", ".")
os.makedirs(out_dir, exist_ok=True)


def log(text):
    unreal.log_warning("TRAFFICLIGHT " + text)


def export(asset, filename, exporter):
    task = unreal.AssetExportTask()
    task.set_editor_property("object", asset)
    task.set_editor_property("filename", filename)
    task.set_editor_property("automated", True)
    task.set_editor_property("prompt", False)
    task.set_editor_property("replace_identical", True)
    if exporter is not None:
        task.set_editor_property("exporter", exporter)
    ok = unreal.Exporter.run_asset_export_task(task)
    log("export {} -> {} ok={}".format(asset.get_path_name(), filename, ok))
    return ok


mesh = unreal.EditorAssetLibrary.load_asset(MESH)
if not mesh:
    log("mesh not found: " + MESH)
else:
    box = mesh.get_bounding_box()
    log("bounds min=({:.2f},{:.2f},{:.2f}) max=({:.2f},{:.2f},{:.2f})".format(box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z))
    log("lod0 triangles={} vertices={} sections={}".format(
        mesh.get_num_triangles(0) if hasattr(mesh, "get_num_triangles") else -1,
        mesh.get_num_vertices(0) if hasattr(mesh, "get_num_vertices") else -1,
        mesh.get_num_sections(0)))
    for i, material in enumerate(mesh.static_materials):
        log("material slot {} name={} material={}".format(i, material.material_slot_name,
            material.material_interface.get_path_name() if material.material_interface else "None"))
    export(mesh, os.path.join(out_dir, "retro_traffic_light.obj"), unreal.StaticMeshExporterOBJ())

for path in unreal.EditorAssetLibrary.list_assets(TEXTURE_DIR, recursive=False, include_folder=False):
    texture = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(texture, unreal.Texture2D):
        continue
    log("texture {} size={}x{} compression={} srgb={}".format(path, texture.blueprint_get_size_x(), texture.blueprint_get_size_y(),
        texture.get_editor_property("compression_settings"), texture.get_editor_property("srgb")))
    export(texture, os.path.join(out_dir, texture.get_name() + ".png"), unreal.TextureExporterPNG())
