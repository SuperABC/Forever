# TrafficLight

把 `/Game/3rdParty/Retro_traffic_light/StaticMeshes/retro_traffic_light`(一个材质槽，三盏灯没分开，没有源文件)拆成框架和红/黄/绿三个灯罩，导入到 `/Game/Asset/TrafficLight/`，给 `ATrafficLightElement` 用。三步都要先关掉编辑器；可重复运行(已存在就覆盖)。

1. **导出**：`set TRAFFIC_LIGHT_DIR=<目录>`，`UnrealEditor-Cmd.exe Forever.uproject -run=pythonscript -script=<绝对路径>/Tools/TrafficLight/export_traffic_light.py -unattended -nosplash -nullrhi`。写出 `retro_traffic_light.obj` 和底色贴图 `retro_traffic_light_texture_0.png`，日志打印包围盒。
2. **拆分**：`python Tools/TrafficLight/split_traffic_light.py <导出目录> <输出目录>`(numpy+PIL)。按顶点焊接求连通分量(实测21个)，在UV中心采样底色贴图，偏红/黄/绿的平面圆片是灯罩(分不开时按高度兜底)，其余归框架；写出4个glb，坐标=OBJ/100，和原模型同原点同朝向(灯罩朝UE的-X，红249–266cm、黄227–244cm、绿206–223cm)。打印每个分量的归类和包围盒。
3. **导入**：`set TRAFFIC_LIGHT_DIR=<输出目录>`，`UnrealEditor-Cmd.exe Forever.uproject -ExecutePythonScript="<绝对路径>/Tools/TrafficLight/import_traffic_light.py" -unattended -stdout -FullStdOutLogOutput -nosplash`(不能用 `-run=pythonscript`：交换格式导入要同步内容浏览器，命令行模式下会崩)。建灯罩材质 `M_TrafficLamp`(用于ISM；发光=LampColor×lerp(0,100,逐实例数据0)，底色=灯色×0.12)和 `MI_TrafficLamp_Red/Yellow/Green`；框架沿用原材质 `Scene_-_Root`；**关掉Nanite**(工程默认给导入网格开Nanite，渲染数据变成减面的回退网格，框架少了一半三角形)。导入后再导出成 `<输出目录>/check/*.obj`。
4. **核对**：`python Tools/TrafficLight/check_traffic_light.py <导出目录> <输出目录>`，逐个三角形比对位置、绕序、UV。2026-11-26 结果：框架8649+灯罩17/15/15=8696，和原模型全部一致。
