# canvas.h / canvas.cpp

## 职责

`Canvas`：通用2D图形/输入画布(BGRA像素缓冲区+键鼠事件队列)，供小游戏(`PuzzleMod`)和手机App(`AppMod`)绘制与收取输入。接口覆盖范围参考SGL，命名和项目其它类保持PascalCase。

## 关键设计

- **不依赖UE头文件**，供`Basic.dll`这类运行时加载的Mod使用；但会用Windows GDI(`canvas.cpp`内部`#include <windows.h>`，不污染头文件)：字体渲染交给Windows自带字体库(默认微软雅黑，支持中英文+抗锯齿)，`SetFontSize`直接设置字体像素高度。只有一个固定默认字体，不做字体名/斜体/下划线切换，也不做SGL的文本字符网格模式。
- **跨模块堆分配约束**：`Canvas`常横跨UE模块(`UPuzzleWidget`注入按键)和`Basic.dll`(`PuzzleMod::Loop`消费)两侧使用，而它的方法是普通非virtual成员，`canvas.cpp`会被两侧各自编译、各自链接CRT/allocator。因此：
  - 键盘/鼠标事件队列用**定长环形队列`RingBuffer`**(纯POD，不涉及堆)，不用`std::deque`，否则一侧`push`分配的块被另一侧`pop`用自己的`operator delete`释放，会堆损坏。队列满了丢弃新事件。
  - `Bitmap`(`GetImage`/`PutImage`/`PutRawImage`)是`std::vector`：约定哪个模块创建的`Bitmap`就只由这个模块扩容/释放，不要让另一侧对同一个vector做修改。
  - 尺寸变化回调`ResizeFunc`是**裸函数指针+`void* userData`**，不用`std::function`，原因同上。
- **尺寸**：`Init(w,h)`分配缓冲区(不触发回调)；宿主侦测到视口尺寸变化后调`Resize`重新分配(旧内容不保留，反正每帧整张重画)，并调已注册的`ResizeFunc`通知PuzzleMod调整布局；尺寸没变直接跳过。同一时刻只支持一个回调。
- **宽线**：`PutLine(..., width)`不是多次1像素直线偏移(斜线上相邻线各自取整会有缝隙)，而是按线宽算出矩形(两个填充三角形)，复用`PutTriangle`的扫描线填充，保证实心无缝；`width<=1`退化成1像素Bresenham。
- **透明度**：`SetAlpha`(0~1)之后的所有绘图操作按这个比例和已有像素混合。
- **图片**：`LoadImageFile(path, Bitmap&)`用stb_image(`common/3rdParty/stb_image.h`，公共领域，实现编进`canvas.cpp`)从磁盘解码，支持PNG/JPEG/BMP/GIF(只取第一帧)/TGA/PSD/HDR/PIC/PNM(不支持WebP/SVG)，Windows下路径按UTF-8解读，输出BGRA并保留alpha。`PutBitmap(bitmap,x,y,w,h)`双线性缩放+按图片每像素alpha(再乘`SetAlpha`)混合；`PutImage`(直接覆盖)和`PutRawImage`(忽略图片alpha)保持原样。`LoadImage`是Win32宏(`LoadImageW`)，所以方法名叫`LoadImageFile`。
- **mod图片资源约定**：图片放在`Resource/Mod/<mod名>/`下，**文件名必须以`<mod名>_`开头**(如`Resource/Mod/Basic/Basic_anykey.png`)，因为查找是按文件名在`config.json`的`resource_paths`里递归匹配，前缀保证全局唯一。mod用自由函数`LoadModImage(post, "Basic_anykey.png", bitmap)`加载(内部走`PostHandle`的"resource path"查询+`LoadImageFile`)。`Bitmap`由mod这一侧创建和释放，符合上面的跨模块堆约定。网页版(`Tools/ModWeb`)把`Resource/Mod/Basic`用`--embed-file`内嵌进html，其它mod的图片要加到`build_web.bat`里。
- **键盘**：宿主调`PushKey`注入，逻辑调`HasPendingKey`/`PopKey`/`ClearKeyBuffer`消费。常规字符键直接用ASCII值，没有ASCII字符的键用`KEY_CODE`常量(0x100以上，避免和ASCII重叠)；"抬起"事件在键码上按位或`KEY_RELEASED_FLAG`。
- **鼠标**：宿主调`SetMousePos`/`PushMouseButton`注入，逻辑读`MouseState`/`PopMouseEvent`。滚轮是一个累计整数(`PushMouseWheel`/`ConsumeMouseWheel`)，不进事件队列——滚轮没有按下/抬起，也不该让只认按键的小游戏看到多余事件。

## 依赖关系

- 依赖：标准库；`canvas.cpp`内部用Windows GDI。
- 被谁依赖：`puzzle_mod.h`/`app_mod.h`；`Source/Forever/UI/PuzzleWidget`/`PhoneWidget`持有并注入输入、把像素贴成纹理；`Core/player/puzzle.*`/`phone.*`。

## 待办/后续阶段

- 无。
