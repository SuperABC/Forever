# 诊断：核对 M_Furniture 的节点连接和调色板贴图设置，并把贴图导出成png(环境变量 FURNITURE_EXPORT=路径)，
# 再核对家具网格的朝向(源约定：背面在-X、正面在+X，床头板/椅背是最高的顶点，应在X<0一侧)。
# 运行方式同 import_furniture.py（编辑器须关闭）。
import os

import unreal

eal = unreal.EditorAssetLibrary


def log(text):
    unreal.log_warning("FURNITURE-INSPECT " + text)


material = eal.load_asset("/Game/Asset/Room/Material/M_Furniture")
mel = unreal.MaterialEditingLibrary
if material:
    node = mel.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
    log("BaseColor <- {}".format(node.get_class().get_name() if node else None))
    if node and node.get_class().get_name() == "MaterialExpressionDesaturation":
        log("  desaturation inputs <- {}".format([x.get_class().get_name() + ":" + x.get_name() for x in mel.get_inputs_for_material_expression(material, node)]))

texture = eal.load_asset("/Game/Asset/Room/Texture/T_FurniturePalette")
if texture:
    for p in ("filter", "mip_gen_settings", "srgb", "compression_settings", "never_stream"):
        log("texture.{}={}".format(p, texture.get_editor_property(p)))
    export_path = os.environ.get("FURNITURE_EXPORT")
    if export_path:
        task = unreal.AssetExportTask()
        task.set_editor_property("object", texture)
        task.set_editor_property("filename", export_path)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_identical", True)
        task.set_editor_property("prompt", False)
        log("export {} -> {}".format(unreal.Exporter.run_asset_export_task(task), export_path))

for name in ("Bed_Double_A", "Chair_A", "Sofa3_A"):
    mesh = eal.load_asset("/Game/Asset/Room/Mesh/" + name)
    if not mesh:
        continue
    desc = mesh.get_static_mesh_description(0)
    pts = [desc.get_vertex_position(unreal.VertexID(i)) for i in range(desc.get_vertex_count())]
    top = max(p.z for p in pts)
    tall = [p for p in pts if p.z > top - 0.5]
    log("{}: tallest verts mean x={:.1f} (应<0)".format(name, sum(p.x for p in tall) / len(tall)))
