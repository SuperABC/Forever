# 在主工程里创建火箭弹蓝图：父类是C++类AForeverRocketProjectile，只覆盖参数和网格(逻辑全在C++里，蓝图里没有节点)。
#   /Game/Asset/Projectile/BP_RPG7Rocket    网格 SM_RPG7_Rocket，碰撞盒按网格实测尺寸
# 脚本 Resource/Story/weapon_rpg.script 的 spawn_actor 引用这个路径。
# 运行(先编译好Forever_UE，先跑完 import_rpg7.py，关掉打开着的编辑器；可重复运行，已存在则只更新参数)：
#   UnrealEditor-Cmd.exe Forever.uproject -run=pythonscript -script=Tools/make_rocket_blueprint.py -unattended
# 注意：命令行里print不显示，日志用unreal.log_warning。
import unreal

PACKAGE_DIR = "/Game/Asset/Projectile"
NAME = "BP_RPG7Rocket"
PATH = PACKAGE_DIR + "/" + NAME
MESH_PATH = "/Game/3rdParty/RPG7/StaticMeshes/SM_RPG7_Rocket"

# 飞行/爆炸参数(目测起点，PIE里调)。单位cm、cm/s、cm/s^2。
OVERRIDES = {
    "initial_speed": 1500.0,
    "max_speed": 9000.0,
    "acceleration": 6000.0,
    "life_seconds": 8.0,
    "explosion_scale": 3.0,
}


def log(text):
    unreal.log_warning("MAKEFX " + text)


parent = unreal.load_class(None, "/Script/Forever.ForeverRocketProjectile")
if parent is None:
    raise RuntimeError("找不到/Script/Forever.ForeverRocketProjectile：Forever没编译？")

mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
if mesh is None:
    raise RuntimeError("找不到火箭弹网格 " + MESH_PATH + "：先跑 import_rpg7.py")

unreal.EditorAssetLibrary.make_directory(PACKAGE_DIR)
if unreal.EditorAssetLibrary.does_asset_exist(PATH):
    blueprint = unreal.EditorAssetLibrary.load_asset(PATH)
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PACKAGE_DIR, unreal.Blueprint, factory)
if blueprint is None:
    raise RuntimeError("创建蓝图失败: " + PATH)

default_object = unreal.get_default_object(blueprint.generated_class())
default_object.set_editor_property("rocket_mesh", mesh)

# 碰撞盒半尺寸：网格原点在尾端、弹头朝+X，所以长度=包围盒X尺寸，Y/Z取半径(略放大一点，避免擦边穿过)。
box = mesh.get_bounding_box()
length = box.max.x - box.min.x
radius = max(box.max.y - box.min.y, box.max.z - box.min.z) * 0.5
default_object.set_editor_property("box_half_extent", unreal.Vector(length * 0.5, radius * 1.2, radius * 1.2))
log("mesh length={:.1f} radius={:.1f}".format(length, radius))

for key, value in OVERRIDES.items():
    default_object.set_editor_property(key, value)

unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
unreal.EditorAssetLibrary.save_asset(PATH)
log("saved " + PATH)
