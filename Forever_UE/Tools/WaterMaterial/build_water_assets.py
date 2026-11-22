# 创建/重建水面相关资产(都在 /Game/Asset/Water/)：
#   MPC_Water       标量参数 WaterTime(世界时间，C++每帧写)、WaterBaseLevel(相机所在格水位，cm，C++写)、WaterLightScale(阳光下白色漫反射面的辐亮度=(太阳+环境lux)*0.5/π，C++写，雾色乘它)
#   M_WaterSurface  水面材质：WPO=波浪高度(只加Z)，法线=对同一个高度表达式做有限差分
#   PP_Underwater   水下后处理：逐像素重建世界坐标，按"视线在水面以下的路径长度"做吸收+雾
# 波浪表达式只有一份：Source/Forever/Water/WaterWave.inc(C++浮力也#include它)，本脚本把它的文本贴进材质的Custom节点。
# 改了波形(.inc)之后重跑本脚本即可。可重复运行：已存在的资产会清空节点后重建。
# 运行(先编译好Forever_UE，关掉打开着的编辑器)：
#   UnrealEditor-Cmd.exe <Forever.uproject绝对路径> -run=pythonscript -script=<本脚本绝对路径> -unattended
# 注意：命令行里print不显示，日志用unreal.log_warning；uproject要用绝对路径。
import os
import re
import unreal

PACKAGE_DIR = "/Game/Asset/Water"
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
INC_PATH = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", "Source", "Forever", "Water", "WaterWave.inc"))

mel = unreal.MaterialEditingLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
problems = []


def warn(msg):
    unreal.log_warning("[build_water_assets] " + msg)


def fail(msg):
    problems.append(msg)
    unreal.log_warning("[build_water_assets] PROBLEM: " + msg)


# ---------------------------------------------------------------------------------------------
# 把 WaterWave.inc 的函数体"内联"成HLSL代码块：x/y/t/iterations换成给定表达式，末尾的return换成给out变量赋值。
# Custom节点里没法定义函数，所以每次求值都贴一份(用花括号隔离局部变量)。
# ---------------------------------------------------------------------------------------------
def load_wave_body():
    with open(INC_PATH, "r", encoding="utf-8") as f:
        text = f.read()
    # 去掉整行注释(里面的x,y会被下面的替换误伤，虽然无害)
    lines = [l for l in text.splitlines() if not l.strip().startswith("//")]
    body = "\n".join(lines)
    if "return " not in body:
        raise RuntimeError("WaterWave.inc has no return statement")
    return body


WAVE_BODY = load_wave_body()
_block_counter = [0]


def wave_block(x_expr, y_expr, t_expr, it_expr, out_var):
    _block_counter[0] += 1
    n = _block_counter[0]
    xv, yv, tv, iv = "wx%d" % n, "wy%d" % n, "wt%d" % n, "wi%d" % n
    body = re.sub(r"\bx\b", xv, WAVE_BODY)
    body = re.sub(r"\by\b", yv, body)
    body = re.sub(r"\bt\b", tv, body)
    body = re.sub(r"\biterations\b", iv, body)
    # 最后一个return换成赋值
    idx = body.rindex("return ")
    end = body.index(";", idx)
    body = body[:idx] + out_var + " = " + body[idx + len("return "):end] + ";" + body[end + 1:]
    return "{\n float %s = %s; float %s = %s; float %s = %s; float %s = %s;\n%s\n}\n" % (
        xv, x_expr, yv, y_expr, tv, t_expr, iv, it_expr, body)


# ---------------------------------------------------------------------------------------------
# 资产工具
# ---------------------------------------------------------------------------------------------
def get_or_create(name, asset_class, factory):
    path = "%s/%s" % (PACKAGE_DIR, name)
    asset = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if asset is None:
        asset = asset_tools.create_asset(name, PACKAGE_DIR, asset_class, factory)
    return asset


def set_prop(obj, name, value):
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception as e:  # noqa
        fail("cannot set %s.%s: %s" % (obj.get_name(), name, str(e)[:150]))
        return False


def clear_graph(mat):
    # delete_all_material_expressions一次删不干净，重复到清空为止，免得重跑时留下一堆孤立节点。
    for _ in range(10):
        if mel.get_num_material_expressions(mat) == 0:
            return
        mel.delete_all_material_expressions(mat)
    if mel.get_num_material_expressions(mat) != 0:
        fail("could not clear %s" % mat.get_name())


def add_expr(mat, cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


def connect(src, src_out, dst, dst_in):
    if not mel.connect_material_expressions(src, src_out, dst, dst_in):
        fail("connect failed: %s.%s -> %s.%s" % (src.get_name(), src_out, dst.get_name(), dst_in))


def make_custom(mat, name, code, inputs, output_type, x, y):
    node = add_expr(mat, unreal.MaterialExpressionCustom, x, y)
    set_prop(node, "description", name)
    set_prop(node, "code", code)
    set_prop(node, "output_type", output_type)
    items = []
    for in_name in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", in_name)
        items.append(ci)
    set_prop(node, "inputs", items)
    return node


def collection_param(mat, mpc, param, x, y):
    node = add_expr(mat, unreal.MaterialExpressionCollectionParameter, x, y)
    set_prop(node, "collection", mpc)
    set_prop(node, "parameter_name", param)
    return node


def vector_param(mat, name, rgb, x, y):
    node = add_expr(mat, unreal.MaterialExpressionVectorParameter, x, y)
    set_prop(node, "parameter_name", name)
    set_prop(node, "default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return node


def scalar_param(mat, name, value, x, y):
    node = add_expr(mat, unreal.MaterialExpressionScalarParameter, x, y)
    set_prop(node, "parameter_name", name)
    set_prop(node, "default_value", value)
    return node


def recompile_and_save(mat, path):
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)
    warn("saved %s" % path)


# ---------------------------------------------------------------------------------------------
# MPC_Water
# ---------------------------------------------------------------------------------------------
def build_mpc():
    mpc = get_or_create("MPC_Water", unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    params = []
    for pname in ["WaterTime", "WaterBaseLevel", "WaterLightScale"]:
        p = unreal.CollectionScalarParameter()
        p.set_editor_property("parameter_name", pname)
        p.set_editor_property("default_value", 0.0)
        params.append(p)
    set_prop(mpc, "scalar_parameters", params)
    unreal.EditorAssetLibrary.save_asset("%s/MPC_Water" % PACKAGE_DIR)
    warn("saved MPC_Water")
    return mpc


# ---------------------------------------------------------------------------------------------
# M_WaterSurface
# ---------------------------------------------------------------------------------------------
def build_surface(mpc):
    mat = get_or_create("M_WaterSurface", unreal.Material, unreal.MaterialFactoryNew())
    clear_graph(mat)

    set_prop(mat, "two_sided", True)
    set_prop(mat, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    set_prop(mat, "shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    set_prop(mat, "translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    set_prop(mat, "tangent_space_normal", False)  # 法线是世界空间
    set_prop(mat, "max_world_position_offset_displacement", 400.0)  # 波浪最大位移(厘米)，给包围盒留余量

    world_pos = add_expr(mat, unreal.MaterialExpressionWorldPosition, -1400, 0)
    cam_pos = add_expr(mat, unreal.MaterialExpressionCameraPositionWS, -1400, 160)
    time_node = collection_param(mat, mpc, "WaterTime", -1400, 320)

    # 波浪高度(厘米)：iterations随到相机的距离连续变化(30米内12，600米外1)，C++浮力用12(玩家离相机不到30米)。
    height_code = ("float d = length(WP - CamPos);\n"
                   "float its = 12.0 - 11.0 * saturate((d - 3000.0) / 57000.0);\n"
                   "float h;\n" + wave_block("WP.x", "WP.y", "T", "its", "h") + "return h;")
    height = make_custom(mat, "WaterHeight", height_code, ["WP", "CamPos", "T"], unreal.CustomMaterialOutputType.CMOT_FLOAT1, -1000, 0)
    connect(world_pos, "", height, "WP")
    connect(cam_pos, "", height, "CamPos")
    connect(time_node, "", height, "T")

    up = add_expr(mat, unreal.MaterialExpressionConstant3Vector, -1000, 220)
    set_prop(up, "constant", unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    wpo = add_expr(mat, unreal.MaterialExpressionMultiply, -700, 0)
    connect(height, "", wpo, "A")
    connect(up, "", wpo, "B")
    mel.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    # 法线(世界空间)：对同一个表达式在(x,y)、(x+e,y)、(x,y+e)求值做有限差分，近处octave比几何多，远处法线渐渐拉平。
    normal_code = ("float d = length(WP - CamPos);\n"
                   "float its = 24.0 - 23.0 * saturate((d - 3000.0) / 20000.0);\n"
                   "float e = 25.0;\n"
                   "float hc; float hx; float hy;\n"
                   + wave_block("WP.x", "WP.y", "T", "its", "hc")
                   + wave_block("WP.x + e", "WP.y", "T", "its", "hx")
                   + wave_block("WP.x", "WP.y + e", "T", "its", "hy")
                   + "float3 n = normalize(float3(-(hx - hc), -(hy - hc), e));\n"
                   "float flat_amount = 0.8 * min(1.0, sqrt(d * 0.00002) * 1.1);\n"
                   "return normalize(lerp(n, float3(0.0, 0.0, 1.0), flat_amount));")
    normal = make_custom(mat, "WaterNormal", normal_code, ["WP", "CamPos", "T"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, -1000, 420)
    connect(world_pos, "", normal, "WP")
    connect(cam_pos, "", normal, "CamPos")
    connect(time_node, "", normal, "T")
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    # 外观(占位，可在材质里随便改)：深蓝绿底色、很光滑、较高不透明度。
    color = vector_param(mat, "WaterColor", (0.02, 0.14, 0.20), -700, 420)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = scalar_param(mat, "Roughness", 0.06, -700, 540)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = scalar_param(mat, "Specular", 0.6, -700, 620)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    opacity = scalar_param(mat, "Opacity", 0.82, -700, 700)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    recompile_and_save(mat, "%s/M_WaterSurface" % PACKAGE_DIR)
    return mat


# ---------------------------------------------------------------------------------------------
# PP_Underwater
# ---------------------------------------------------------------------------------------------
# 水面小波纹的坡度(不要求准确，只要一直在动)：4个不同方向/波长/速度的正弦，返回坡度(gx,gy)。
# 水下看水面的两处都用它：DistortUV(屏幕空间扰动画面)和主节点(扰动法线算菲涅尔)，所以两处公式必须一致。
def ripple_block(px, py, t, gx, gy):
    # 和水面现有材质(Water_Material)同一张法线贴图 T_Water_TilingNormal_With_Height_02_Softened：两层不同尺度/流速叠加，
    # 取切线空间法线的xy当坡度(gx,gy)。用SampleLevel避免在分支里用导数；贴图对象由脚本接到名为Nrm的输入上。
    return ("{" + chr(10) +
            " float2 wuv = float2(" + px + ", " + py + ");" + chr(10) +
            " float2 n1 = Texture2DSampleLevel(Nrm, NrmSampler, wuv * 0.0012 + float2(" + t + " * 0.020, " + t + " * 0.013), 0.0).xy * 2.0 - 1.0;" + chr(10) +
            " float2 n2 = Texture2DSampleLevel(Nrm, NrmSampler, wuv * 0.0041 + float2(" + t + " * -0.031, " + t + " * 0.024), 0.0).xy * 2.0 - 1.0;" + chr(10) +
            " " + gx + " += n1.x * 0.6 + n2.x * 0.4;" + chr(10) +
            " " + gy + " += n1.y * 0.6 + n2.y * 0.4;" + chr(10) +
            "}" + chr(10))


def build_underwater(mpc):
    mat = get_or_create("PP_Underwater", unreal.Material, unreal.MaterialFactoryNew())
    clear_graph(mat)

    set_prop(mat, "material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    set_prop(mat, "blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_BLOOM)

    scene_color = add_expr(mat, unreal.MaterialExpressionSceneTexture, -1400, 0)
    set_prop(scene_color, "scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    scene_depth = add_expr(mat, unreal.MaterialExpressionSceneTexture, -1400, 200)
    set_prop(scene_depth, "scene_texture_id", unreal.SceneTextureId.PPI_SCENE_DEPTH)
    screen_pos = add_expr(mat, unreal.MaterialExpressionScreenPosition, -1400, 400)  # 默认映射=视口UV
    cam_pos = add_expr(mat, unreal.MaterialExpressionCameraPositionWS, -1400, 520)
    time_node = collection_param(mat, mpc, "WaterTime", -1400, 640)
    base_level = collection_param(mat, mpc, "WaterBaseLevel", -1400, 760)
    light_scale = collection_param(mat, mpc, "WaterLightScale", -1400, 1480)
    absorption = vector_param(mat, "Absorption", (0.0040, 0.0012, 0.0008), -1400, 880)  # 每厘米的吸收(RGB)，红光吸收最快
    fog_color = vector_param(mat, "FogColor", (0.10, 0.50, 0.70), -1400, 1000)
    fog_density = scalar_param(mat, "FogDensity", 0.0002, -1400, 1120)  # 每厘米的散射雾浓度

    # 像素世界坐标 = 相机世界坐标 + (像素平移世界坐标 - 相机平移世界坐标)，不依赖LWC类型的PreViewTranslation。
    # 水面高度 = 相机所在格水位 + 波浪表达式(只用公式，不管像素落在哪个格，是否启用由C++开关blendable权重)。
    # 视线(相机->像素)在水面以下的那一段长度=水中路径，吸收/雾按它算。
    code = ("float depth = min(D.x, 5000000.0);\n"  # SceneTexture输出是float4，取R=线性场景深度(cm)
            "float3 sceneColor = Color.rgb;\n"
            "float3 absorb = Absorb.rgb;\n"
                        # 混合在色调映射之前(BL_SCENE_COLOR_BEFORE_BLOOM)，场景颜色是辐亮度*PreExposure。DebugMode 6的实测：估的基准(Lit*PreExposure)比阳光下的海底暗一个量级以上，
            # 对整屏采样的平均亮度读出来接近0(这条路不通)。所以雾的亮度基准=max(当前像素自己的场景亮度, 估的基准*KRef)：
            # 远处的海底像素用它自己没加雾的亮度(雾与海底亮度比固定=FogColor)，没有任何物体的区域(天空/地图外)用放大后的估算基准。
            "float lref = max(dot(sceneColor, float3(0.2126, 0.7152, 0.0722)), Lit * ResolvedView.PreExposure * KRef);\n"
            "float3 fogColor = Fog.rgb * Lit * ResolvedView.PreExposure * KRef * WScale;\n"
            # 与引擎SvPositionToTranslatedWorld相同：像素坐标(Parameters.SvPosition.xy)+设备深度乘SVPositionToTranslatedWorld。
            # 同一个像素取两个点：场景点(设备深度由线性深度转换)和视线在近裁切面上的起点(反向Z下近平面deviceZ=1)。
            "float4 hw = mul(float4(Parameters.SvPosition.xy, ConvertToDeviceZ(depth), 1.0), ResolvedView.SVPositionToTranslatedWorld);\n"
            "float3 pw = CamPos + (hw.xyz / hw.w - ResolvedView.TranslatedWorldCameraOrigin);\n"
            "float4 hn = mul(float4(Parameters.SvPosition.xy, 1.0, 1.0), ResolvedView.SVPositionToTranslatedWorld);\n"
            "float3 pn = CamPos + (hn.xyz / hn.w - ResolvedView.TranslatedWorldCameraOrigin);\n"
            "float hp;\n" + wave_block("pw.x", "pw.y", "T", "12.0", "hp") +
            "float hq;\n" + wave_block("pn.x", "pn.y", "T", "12.0", "hq") +
            # 判据(用户要求)：视线起点(近裁切面上的点)在水面下才加效果；起点在水面上的像素不处理(水面本身由水面材质画)。
            # 水中路径=起点到场景点的距离，若场景点在水面上(视线穿出水面)则截到水面交点。
            "float zs = Base + hp;\n"
            "float zsn = Base + hq;\n"
            "float zn = pn.z;\n"
            "float zp = pw.z;\n"
            # 水下看水面(用户要求)：不反射场景/物体，只算"没有任何物体时的纯颜色反射"(=雾色)，按菲涅尔(水->空气，含全反射)和透射混合。
            # 视线从水里穿出水面(水面交点比场景点近)才有这一步；水面mesh在相机潜入水里时由C++隐藏，所以穿出水面看到的是水面上的世界。
            "bool startUnder = zn < zsn;\n"
            "float3 dv = normalize(pn - CamPos);\n"
            "float tScene = min(length(pw - CamPos), 1000000.0);\n"
            "float ts = (dv.z > 0.0001) ? (zsn - CamPos.z) / dv.z : 1000000000.0;\n"
            "bool crosses = startUnder && ts < tScene;\n"
            "float path = startUnder ? (crosses ? ts : tScene) : 0.0;\n"
            "float f = startUnder ? 1.0 : 0.0;\n"
            "float3 transmit = exp(-absorb * path);\n"
            "float fogT = exp(-FogDensity * path);\n"
            # 水的颜色(穿出水面的像素)=雾色(同一个值：水下远处的海底被雾完全盖住时=fogColor，水平线上下两边才能接上)：必须是均匀的纯色，亮度不能跟像素自己的场景亮度走(否则被扰动的场景结构会通过亮度透出来，看起来像一直在透射)。亮度=估算基准*WaterColorScale，用DebugMode 6第4条带对比雾的亮度调。
                        "float refK = Lit * ResolvedView.PreExposure * KRef;\n"
            "float3 waterColor = fogColor;\n"
            "float3 inwater = sceneColor * transmit + (crosses ? waterColor : fogColor) * (1.0 - fogT);\n"
            # 水下看水面：不采样天空反射。扰动后的法线(随时间一直在动)和视线夹角 -> 菲涅尔反射率fres，透射率=1-fres；
            # 用透射率混合"穿出水面看到的原画面(已按水中路径衰减)"和"水的颜色"。画面本身在DistortUV节点里按同一组波纹做了屏幕空间扰动。
            "float gx = 0.0; float gy = 0.0;\n"
            "" + ripple_block("(CamPos.x + dv.x * min(ts, 1000000.0))", "(CamPos.y + dv.y * min(ts, 1000000.0))", "T", "gx", "gy") +
            "float3 nr = normalize(float3(-gx * Rip, -gy * Rip, 1.0));\n"
            "float cosi = saturate(abs(dot(dv, nr)));\n"
            # 简化的透射率：只看视线和竖直方向的夹角，透射率=dv.z^FresnelPower(正上方=1，越斜越小，水平=0)，反射率fres=1-透射率(纯水色，不采样天空)。
            "float fres = 1.0 - pow(saturate(dv.z), FPow);\n"
            "float3 result = crosses ? lerp(inwater, waterColor, fres) : inwater;\n"
            "if (!(result.x == result.x) || !(result.y == result.y) || !(result.z == result.z)) result = sceneColor;\n"
            # 调试开关(材质参数DebugMode，默认0=正常)：1直通原图 2水下比例f 3深度灰度 4红=视线起点在水面下/绿=场景点在水面下/蓝=NaN 5像素世界高度的等高线
            "float pe = lref;\n"
            # DebugMode 6：屏幕顶部三条灰带，亮度直接是预曝光空间的值(同一帧同一曝光下可以互相比较)：
            # 第1条(最上)=Lit*PreExposure(估的阳光下白面亮度) 第2条=雾的亮度基准lref(按像素) 第3条=当前像素自己的场景亮度
            "float vy = Parameters.SvPosition.y * ResolvedView.ViewSizeAndInvSize.w;\n"
            "float pixLum = dot(sceneColor, float3(0.2126, 0.7152, 0.0722));\n"
            "if (Dbg > 5.5) { if (vy < 0.05) return float3(1.0, 1.0, 1.0) * Lit * ResolvedView.PreExposure; if (vy < 0.10) return float3(1.0, 1.0, 1.0) * lref; if (vy < 0.15) return float3(1.0, 1.0, 1.0) * pixLum; if (vy < 0.20) return float3(1.0, 1.0, 1.0) * dot(waterColor, float3(0.2126, 0.7152, 0.0722)); return result; }\n"
            "if (Dbg > 4.5) return float3(1.0, 1.0, 1.0) * frac(pw.z / 1000.0) * pe;\n"
            "if (Dbg > 3.5) return float3(startUnder ? 1.0 : 0.0, zp < zs ? 1.0 : 0.0, (!(zs == zs) || !(zp == zp) || !(zsn == zsn)) ? 1.0 : 0.0) * pe;\n"
            "if (Dbg > 2.5) return float3(1.0, 1.0, 1.0) * saturate(depth / 100000.0) * pe;\n"
            "if (Dbg > 1.5) return float3(1.0, 1.0, 1.0) * (crosses ? fres : 0.0) * pe;\n"
            "if (Dbg > 0.5) return sceneColor;\n"
            "return result;")
    nrm_tex = add_expr(mat, unreal.MaterialExpressionTextureObject, -1400, 2080)
    set_prop(nrm_tex, "texture", unreal.load_asset("/Water/Textures/Normals/T_Water_TilingNormal_With_Height_02_Softened"))
    # 屏幕空间扰动：只对"起点在水面下、视线向上穿出水面"的像素，按水面小波纹坡度偏移采样画面的UV，一直在动。
    dist_src = ("float3 pn0 = float3(0.0, 0.0, 0.0);\n"
                "float4 hn0 = mul(float4(Parameters.SvPosition.xy, 1.0, 1.0), ResolvedView.SVPositionToTranslatedWorld);\n"
                "float3 pn1 = CamPos + (hn0.xyz / hn0.w - ResolvedView.TranslatedWorldCameraOrigin);\n"
                "float3 dv1 = normalize(pn1 - CamPos);\n"
                "float hq1;\n" + wave_block("pn1.x", "pn1.y", "T", "12.0", "hq1") +
                "float zsn1 = Base + hq1;\n"
                "float2 off = float2(0.0, 0.0);\n"
                "if (pn1.z < zsn1 && dv1.z > 0.02) {\n"
                "  float ts1 = (zsn1 - CamPos.z) / dv1.z;\n"
                # 只扰动"视线穿出水面"的像素：本像素的场景点比水面交点近(水下的角色/物体)就不动，否则水下物体会被波纹打碎
                "  float dep0 = min(D.x, 5000000.0);\n"
                "  float4 hs0 = mul(float4(Parameters.SvPosition.xy, ConvertToDeviceZ(dep0), 1.0), ResolvedView.SVPositionToTranslatedWorld);\n"
                "  float tScene0 = length(hs0.xyz / hs0.w - ResolvedView.TranslatedWorldCameraOrigin);\n"
                "  if (ts1 < tScene0) {\n"
                "    float gx1 = 0.0; float gy1 = 0.0;\n"
                + ripple_block("(CamPos.x + dv1.x * ts1)", "(CamPos.y + dv1.y * ts1)", "T", "gx1", "gy1") +
                "    off = float2(gx1, gy1) * Dist * Rip * saturate(dv1.z * 4.0);\n"
                # 偏移后采样到的点若是水下近处的物体(比水面交点近)就放弃偏移，否则角色的像素会被抖到背景上
                "    float2 uv2 = saturate(UV.xy + off);\n"
                "    float dep2 = min(SceneTextureLookup(uv2, 1, false).x, 5000000.0);\n"
                "    float4 hs2 = mul(float4(Parameters.SvPosition.xy + off * ResolvedView.ViewSizeAndInvSize.xy, ConvertToDeviceZ(dep2), 1.0), ResolvedView.SVPositionToTranslatedWorld);\n"
                "    if (length(hs2.xyz / hs2.w - ResolvedView.TranslatedWorldCameraOrigin) < ts1) off = float2(0.0, 0.0);\n"
                "  }\n"
                "}\n"
                "return saturate(UV.xy + off);")
    dist_node = make_custom(mat, "DistortUV", dist_src, ["UV", "CamPos", "Base", "T", "Dist", "Rip", "Nrm", "D"],
                            unreal.CustomMaterialOutputType.CMOT_FLOAT2, -1100, 0)
    connect(screen_pos, "", dist_node, "UV")
    connect(cam_pos, "", dist_node, "CamPos")
    connect(base_level, "", dist_node, "Base")
    connect(time_node, "", dist_node, "T")
    connect(scalar_param(mat, "DistortStrength", 0.04, -1400, 1960), "", dist_node, "Dist")
    connect(scalar_param(mat, "RippleStrength", 1.0, -1400, 1720), "", dist_node, "Rip")
    connect(nrm_tex, "", dist_node, "Nrm")
    connect(scene_depth, "", dist_node, "D")
    connect(dist_node, "", scene_color, "UVs")

    effect = make_custom(mat, "Underwater", code,
                         ["UV", "D", "CamPos", "T", "Base", "Color", "Absorb", "FogDensity", "Fog", "Dbg", "WScale", "Lit", "KRef", "Rip", "FPow", "Nrm"],
                         unreal.CustomMaterialOutputType.CMOT_FLOAT3, -900, 400)
    connect(screen_pos, "", effect, "UV")
    connect(scene_depth, "", effect, "D")
    connect(cam_pos, "", effect, "CamPos")
    connect(time_node, "", effect, "T")
    connect(base_level, "", effect, "Base")
    connect(scene_color, "", effect, "Color")
    connect(absorption, "", effect, "Absorb")
    connect(fog_density, "", effect, "FogDensity")
    connect(fog_color, "", effect, "Fog")
    connect(light_scale, "", effect, "Lit")
    connect(nrm_tex, "", effect, "Nrm")
    connect(scalar_param(mat, "RippleStrength", 1.0, -1400, 1720), "", effect, "Rip")
    connect(scalar_param(mat, "FresnelPower", 1.0, -1400, 1840), "", effect, "FPow")
    connect(scalar_param(mat, "FogRefScale", 15.0, -1400, 1600), "", effect, "KRef")
    connect(scalar_param(mat, "WaterColorScale", 40.0, -1400, 1360), "", effect, "WScale")
    connect(scalar_param(mat, "DebugMode", 0.0, -1400, 1240), "", effect, "Dbg")
    mel.connect_material_property(effect, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    recompile_and_save(mat, "%s/PP_Underwater" % PACKAGE_DIR)
    return mat


def main():
    if not unreal.EditorAssetLibrary.does_directory_exist(PACKAGE_DIR):
        unreal.EditorAssetLibrary.make_directory(PACKAGE_DIR)
    mpc = build_mpc()
    build_surface(mpc)
    build_underwater(mpc)
    if problems:
        warn("finished with %d problem(s), see PROBLEM lines above" % len(problems))
    else:
        warn("finished OK")


main()
