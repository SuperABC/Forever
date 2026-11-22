# ForeverWaterFrameworkComponent

水面mesh + 水下后处理开关。mesh构造还原自原来的地形组件`BuildOceanMesh`；分级细分(LOD)版本因性能差已移除（git历史里没有提交，需要时参照`WaterWave.md`的设计重写）。

## 水面mesh
- `GenerateWater(map)`（由`AForeverFrameworkActor`在地形生成后调用一次）：整张图一个`ProceduralMeshComponent`（`OceanMesh`），共享顶点网格`(w+1)*(h+1)`，`map->GetWater(x,y).first`的格子写4个角Z=水位×1000（相邻格水位不同时后写覆盖），河流水面会倾斜，所以法线按面法线面积加权累加到顶点。无碰撞、不投影。
- 材质：引擎Water插件的`Water_Material_CustomMesh`（`LoadObject`+`MaterialInstanceDynamic`）。`Content/Asset/Water/M_WaterSurface`是之前自建的水面材质，当前**不使用**。

## 每帧（`UpdateWaterParameters`）
- 写`MPC_Water`：`WaterTime`、`WaterBaseLevel`（相机所在地形格水位×1000）。
- 相机所在地形格有水（`GetWater`，带范围保护）→ `PP_Underwater`的blendable权重1，否则0（挂在Sky组件的PostProcessVolume上，`GetPostProcessVolume()`）。
- 相机在所在格水面下（`kHideSurfaceDepth`=0）时把`oceanMesh`的相对缩放缩到1e-6（不用SetVisibility：它会重建整图mesh的渲染代理，相机在水面附近来回时严重掉帧且晚一帧）：水下看水面由后处理自己画，避免mesh自带的SSR反射。相机在水面上方时mesh始终显示（半水面画面从水上看水下正常）；曾取30cm，结果相机刚没入水面时mesh进场景深度、后处理的"穿出水面"判断和mesh深度打架导致闪烁、露出mesh的倒影，所以改成0。
- 后处理判据：视线在近裁切面上的起点在水面下才加效果（见`Tools/WaterMaterial/README.md`）。

## 波浪
`Water/WaterWave.inc`现在恒返回0（水面是平的），C++（`QueryWater`）和后处理里的水面高度都因此是`WaterBaseLevel`本身。要恢复波浪：改回`.inc`末尾的返回式并重跑`build_water_assets.py`，且水面材质需要有对应WPO（当前插件材质没有）。
