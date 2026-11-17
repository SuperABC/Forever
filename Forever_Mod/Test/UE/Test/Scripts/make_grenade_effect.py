# 在迷你工程里创建手雷爆炸特效蓝图 /Test/Effect/BP_GrenadeExplosion：父类是主程序里的AForeverExplosionEffect(迷你工程里的
# Forever桩模块提供同名类)，只覆盖参数。用法见Forever_Mod/README.md"用mod提供特效蓝图"。
# 运行：UnrealEditor-Cmd.exe Test.uproject -ExecutePythonScript=Scripts/make_grenade_effect.py -unattended
import unreal

PACKAGE_DIR = "/Test/Effect"
NAME = "BP_GrenadeExplosion"
PATH = PACKAGE_DIR + "/" + NAME

# 比枪击特效(默认值)更大更久的参数，目测起点。键是C++里UPROPERTY名的snake_case。
OVERRIDES = {
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
}

parent = unreal.load_class(None, "/Script/Forever.ForeverExplosionEffect")
if parent is None:
    raise RuntimeError("找不到/Script/Forever.ForeverExplosionEffect：迷你工程的Forever桩模块没编译？")

if unreal.EditorAssetLibrary.does_asset_exist(PATH):
    unreal.log("{} 已存在，直接更新参数。".format(PATH))
    blueprint = unreal.EditorAssetLibrary.load_asset(PATH)
else:
    unreal.EditorAssetLibrary.make_directory(PACKAGE_DIR)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PACKAGE_DIR, unreal.Blueprint, factory)
if blueprint is None:
    raise RuntimeError("创建蓝图失败: " + PATH)

default_object = unreal.get_default_object(blueprint.generated_class())
for key, value in OVERRIDES.items():
    default_object.set_editor_property(key, value)
    unreal.log("{} = {}".format(key, default_object.get_editor_property(key)))

unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
unreal.EditorAssetLibrary.save_asset(PATH)
unreal.log("已保存 " + PATH)
