# phone.h / phone.cpp

## 职责

`Phone`：手机UI/多任务外壳，**不是`<Concept>Factory`家族成员、不可被Mod替换**，是项目自己的固定逻辑。三态设计：`Home`(桌面图标网格)/`InApp`(当前App全屏)/`TaskList`(最近任务缩略图切换器)。

## 关键设计

- **布局按画布宽高比例算**：画布分辨率随宿主(`UPhoneWidget`)显示尺寸变化，所以网格列宽/图标大小/底部工具栏高度都由`RecomputeLayout`按比例重算，不硬编码像素。
- **生命周期**：跟`Puzzle`不同，`Phone`需要跨"开手机->关手机->再开手机"保持状态(多任务/最近任务要留着)，所以只在第一次打开手机时创建一次，此后由宿主Widget只切换可见性，不`delete`/重建。`Init`首次创建时算布局并枚举已启用的App类型；`Resize`只重算布局、**不重置**`state`/`entries`/当前打开的App。
- **`AppEntry`直接持有`App*`实例**；任务列表缩略图用"离开InApp状态前存的原始BGRA快照"，**快照的宽高单独记录**，不复用`Phone`当前宽高——拍完快照后用户可能拖动窗口改变画布尺寸，`PutRawImage`必须按快照自己实际尺寸解读像素，否则读越界。
- **桌面图标**来自`AppFactory::GetRegisteredIds()`，跳过`"app_empty"`占位id。
- **输入分层**：`InApp`状态下只拦截`Phone`自己关心的几个键(ESC/Tab/Space/Backspace)，其余按键/鼠标事件原样重新入队，留给当前App的`Loop`消费。ESC语义是"退一级"：只有`Home`态的ESC会置`pendingClose`，`Loop`返回非0请求关闭整个手机UI。
- 主屏日期/时间通过`post`查"game time"，见`Core/common/implement.cpp`。

## 依赖关系

- 依赖：`player/app.h`、`player/canvas.h`、`common/handle.h`。
- 被谁依赖：`Source/Forever/UI/PhoneWidget`持有并每帧调用。

## 待办/后续阶段

- 无。
