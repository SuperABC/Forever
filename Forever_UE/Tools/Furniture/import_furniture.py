# 把 make_furniture.py 生成的家具导入主工程：
#   /Game/Asset/Room/Texture/T_FurniturePalette   调色板贴图(Nearest、无mip、不压缩)
#   /Game/Asset/Room/Material/M_Furniture         共用材质(贴图->超饱和->BaseColor，粗糙度0.85，双面)
#   /Game/Asset/Room/Mesh/<名字>                  各家具StaticMesh，材质槽都指向M_Furniture
# 运行(关掉打开着的编辑器；可重复运行，已存在则覆盖)：
#   set FURNITURE_DIR=<make_furniture.py的输出目录>
#   UnrealEditor-Cmd.exe Forever.uproject -ExecutePythonScript="<绝对路径>/Tools/Furniture/import_furniture.py" -unattended -stdout -FullStdOutLogOutput -nosplash
# 注意：命令行里print不显示，日志用unreal.log_warning。
import os

import unreal

ROOT = "/Game/Asset/Room"
MESH_DIR = ROOT + "/Mesh"
MATERIAL_DIR = ROOT + "/Material"
TEXTURE_DIR = ROOT + "/Texture"
TEMP = ROOT + "/_import"
TEXTURE_NAME = "T_FurniturePalette"
MATERIAL_NAME = "M_Furniture"

src_dir = os.environ.get("FURNITURE_DIR", ".")
eal = unreal.EditorAssetLibrary


def log(text):
    unreal.log_warning("FURNITURE " + text)


def import_tasks(tasks):
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)


def make_task(filename, dest_dir, dest_name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", dest_dir)
    task.set_editor_property("destination_name", dest_name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    return task


for d in (MESH_DIR, MATERIAL_DIR, TEXTURE_DIR):
    eal.make_directory(d)

# ---- 贴图 ----
texture_path = "{}/{}".format(TEXTURE_DIR, TEXTURE_NAME)
import_tasks([make_task(os.path.join(src_dir, "palette.png"), TEXTURE_DIR, TEXTURE_NAME)])
texture = eal.load_asset(texture_path)
if texture is None:
    raise RuntimeError("导入调色板贴图失败: " + texture_path)
texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property("srgb", True)
# 不压缩：默认的DXT压缩按4x4像素一块取两个端点色，色块挨在一起的调色板会被糊成灰色(曾经"所有家具都是灰的"的原因)。
texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property("never_stream", True)
eal.save_asset(texture_path)
log("texture ok " + texture_path)

# ---- 材质 ----
material_path = "{}/{}".format(MATERIAL_DIR, MATERIAL_NAME)
if eal.does_asset_exist(material_path):
    eal.delete_asset(material_path)
material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    MATERIAL_NAME, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
mel = unreal.MaterialEditingLibrary
sample = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, -400, 0)
sample.set_editor_property("texture", texture)
# 本工程日间曝光很重(ExtendDefaultLuminanceRange+LocalExposure)，颜色容易被冲淡发灰：贴图色先过一道"超饱和"
# (Desaturation的Fraction取负=向外拉开饱和度)再进BaseColor。想更艳/更淡就改 SATURATION_BOOST。
SATURATION_BOOST = 0.35
saturate = mel.create_material_expression(material, unreal.MaterialExpressionDesaturation, -200, 0)
boost = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 150)
boost.set_editor_property("r", -SATURATION_BOOST)
mel.connect_material_expressions(sample, "RGB", saturate, "")
mel.connect_material_expressions(boost, "", saturate, "Fraction")
mel.connect_material_property(saturate, "", unreal.MaterialProperty.MP_BASE_COLOR)
rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 300)
rough.set_editor_property("r", 0.85)
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
material.set_editor_property("two_sided", True)  # 绕向不确定时的保险；家具很少，开销可忽略
# 家具是 Nanite 网格 + ISM 渲染。命令行建的材质这两个使用标志默认是False，独立运行的游戏里不会自动补，
# 会整体退回引擎默认灰材质(表现为一片暗灰)。必须显式勾上，和 PureColor/BasicAlign 一致。
material.set_editor_property("used_with_instanced_static_meshes", True)
material.set_editor_property("used_with_nanite", True)
mel.recompile_material(material)
eal.save_asset(material_path)
log("material ok " + material_path)

# ---- 网格 ----
if eal.does_directory_exist(TEMP):
    eal.delete_directory(TEMP)
names = sorted(f[:-4] for f in os.listdir(src_dir) if f.endswith(".glb"))
import_tasks([make_task(os.path.join(src_dir, n + ".glb"), TEMP, n) for n in names])

for name in names:
    path = "{}/{}".format(MESH_DIR, name)
    temp_path = "{0}/{1}/StaticMeshes/{1}".format(TEMP, name)
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    if eal.does_asset_exist(temp_path):
        eal.rename_asset(temp_path, path)
    mesh = eal.load_asset(path)
    if mesh is None:
        log("IMPORT FAILED " + path)
        continue
    for slot in range(len(mesh.static_materials)):
        mesh.set_material(slot, material)
    eal.save_asset(path)
    box = mesh.get_bounding_box()
    size = box.max - box.min
    log("{} slots={} min=({:.1f},{:.1f},{:.1f}) size=({:.1f},{:.1f},{:.1f})".format(
        name, len(mesh.static_materials), box.min.x, box.min.y, box.min.z, size.x, size.y, size.z))

eal.delete_directory(TEMP)
log("done {} meshes".format(len(names)))
