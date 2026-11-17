# 在主工程里创建特效蓝图：父类是C++类AForeverExplosionEffect，只覆盖参数。
#   /Game/Asset/Effects/BP_BulletImpact    枪击特效(全部用C++默认参数，即以前枪打在人身上的效果)
#   /Game/Asset/Effects/BP_GrenadeExplosion 手雷爆炸(比枪击更大更久，目测起点，PIE里调)
# 脚本里的spawn_effect引用这两个路径(Resource/Story/weapon_*.script、asset_grenade.script)。
# 运行(先编译好Forever_UE，关掉打开着的编辑器；可重复运行，已存在则只更新参数)：
#   UnrealEditor-Cmd.exe Forever.uproject -run=pythonscript -script=Tools/make_effect_blueprints.py -unattended
# 注意：命令行里print不显示，日志用unreal.log_warning。
import unreal

PACKAGE_DIR = "/Game/Asset/Effects"

BLUEPRINTS = {
    "BP_BulletImpact": {},
    "BP_GrenadeExplosion": {
        "fireball_radius": 150.0,
        "fireball_duration": 0.7,
        "ring_radius": 500.0,
        "ring_duration": 0.5,
        "total_duration": 2.2,
        "flash_intensity": 600000.0,
        "flash_duration": 0.5,
        "spark_speed_min": 500.0,
        "spark_speed_max": 1400.0,
        "smoke_radius": 70.0,
        "smoke_rise_speed": 110.0,
    },
}

parent = unreal.load_class(None, "/Script/Forever.ForeverExplosionEffect")
if parent is None:
    raise RuntimeError("找不到/Script/Forever.ForeverExplosionEffect：Forever没编译？")

unreal.EditorAssetLibrary.make_directory(PACKAGE_DIR)
for name, overrides in BLUEPRINTS.items():
    path = PACKAGE_DIR + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        blueprint = unreal.EditorAssetLibrary.load_asset(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PACKAGE_DIR, unreal.Blueprint, factory)
    if blueprint is None:
        raise RuntimeError("创建蓝图失败: " + path)
    default_object = unreal.get_default_object(blueprint.generated_class())
    for key, value in overrides.items():
        default_object.set_editor_property(key, value)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log_warning("MAKEFX saved " + path)
