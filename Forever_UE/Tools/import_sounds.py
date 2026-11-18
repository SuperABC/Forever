# 把Tools/SoundGen/out下的合成音效(wav)导入成UE资产：/Game/Asset/Sound/<名字>(SoundWave)。
# 脚本里的sound_effect和火箭弹的flightSoundPath引用的都是这些UE资产路径，游戏里不读磁盘文件。
# 先跑Tools/SoundGen/soundgen.py生成wav，再运行(关掉打开着的编辑器；可重复运行，已存在则覆盖)：
#   UnrealEditor-Cmd.exe Forever.uproject -run=pythonscript -script=Tools/import_sounds.py -unattended -AllowCommandletAudio
# (不带-AllowCommandletAudio导入会因为找不到BINKA解码器在ensure处崩掉)
# 注意：命令行里print不显示，日志用unreal.log_warning。
import os
import unreal

PACKAGE_DIR = "/Game/Asset/Sound"
SOURCE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "SoundGen", "out")
# 要导入的音效；LOOPING里的在导入后打开循环(火箭弹飞行音)。
SOUNDS = ["pistol", "rifle", "shotgun", "sniper", "camera", "grenade", "smoke",
          "rocket_launch", "rocket_flight", "rocket_explosion"]
LOOPING = {"rocket_flight"}

unreal.EditorAssetLibrary.make_directory(PACKAGE_DIR)
tasks = []
for name in SOUNDS:
    source = os.path.join(SOURCE_DIR, name + ".wav")
    if not os.path.exists(source):
        raise RuntimeError("找不到 " + source + "：先跑Tools/SoundGen/soundgen.py")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", PACKAGE_DIR)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

for name in SOUNDS:
    path = PACKAGE_DIR + "/" + name
    wave = unreal.EditorAssetLibrary.load_asset(path)
    if wave is None:
        raise RuntimeError("导入失败: " + path)
    wave.set_editor_property("looping", name in LOOPING)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log_warning("IMPORTSOUND saved " + path)
