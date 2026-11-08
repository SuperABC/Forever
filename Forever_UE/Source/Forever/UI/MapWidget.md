# MapWidget.h / MapWidget.cpp

## 职责

`UMapWidget`：游戏内**地图**。M键打开、Esc/M关闭，支持滚轮缩放、拖拽平移，并显示玩家位置朝向标记。

## 关键设计

- 用`Dependence/player/canvas.h`的软件光栅`Canvas`把地形/大路/园区/建筑画一次，贴进一张`UTexture2D`，缩放/拖拽**不重新光栅化**，只改`DisplayImage`所贴动态材质的`UVScale`/`UVOffset`参数(材质参数名`MapTexture`/`UVScale`/`UVOffset`，在Class Defaults指定`baseMapMaterial`)。
- 与`PhoneWidget`/`PuzzleWidget`不同：canvas尺寸按`Map::GetSize()`×超采样倍数(上限`kMaxSuperSample`，目标`kTargetResolution`)算出的固定值，与屏幕分辨率无关；地图生成后内容不变，只在**第一次`OpenMap`**画一次(`DrawFullMap`+`BlitCanvasToImage`)，`NativeTick`只做材质参数和`PlayerMarker`的轻量更新。
- 显示尺寸：`ResizeDisplayImageToCoverViewport`按地图真实宽高比(不假设正方形)取"刚好盖满视口"的尺寸并居中，长宽比不匹配的轴裁出屏幕，`ClampUVOffset`按同一尺寸放宽平移范围，拖拽仍能看到被裁部分(铺满不留空白，靠拖拽看边缘)。每帧重算以兼容运行时改分辨率。
- 打开时是UI Only输入模式，Enhanced Input收不到键，所以Esc/M必须在`NativeOnKeyDown`里直接拦截(OpenMap把键盘焦点设给自己)；`ToggleMap`(M)与`CloseMap`(Esc，只关不开)的区别在此。
- `CloseButton`是给鼠标用户的备用关闭按钮，蓝图不绑事件也不影响Esc/M。

## 依赖关系

- 依赖：`Dependence/player/canvas.h`、Core `Map`、`UMaterialInstanceDynamic`。
- 被谁依赖：`Player/ForeverPlayerController`(`ToggleMap`/`HandleCloseMap`)。

## 待办/后续阶段

- 无。

## 时间
打开地图时记下当前全局时间缩放并设0暂停世界，关闭时还原成打开前的值(暂停面板、谜题、商店同一套做法)。
