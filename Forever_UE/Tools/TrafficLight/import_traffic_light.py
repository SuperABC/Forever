# 第3步：把 split_traffic_light.py 写出的4个glb导入主工程，建灯的发光材质，再把导入结果导出成OBJ供核对：
#   /Game/Asset/TrafficLight/SM_TrafficLight_Frame    材质沿用原模型的 /Game/3rdParty/Retro_traffic_light/Materials/Scene_-_Root
#   /Game/Asset/TrafficLight/SM_TrafficLight_Red/Yellow/Green   材质 MI_TrafficLamp_Red/Yellow/Green
#   /Game/Asset/TrafficLight/M_TrafficLamp            灯罩材质：发光=LampColor×lerp(OffIntensity, OnIntensity, 逐实例数据0)，
#                                                     开了"用于ISM"，UE里ATrafficLightElement按信号改逐实例数据0(0=灭，亮的值见 TrafficLightElement.cpp 的 kLampLitValue；lerp 不截断，可以大于1)
# 运行(先编译好Forever_UE，关掉编辑器；可重复运行，已存在则覆盖)：
#   set TRAFFIC_LIGHT_DIR=<split输出目录(里面有4个glb)>
#   UnrealEditor-Cmd.exe Forever.uproject -ExecutePythonScript="<绝对路径>/Tools/TrafficLight/import_traffic_light.py" -unattended -stdout -FullStdOutLogOutput -nosplash
#   (不能用 -run=pythonscript：交换格式(Interchange)导入要同步内容浏览器，命令行模式下没有界面框架会崩)
# 导入后把4个网格再导出成 <目录>/check/*.obj，check_traffic_light.py 拿它和原模型逐个三角形比对(位置、UV、绕序)。
import os

import unreal

DEST = "/Game/Asset/TrafficLight"
TEMP = DEST + "/_import"
FRAME_MATERIAL = "/Game/3rdParty/Retro_traffic_light/Materials/Scene_-_Root"
LAMP_COLORS = {
    "Red": unreal.LinearColor(1.0, 0.04, 0.02, 1.0),
    "Yellow": unreal.LinearColor(1.0, 0.55, 0.02, 1.0),
    "Green": unreal.LinearColor(0.05, 1.0, 0.25, 1.0),
}
glb_dir = os.environ.get("TRAFFIC_LIGHT_DIR", ".")
check_dir = os.path.join(glb_dir, "check")
os.makedirs(check_dir, exist_ok=True)
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(text):
    unreal.log_warning("TRAFFICLIGHT " + text)


def box_text(mesh):
    box = mesh.get_bounding_box()
    return "min=({:.2f},{:.2f},{:.2f}) max=({:.2f},{:.2f},{:.2f})".format(box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)


def build_lamp_material():
    path = DEST + "/M_TrafficLamp"
    if library.does_asset_exist(path):
        library.delete_asset(path)
    material = tools.create_asset("M_TrafficLamp", DEST, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("used_with_instanced_static_meshes", True)
    color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -900, 0)
    color.set_editor_property("parameter_name", "LampColor")
    color.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    on = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 250)
    on.set_editor_property("parameter_name", "OnIntensity")
    on.set_editor_property("default_value", 100.0)
    off = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 350)
    off.set_editor_property("parameter_name", "OffIntensity")
    off.set_editor_property("default_value", 0.0)
    lit = mel.create_material_expression(material, unreal.MaterialExpressionPerInstanceCustomData, -900, 450)
    lit.set_editor_property("data_index", 0)  # 没有逐实例数据时(普通网格组件)默认是0=灭
    lerp = mel.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -600, 300)
    mel.connect_material_expressions(off, "", lerp, "A")
    mel.connect_material_expressions(on, "", lerp, "B")
    mel.connect_material_expressions(lit, "", lerp, "Alpha")
    emissive = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, 150)
    mel.connect_material_expressions(color, "", emissive, "A")
    mel.connect_material_expressions(lerp, "", emissive, "B")
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # 灭的时候看起来是暗色的玻璃：底色=灯色的12%。
    dim = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, -50)
    dim.set_editor_property("const_b", 0.12)
    mel.connect_material_expressions(color, "", dim, "A")
    mel.connect_material_property(dim, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -350, 450)
    rough.set_editor_property("r", 0.25)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    library.save_asset(path)
    log("material " + path)
    instances = {}
    for name, value in LAMP_COLORS.items():
        instance_path = "{}/MI_TrafficLamp_{}".format(DEST, name)
        if library.does_asset_exist(instance_path):
            library.delete_asset(instance_path)
        instance = tools.create_asset("MI_TrafficLamp_" + name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(instance, material)
        mel.set_material_instance_vector_parameter_value(instance, "LampColor", value)
        mel.update_material_instance(instance)
        library.save_asset(instance_path)
        instances[name] = instance
        log("material instance " + instance_path)
    return instances


def export_obj(mesh, name):
    task = unreal.AssetExportTask()
    task.set_editor_property("object", mesh)
    task.set_editor_property("filename", os.path.join(check_dir, name + ".obj"))
    task.set_editor_property("automated", True)
    task.set_editor_property("prompt", False)
    task.set_editor_property("exporter", unreal.StaticMeshExporterOBJ())
    unreal.Exporter.run_asset_export_task(task)


for stale in [TEMP] + ["{}/SM_TrafficLight_{}".format(DEST, n) for n in ("Frame", "Red", "Yellow", "Green")]:
    if library.does_directory_exist(stale):
        library.delete_directory(stale)
lamp_materials = build_lamp_material()
frame_material = library.load_asset(FRAME_MATERIAL)

names = ["SM_TrafficLight_Frame"] + ["SM_TrafficLight_" + n for n in LAMP_COLORS]
tasks = []
for name in names:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", "{}/{}.glb".format(glb_dir, name))
    task.set_editor_property("destination_path", TEMP)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    tasks.append(task)
tools.import_asset_tasks(tasks)

for name in names:
    path = "{}/{}".format(DEST, name)
    temp_path = "{0}/{1}/StaticMeshes/{1}".format(TEMP, name)
    if library.does_asset_exist(path):
        library.delete_asset(path)
    if library.does_asset_exist(temp_path):
        library.rename_asset(temp_path, path)
    mesh = library.load_asset(path)
    if mesh is None:
        log("IMPORT FAILED " + path)
        continue
    lamp = name.replace("SM_TrafficLight_", "")
    material = lamp_materials.get(lamp, frame_material)
    for slot in range(len(mesh.static_materials)):
        mesh.set_material(slot, material)
    # 不要Nanite：工程默认会给导入的网格开Nanite，渲染数据就变成按比例减面的回退网格(实测框架三角形少了一半、
    # 形状也变了)。原模型也没开Nanite。阻挡碰撞由UE侧另外给，这里不管。
    nanite = mesh.get_editor_property("nanite_settings")
    was_nanite = nanite.get_editor_property("enabled")
    if was_nanite:
        nanite.set_editor_property("enabled", False)
        # 走属性修改通知(相当于编辑器里勾掉)，网格会按新设置重新构建渲染数据
        mesh.set_editor_property("nanite_settings", nanite, unreal.PropertyAccessChangeNotifyMode.ALWAYS)
    log("{} nanite was {} now {}".format(name, was_nanite, mesh.get_editor_property("nanite_settings").get_editor_property("enabled")))
    library.save_asset(path)
    export_obj(mesh, name)
    log("{} slots={} tris={} {}".format(name, len(mesh.static_materials), mesh.get_num_triangles(0), box_text(mesh)))

library.delete_directory(TEMP)
