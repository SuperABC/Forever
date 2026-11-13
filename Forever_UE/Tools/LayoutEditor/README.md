# LayoutEditor

`.layout`（`Resource/Layouts/`）的可视化编辑器，单个 HTML，用 Chromium 系浏览器直接打开 `layout_editor.html`（file://，需要 File System Access API 才能原地保存）。格式说明见文件头注释和 `Source/Core/map/building.md`。

## 视图

- **平面**：中间区域上方切换"平面 / 3D"，右上角"重置视图并重读"按钮：平面复位缩放/平移，3D 复位相机，并重新读取磁盘上的当前文件（有未保存修改会先确认；没有磁盘句柄的文件只重置视图）。
  - 鼠标在画布上滚轮缩放（以光标为中心，0.25x–8x）；按住**空格 + 左键**拖动平移。
  - 缩放/平移只影响显示，不改数据；框选、点击命中随缩放自动对应。
- **3D**（three.js r149，本地 `vendor/three.min.js`，离线可用）：
  - 左键拖拽旋转，右键拖拽平移，滚轮缩放；`WASD` 水平移动，`Q/E` 升降，按住 `Shift` 加速（仅在 3D 视图且焦点不在输入框时生效）。
  - 顶部"层高(默认0.4) / 墙厚(默认0.02)"只是 3D 显示参数（记在浏览器 localStorage，不写入 .layout）。
  - 显示内容：地面/天花板/洞口、房间墙体、行人/车辆导航点与连线。楼梯/斜坡/电梯本体不画，只画它们带的墙；门窗不画模型，`doors`/`windows` 的位置在墙上切洞：水平范围沿墙，竖直范围 `y = ratio*层高 + offset` 且**从天花板往下量**（与 Core `BuildDoorsForElement` 一致），所以门窗的上下沿由 layout 决定，没有任何默认高度。
  - 3D 模式下平面的显示模式下拉框被禁用；编辑数据后 3D 会自动重建。

## three.js 来源

`vendor/three.min.js` 取自 `npm pack three@0.149.0`（`build/three.min.js`，MIT，许可证见 `vendor/three.LICENSE`）。r150 起官方不再提供经典全局脚本构建，file:// 下 ES module 会被浏览器拦截，所以固定在 r149。
