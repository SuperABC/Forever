# Tools

开发辅助工具索引。除特别说明外都**不在** `Source/Framework.sln` 里，不随 `build.bat` 编译，需要时单独构建。

| 目录 | 用途 | 构建/运行 |
|---|---|---|
| AvatarGen | 生成市民头像 PNG（按年龄段/性别），输出 `Resource/Avatars`（被 git 忽略，新机器需重新生成） | Node.js：`npm install` 后 `node generate.mjs`，详见其 README |
| DoorRig | 用 UE Python 重写门骨骼网格的蒙皮权重 | `UnrealEditor-Cmd.exe Forever.uproject -run=pythonscript -script=Tools/DoorRig/fix_door_weights.py`，编辑器须关闭；环境变量 `DOOR_APPLY=1` 才写回，否则只打印 |
| JunctionTest | `Core/traffic/junction.h` 单测控制台程序，返回值=失败数 | `MSBuild Tools\JunctionTest\JunctionTest.vcxproj /p:Configuration=Release /p:Platform=x64`，运行 `x64\Release\JunctionTest.exe` |
| LayoutEditor | `.layout`（`Resource/Layouts/`）可视化编辑器，平面 + three.js 3D 视图，单个 HTML | 用 Chromium 系浏览器直接打开 `layout_editor.html`（需要 File System Access API 才能原地保存） |
| ModHost | 原生宿主：加载 mod DLL 单独运行 puzzle / 手机 app，可开窗口、可脚本化截图 | 见其 README（`ModHost.exe --list / --puzzle <id> --window / --phone ...`） |
| ModWeb | 把 puzzle/app 逻辑 + 手机外壳用 Emscripten 编成单个离线 html，拷到手机直接开 | 需先装 `.emsdk`（已被忽略），`build_web.bat` 产出 `dist\forever_mods.html` |
| RoadnetViz | 地形/路网/lot 渲染成 PNG 的可视化工具 | 自带 `Tools\RoadnetViz\RoadnetViz.sln`（含 Dependence/Core/Basic）：`MSBuild Tools\RoadnetViz\RoadnetViz.sln /p:Configuration=Release /p:Platform=x64` |
| VehicleRig | 新载具绑骨：FBX → 带轮子权重的 GLB（靠材质自动识别轮子/车头） | `python Tools/VehicleRig/make_vehicle_glb.py <fbx> <glb> --mesh ... --hub-mat ... --tire-mat ... --front-mats ...`，详见其 README |

## 相关

- 自动化测试管线在 `Source/Test`（`Source/Test/README.md`），不属于 Tools。
- 新机器环境搭建见仓库根目录 `ENVIRONMENT.md`。
