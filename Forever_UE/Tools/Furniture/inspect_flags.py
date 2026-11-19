# 诊断(只读)：对比现有能正常显示的基础材质和 M_Furniture 的"使用标志"(ISM/Nanite等)。
# 材质没勾 used_with_instanced_static_meshes 时，独立运行的游戏里不会自动补，会退回引擎默认灰材质。
import unreal

eal = unreal.EditorAssetLibrary
FLAGS = ("used_with_instanced_static_meshes", "used_with_nanite", "used_with_static_lighting", "used_with_skeletal_mesh",
         "automatically_set_usage_in_editor", "two_sided", "blend_mode")


def log(text):
    unreal.log_warning("FURNITURE-FLAGS " + text)


for path in ("/Game/Asset/Common/Material/PureColor", "/Game/Asset/Common/Material/BasicAlign", "/Game/Asset/Room/Material/M_Furniture"):
    material = eal.load_asset(path)
    parts = []
    for flag in FLAGS:
        try:
            parts.append("{}={}".format(flag, material.get_editor_property(flag)))
        except Exception as e:  # noqa
            parts.append("{}=<n/a>".format(flag))
    log("{}: {}".format(path, " ".join(parts)))

mesh = eal.load_asset("/Game/Asset/Room/Mesh/Bed_Double_A")
try:
    log("Bed_Double_A nanite enabled={}".format(mesh.get_editor_property("nanite_settings").get_editor_property("enabled")))
except Exception as e:  # noqa
    log("nanite_settings n/a: {}".format(str(e)[:100]))
building_mesh = eal.load_asset("/Game/Asset/Building/Mesh/Wall")
try:
    log("Building Wall nanite enabled={}".format(building_mesh.get_editor_property("nanite_settings").get_editor_property("enabled")))
except Exception as e:  # noqa
    log("wall nanite n/a: {}".format(str(e)[:100]))
