# 创建/重建地形材质 /Game/Asset/Terrain/M_Terrain(运行时唯一的地形材质，所有LOD共用)。
# 材质只有一个Custom HLSL节点(terrain_material.hlsl原样贴入)，纹理/表/覆盖图都是运行时由
# Source/Forever/Framework/TerrainMaterialBuilder.cpp 生成并通过MID的同名参数传入；这里的Placeholder资产只是为了让
# 材质编译时知道参数类型(Texture2DArray/Texture2D、sRGB与否)，运行时会被MID整个替换。
# 可重复运行：已存在的资产会清空节点后重建。改了hlsl后重跑即可。
# 运行(先关掉打开着的编辑器；-AllowCommandletRendering才会真的编译shader，否则statistics恒为0)：
#   UnrealEditor-Cmd.exe <Forever.uproject绝对路径> -run=pythonscript -script=<本脚本绝对路径> -unattended -AllowCommandletRendering
# 注意：命令行里print不显示，日志用unreal.log_warning；uproject要用绝对路径。
import os
import unreal

PACKAGE_DIR = "/Game/Asset/Terrain"
PLACEHOLDER_DIR = PACKAGE_DIR + "/Placeholder"
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
HLSL_PATH = os.path.join(SCRIPT_DIR, "terrain_material.hlsl")

mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
problems = []


def warn(msg):
    unreal.log_warning("[build_terrain_material] " + msg)


def fail(msg):
    problems.append(msg)
    unreal.log_warning("[build_terrain_material] PROBLEM: " + msg)


def set_prop(obj, name, value):
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception as e:  # noqa
        fail("cannot set %s.%s: %s" % (obj.get_name(), name, str(e)[:150]))
        return False


def get_or_create(directory, name, asset_class, factory):
    path = "%s/%s" % (directory, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.EditorAssetLibrary.load_asset(path)
    return asset_tools.create_asset(name, directory, asset_class, factory)


def make_placeholder_array(name, srgb):
    tex = get_or_create(PLACEHOLDER_DIR, name, unreal.Texture2DArray, unreal.Texture2DArrayFactory())
    set_prop(tex, "srgb", srgb)
    unreal.EditorAssetLibrary.save_asset("%s/%s" % (PLACEHOLDER_DIR, name))
    return tex


def make_placeholder_2d(name, srgb):
    tex = get_or_create(PLACEHOLDER_DIR, name, unreal.Texture2D, unreal.Texture2DFactoryNew())
    set_prop(tex, "srgb", srgb)
    set_prop(tex, "compression_settings", unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)  # 不压缩的BGRA8，线性
    unreal.EditorAssetLibrary.save_asset("%s/%s" % (PLACEHOLDER_DIR, name))
    return tex


def add_expr(mat, cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


def connect(src, src_out, dst, dst_in):
    if not mel.connect_material_expressions(src, src_out, dst, dst_in):
        fail("connect failed: %s.%s -> %s.%s" % (src.get_name(), src_out, dst.get_name(), dst_in))


def texture_param(mat, name, texture, sampler_type, x, y):
    node = add_expr(mat, unreal.MaterialExpressionTextureObjectParameter, x, y)
    set_prop(node, "parameter_name", name)
    set_prop(node, "texture", texture)
    set_prop(node, "sampler_type", sampler_type)
    return node


def vector_param(mat, name, value, x, y):
    # 向量参数节点的主输出只有RGB(float3)，Custom里要用.w，所以用Append把A通道接到后面拼成float4。
    node = add_expr(mat, unreal.MaterialExpressionVectorParameter, x, y)
    set_prop(node, "parameter_name", name)
    set_prop(node, "default_value", unreal.LinearColor(value[0], value[1], value[2], value[3]))
    append = add_expr(mat, unreal.MaterialExpressionAppendVector, x + 200, y)
    connect(node, "", append, "A")
    connect(node, "A", append, "B")
    return append


def build():
    if not unreal.EditorAssetLibrary.does_directory_exist(PLACEHOLDER_DIR):
        unreal.EditorAssetLibrary.make_directory(PLACEHOLDER_DIR)
    ta_color = make_placeholder_array("TA_Color", True)
    ta_linear = make_placeholder_array("TA_Linear", False)
    t2_linear = make_placeholder_2d("T2D_Linear", False)

    mat = get_or_create(PACKAGE_DIR, "M_Terrain", unreal.Material, unreal.MaterialFactoryNew())
    for _ in range(10):
        if mel.get_num_material_expressions(mat) == 0:
            break
        mel.delete_all_material_expressions(mat)
    set_prop(mat, "tangent_space_normal", False)  # 材质输出的是世界空间法线
    set_prop(mat, "blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    set_prop(mat, "shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)

    S = unreal.MaterialSamplerType
    diff = texture_param(mat, "TerrainDiffuseArray", ta_color, S.SAMPLERTYPE_COLOR, -1400, -600)
    nrm = texture_param(mat, "TerrainNormalArray", ta_linear, S.SAMPLERTYPE_LINEAR_COLOR, -1400, -440)
    mask = texture_param(mat, "TerrainMaskArray", ta_linear, S.SAMPLERTYPE_LINEAR_COLOR, -1400, -360)
    cov = texture_param(mat, "TerrainCoverageArray", ta_linear, S.SAMPLERTYPE_LINEAR_COLOR, -1400, -280)
    ttab = texture_param(mat, "TerrainTypeTable", t2_linear, S.SAMPLERTYPE_LINEAR_COLOR, -1400, -120)
    ltab = texture_param(mat, "TerrainLayerTable", t2_linear, S.SAMPLERTYPE_LINEAR_COLOR, -1400, 40)
    noise = texture_param(mat, "TerrainNoise", t2_linear, S.SAMPLERTYPE_LINEAR_COLOR, -1400, 200)

    map_info = vector_param(mat, "TerrainMapInfo", (1024.0, 1024.0, 5.0, 2.0), -1400, 380)
    water_enc = vector_param(mat, "TerrainWaterEncoding", (-4.0, 24.0, 0.0, 0.0), -1400, 520)
    # 调参向量：T0=(覆盖图采样扭曲幅度(element), 每种地形噪声幅度, 锐化带宽, -)
    #           T1=(远处平均色起/止(米), 细节法线淡出起/止(米))
    #           T2=(三向投影最远距离(米), 噪声周期(element), -, -)
    t0 = vector_param(mat, "TerrainTuning0", (0.35, 0.12, 0.30, 0.0), -1400, 660)
    t1 = vector_param(mat, "TerrainTuning1", (120.0, 200.0, 40.0, 140.0), -1400, 800)
    t2 = vector_param(mat, "TerrainTuning2", (70.0, 16.0, 0.0, 0.0), -1400, 940)

    world_pos = add_expr(mat, unreal.MaterialExpressionWorldPosition, -1400, 1080)
    cam_pos = add_expr(mat, unreal.MaterialExpressionCameraPositionWS, -1400, 1220)
    vnormal = add_expr(mat, unreal.MaterialExpressionVertexNormalWS, -1400, 1360)
    vcolor = add_expr(mat, unreal.MaterialExpressionVertexColor, -1400, 1500)

    with open(HLSL_PATH, "r", encoding="utf-8") as f:
        code = f.read()
    custom = add_expr(mat, unreal.MaterialExpressionCustom, -700, 400)
    set_prop(custom, "description", "TerrainShading")
    set_prop(custom, "code", code)
    set_prop(custom, "output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    input_names = ["WP", "Cam", "VN", "VC", "DiffArr", "NrmArr", "MaskArr", "CovArr", "TypeTab", "LayerTab", "Noise",
                   "MapInfo", "WaterEnc", "T0", "T1", "T2"]
    items = []
    for n in input_names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        items.append(ci)
    set_prop(custom, "inputs", items)
    outs = []
    for n, t in [("NormalOut", unreal.CustomMaterialOutputType.CMOT_FLOAT3),
                 ("RoughOut", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                 ("AOOut", unreal.CustomMaterialOutputType.CMOT_FLOAT1)]:
        co = unreal.CustomOutput()
        co.set_editor_property("output_name", n)
        co.set_editor_property("output_type", t)
        outs.append(co)
    set_prop(custom, "additional_outputs", outs)

    for node, name in [(world_pos, "WP"), (cam_pos, "Cam"), (vnormal, "VN"), (vcolor, "VC"), (diff, "DiffArr"), (nrm, "NrmArr"), (mask, "MaskArr"),
                       (cov, "CovArr"), (ttab, "TypeTab"), (ltab, "LayerTab"), (noise, "Noise"), (map_info, "MapInfo"),
                       (water_enc, "WaterEnc"), (t0, "T0"), (t1, "T1"), (t2, "T2")]:
        connect(node, "", custom, name)

    P = unreal.MaterialProperty
    for out_name, prop in [("", P.MP_BASE_COLOR), ("NormalOut", P.MP_NORMAL), ("RoughOut", P.MP_ROUGHNESS), ("AOOut", P.MP_AMBIENT_OCCLUSION)]:
        if not mel.connect_material_property(custom, out_name, prop):
            fail("connect_material_property failed: %s -> %s" % (out_name or "main", prop))

    mel.recompile_material(mat)
    st = mel.get_statistics(mat)
    warn("statistics: ps=%s vs=%s samplers=%s virtual_texture_stack=%s" % (
        st.num_pixel_shader_instructions, st.num_vertex_shader_instructions, st.num_samplers, st.num_virtual_texture_samples))
    if st.num_pixel_shader_instructions <= 0:
        fail("pixel shader instruction count is 0: shader did not compile (missing -AllowCommandletRendering, or HLSL error; see log)")
    unreal.EditorAssetLibrary.save_asset(PACKAGE_DIR + "/M_Terrain")
    warn("saved M_Terrain")


build()
if problems:
    unreal.log_warning("[build_terrain_material] FINISHED WITH %d PROBLEM(S)" % len(problems))
else:
    unreal.log_warning("[build_terrain_material] OK")
