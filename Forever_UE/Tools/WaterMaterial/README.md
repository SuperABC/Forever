# WaterMaterial

用编辑器 Python 生成 `Content/Asset/Water/` 下的 `MPC_Water`、`M_WaterSurface`、`PP_Underwater`。波浪公式读自 `Source/Forever/Water/WaterWave.inc` 并内联进 Custom 节点。

## 运行（编辑器必须关闭）
```
UnrealEditor-Cmd.exe C:/Workspace/UE5/Projects/Forever/Forever_UE/Forever.uproject -run=pythonscript -script=C:/Workspace/UE5/Projects/Forever/Forever_UE/Tools/WaterMaterial/build_water_assets.py -unattended -nopause -nosplash -stdout -FullStdOutLogOutput
```
- uproject 必须绝对路径；日志找 `[build_water_assets] finished OK`。
- 验证 shader 能编译：加 `-AllowCommandletRendering` 跑一个读 `MaterialEditingLibrary.get_statistics` 的脚本（指令数>0 即成功），并 grep 日志 `Failed to compile`。
- 脚本幂等：`clear_graph` 清空旧节点后重建（M_WaterSurface 11 个、PP_Underwater 10 个表达式）。
- 改波浪只改 `WaterWave.inc` 再重跑；水色/吸收/雾等参数在脚本里的向量/标量参数默认值，也可在编辑器里改材质实例。

## PP_Underwater 的效果（2026-11-22，用户验收）
- 判据：视线在近裁切面上的起点在水面下才处理；路径=起点到场景点（视线穿出水面则截到水面交点）。
- 雾：`result = scene*exp(-Absorption*path) + fogColor*(1-exp(-FogDensity*path))`，`FogDensity`=0.0002/cm。
- 雾色/水色亮度：`fogColor = FogColor * Lit*PreExposure*FogRefScale(15) * WaterColorScale(40)`，固定值，不跟像素亮度走；穿出水面像素的水色=同一个fogColor，所以水平线上下两边能接上。混合在色调映射前，曝光参数不动。
- 看水面（视线穿出水面）：透射率=`dv.z^FresnelPower`(1.5，dv.z=视线与竖直方向夹角的余弦)，反射率=1-透射率，只混纯水色，不采样天空/场景；不做全反射贴图。
- 屏幕空间扰动：用插件水面材质同一张法线贴图(`T_Water_TilingNormal_With_Height_02_Softened`，世界XY两层平移)偏移采样UV，`DistortStrength`/`RippleStrength`控制；只对视线穿出水面、且偏移前后采样点都在水面外的像素生效(水下物体不抖)。
- C++：相机一低于水面(`kHideSurfaceDepth`=0)就把水面mesh缩放到1e-6隐藏(不用SetVisibility，会重建整图mesh代理导致掉帧/晚一帧)；组件在`TG_PostUpdateWork`读相机。
- `DebugMode`：1直通 2穿出水面像素的反射率 3深度灰度 4红=起点在水面下/绿=场景点在水面下/蓝=NaN 5世界高度等高线 6顶部四条亮度带(Lit*P/lref/像素亮度/水色)。
- FogColor是固定颜色，亮度基准随昼夜光照度变（`WaterLightScale`），不随曝光变。
