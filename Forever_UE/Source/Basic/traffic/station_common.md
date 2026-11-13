# station_common.h / station_common.cpp

## 职责

TrainStation/AirStation共用的"贴路"坐标换算：`NearRoadPoint`、`AlongAxisHeadingDegrees`。

## 关键设计

- **两个坐标换算helper**(匿名namespace，三个类共用)：
  - `NearRoadPoint(direction, alongRatio, depthRatio)`——`alongRatio`是沿道路方向(frontage
    轴)的位置，`depthRatio`是垂直道路方向(0=贴着道路那条边，1=远离道路那条边)，按
    `direction`换算成quad局部坐标系下的`PointParams`。`depthRatio`在`FACE_EAST`/
    `FACE_NORTH`要翻转(`1-depthRatio`)，`alongRatio`任何方向都不翻转——因为`depthRatio`
    的"0=贴路"要跟着`direction`贴的是哪条边走，`alongRatio`只是沿同一条边平移，不需要跟着
    翻。
  - `AlongAxisHeadingDegrees(direction, towardIncreasing)`——沿frontage轴(`alongRatio`
    增大方向)的朝向角度，直接从`alongRatio`映射到的固定轴反推(`FACE_WEST`/`FACE_EAST`时是
    Y轴，`FACE_NORTH`/`FACE_SOUTH`时是X轴)，因此"朝`alongRatio`增大方向"永远只有两个可能
    角度(90°或0°)，和`direction`贴的是哪条边无关。不要用`FaceNormalDegrees(direction)
    ±90`这套公式代替——它对`EAST`/`NORTH`两个方向符号会算反。

## 依赖关系

- 被谁依赖：`station_train`、`station_air`。
