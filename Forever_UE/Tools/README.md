# Tools

开发辅助工具索引。除特别说明外都**不在** `Source/Framework.sln` 里，不随 `build.bat` 编译，需要时单独构建。

| 目录 | 用途 | 构建/运行 |
|---|---|---|
| AvatarGen | 生成市民头像 PNG（按年龄段/性别），输出 `Resource/Avatars`（被 git 忽略，新机器需重新生成） | Node.js：`npm install` 后 `node generate.mjs`，详见其 README |
| DoorRig | 用 UE Python 重写门骨骼网格的蒙皮权重 | `UnrealEditor-Cmd.exe Forever.uproject -run=pythonscript -script=Tools/DoorRig/fix_door_weights.py`，编辑器须关闭；环境变量 `DOOR_APPLY=1` 才写回，否则只打印 |
| JunctionTest | `Core/traffic/junction.h` 单测控制台程序，返回值=失败数 | `MSBuild Tools\JunctionTest\JunctionTest.vcxproj /p:Configuration=Release /p:Platform=x64`，运行 `x64\Release\JunctionTest.exe` |
| LayoutEditor | `.layout`（`Resource/Layouts/`）可视化编辑器，平面 + three.js 3D 视图，单个 HTML | 用 Chromium 系浏览器直接打开 `layout_editor.html`（需要 File System Access API 才能原地保存） |
| MapEditor | 地形 + 路网 + 地块手工编辑器（地形字符串笔刷、高度/水、直线/曲线路、隧道/桥/爬升、T口/十字自动路口、城外连接点、地块），导出 `.terrain`/`.roadnet`，由 Basic 的 `terrain_file`/`roadnet_file` mod 读取；单个 HTML + three.js | 用 Chromium 系浏览器直接打开 `map_editor.html`；纯逻辑测试 `node tests/test_*.js`，详见其 README |
| ModHost | 原生宿主：加载 mod DLL 单独运行 puzzle / 手机 app，可开窗口、可脚本化截图 | 见其 README（`ModHost.exe --list / --puzzle <id> --window / --phone ...`） |
| ModWeb | 把 puzzle/app 逻辑 + 手机外壳用 Emscripten 编成单个离线 html，拷到手机直接开 | 需先装 `.emsdk`（已被忽略），`build_web.bat` 产出 `dist\forever_mods.html` |
| RoadnetViz | 地形/路网/lot 渲染成 PNG 的可视化工具 | 自带 `Tools\RoadnetViz\RoadnetViz.sln`（含 Dependence/Core/Basic）：`MSBuild Tools\RoadnetViz\RoadnetViz.sln /p:Configuration=Release /p:Platform=x64` |
| VehicleRig | 新载具绑骨：FBX → 带轮子权重的 GLB（靠材质自动识别轮子/车头） | `python Tools/VehicleRig/make_vehicle_glb.py <fbx> <glb> --mesh ... --hub-mat ... --tire-mat ... --front-mats ...`，详见其 README |
| split_rpg7.py | 把Fab的 `rpg7.glb` 拆成空发射器 `SM_RPG7_Launcher.glb` + 截尾火箭弹 `SM_RPG7_Rocket.glb`（纯python+numpy，`CUT_Y`控制火箭弹截尾位置） | `python Tools/split_rpg7.py`，输出到脚本内配置的目录 |
| import_rpg7.py | 把上面两个glb导入 `/Game/3rdParty/RPG7/StaticMeshes/`，材质指向现有RPG7材质，日志打印包围盒 | 环境变量 `RPG7_GLB_DIR`=glb目录；`UnrealEditor-Cmd.exe Forever.uproject -ExecutePythonScript="<绝对路径>" -unattended -stdout -FullStdOutLogOutput`，编辑器须关闭 |
| Furniture | 住宅家具：`make_furniture.py` 纯python生成57个盒体低模glb(26类×款式变体，单张64x64调色板贴图，色块4x4像素且导入时设为不压缩——DXT压缩会把挨着的1像素色块糊成灰色；材质里贴图色再过一道超饱和`SATURATION_BOOST`，抵消本工程日间重曝光的冲淡；所有家具网格在生成时`recenter()`X/Y居中(布局按居中矩形放，不居中会穿墙)；水槽/洗手盆/浴缸的水面都不和相邻面共面(避免闪烁)；命令行建的材质必须显式勾`used_with_instanced_static_meshes`/`used_with_nanite`，否则独立运行的游戏里退回引擎默认灰材质(整片暗灰)，`import_furniture.py`已设；`inspect_furniture.py`核对材质连接/贴图设置/网格朝向，`inspect_flags.py`对比材质使用标志)+`furniture.json`+C++尺寸表头；`import_furniture.py` 导入到 `/Game/Asset/Room/{Mesh,Material,Texture}`并建共用材质`M_Furniture`；`make_preview_data.py`+`preview.html` 把CoreTest导出的布局渲成俯视预览页 | `python Tools/Furniture/make_furniture.py <输出目录> --header Source/Basic/map/room_furniture_catalog.h`；导入：环境变量 `FURNITURE_DIR`=输出目录，`UnrealEditor-Cmd.exe Forever.uproject -ExecutePythonScript="<import_furniture.py绝对路径>" -unattended -stdout -FullStdOutLogOutput`，编辑器须关闭；预览：`set FURNITURE_DUMP=<x>.json` 跑 `CoreTest.exe Resource\Config\config.json --suite mods`，再 `python Tools/Furniture/make_preview_data.py <x>.json <输出>.html` |
| make_rocket_blueprint.py | 创建 `/Game/Asset/Projectile/BP_RPG7Rocket`（父类 `AForeverRocketProjectile`，只填参数），须先跑import_rpg7 | 同上调用方式 |
| make_effect_blueprints.py | 创建爆炸等特效蓝图 | 同上调用方式 |

## 相关

- 自动化测试管线在 `Source/Test`（`Source/Test/README.md`），不属于 Tools。
- 新机器环境搭建见仓库根目录 `ENVIRONMENT.md`。
