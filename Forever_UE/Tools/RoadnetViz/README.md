# RoadnetViz

地形路网可视化测试工具：跑Core的Map::InitTerrains/InitRoadnet，把地形、路网、lot画成PNG。

    RoadnetViz.exe [config.json] [out.png] [mapSize=1024] [pixelsPerElement=2]

默认config.json只启用三个地形mod和roadnet_checkboard。图例画在输出图片下方的白色条带里(GDI渲染中文，需Windows)。
路网随机(无seed参数)，每次运行不同。

相对路径的输出图片统一写到源码旁的 `output/`（已被git忽略），例如 `RoadnetViz.exe config.json 1.png` → `output/1.png`；绝对路径则原样写出。
