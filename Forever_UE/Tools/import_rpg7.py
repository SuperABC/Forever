# 把 split_rpg7.py 拆出来的两个glb导入主工程，材质槽指到现有的RPG7材质(不重复导贴图)，打印导入后的包围盒供核对朝向/尺寸：
#   /Game/3rdParty/RPG7/StaticMeshes/SM_RPG7_Launcher   材质 /Game/3rdParty/RPG7/Materials/RPG7
#   /Game/3rdParty/RPG7/StaticMeshes/SM_RPG7_Rocket     材质 /Game/3rdParty/RPG7/Materials/RPG7Rocket
# 运行(先编译好Forever_UE，关掉打开着的编辑器；可重复运行，已存在则覆盖)：
#   set RPG7_GLB_DIR=<glb目录>
#   UnrealEditor-Cmd.exe Forever.uproject -run=pythonscript -script=Tools/import_rpg7.py -unattended
# glb目录(环境变量RPG7_GLB_DIR)里要有 SM_RPG7_Launcher.glb / SM_RPG7_Rocket.glb。注意：命令行里print不显示，日志用unreal.log_warning。
import os

import unreal

DEST = "/Game/3rdParty/RPG7/StaticMeshes"
# Interchange会把导入物放在 <目标>/<名>/StaticMeshes/<名> 并自带一份材质实例：先导到临时目录，再把网格挪到DEST，临时目录整个删掉。
TEMP = "/Game/3rdParty/RPG7/_import"
MATERIAL_DIR = "/Game/3rdParty/RPG7/Materials"
JOBS = [
    ("SM_RPG7_Launcher", "RPG7"),
    ("SM_RPG7_Rocket", "RPG7Rocket"),
]

glb_dir = os.environ.get("RPG7_GLB_DIR", ".")


def log(text):
    unreal.log_warning("RPG7 " + text)


def box_text(mesh):
    box = mesh.get_bounding_box()
    size = box.max - box.min
    return "min=({:.1f},{:.1f},{:.1f}) max=({:.1f},{:.1f},{:.1f}) size=({:.1f},{:.1f},{:.1f})".format(
        box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z, size.x, size.y, size.z)


# 参照：原模型导入后的包围盒(爆炸图整体)，用来对照glTF->UE的轴映射。
original = unreal.EditorAssetLibrary.load_asset(DEST + "/rpg7")
if original:
    log("original rpg7 " + box_text(original))

# 上次失败留下的目录/资产先清掉。
for stale in (DEST + "/SM_RPG7_Launcher", DEST + "/SM_RPG7_Rocket", TEMP):
    if unreal.EditorAssetLibrary.does_directory_exist(stale):
        unreal.EditorAssetLibrary.delete_directory(stale)

tasks = []
for name, _ in JOBS:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", "{}/{}.glb".format(glb_dir, name))
    task.set_editor_property("destination_path", TEMP)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

for name, material_name in JOBS:
    path = "{}/{}".format(DEST, name)
    temp_path = "{0}/{1}/StaticMeshes/{1}".format(TEMP, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    if unreal.EditorAssetLibrary.does_asset_exist(temp_path):
        unreal.EditorAssetLibrary.rename_asset(temp_path, path)
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if mesh is None:
        log("IMPORT FAILED " + path)
        continue
    material = unreal.EditorAssetLibrary.load_asset("{}/{}".format(MATERIAL_DIR, material_name))
    if material is None:
        log("material missing " + material_name)
    else:
        for slot in range(len(mesh.static_materials)):
            mesh.set_material(slot, material)
    unreal.EditorAssetLibrary.save_asset(path)
    log("{} slots={} {}".format(name, len(mesh.static_materials), box_text(mesh)))

unreal.EditorAssetLibrary.delete_directory(TEMP)
