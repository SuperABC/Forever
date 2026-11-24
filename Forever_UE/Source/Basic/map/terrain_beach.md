# terrain_beach.h / terrain_beach.cpp

## 职责

`BeachTerrain`(`terrain_beach`)：沙滩地形的**登记项**——声明地形类型`terrain_beach`、地图界面颜色和地表材质，**不自动生成**任何格子(`DistributeTerrain`是空的)。沙滩格子由地图编辑器(`Tools/MapEditor`)用沙滩笔刷手绘，经`terrain_file`读入。

## 地表材质

`SetupTexture()`里`SetMaterial`声明两层、按水深选择：干沙(`Trampled_Beach_Sand_vmghdejo`)→湿沙(`Wet_Sand_rldapzp0`)，`TerrainMaterialSelector::WaterDepth`，范围`[-0.4, 0.0]`米——高出水面0.4米以上是干沙，水线(深度0)处是湿沙。水深来自UE侧顶点色(最近水面高度-地面高度，见`ForeverTerrainFrameworkComponent.md`"地形材质")，所以沙滩要紧挨着海/河水面才会出现湿沙渐变；离水远的沙滩是纯干沙。与海洋共用`wet_sand`层(同名层只加载一份贴图)。

## 调参

范围和`tile`(贴图平铺米数)都写在`SetupTexture()`里，改完重编Basic即可，不需要重新生成任何资产。
