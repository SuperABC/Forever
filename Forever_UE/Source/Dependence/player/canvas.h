#pragma once

#include <cstdint>
#include <string>
#include <vector>


// 定长环形队列，纯POD、不涉及任何堆分配——Canvas实例常常横跨两个不同的二进制模块使用
// （比如UE的Forever模块通过UPuzzleWidget::NativeOnKeyDown调PushKey注入按键，运行时加载的
// Basic.dll里的PuzzleMod实现通过Loop()调PopKey/ClearKeyBuffer消费），而Canvas的这些方法都
// 是普通(非virtual)成员函数——canvas.cpp会被两侧各自的编译流程独立编译进各自的二进制，
// 各自链接各自的CRT/allocator。如果这里用std::deque这类会做堆分配的容器，UE模块这一侧
// push分配出来的内部块，被Basic.dll这一侧pop/clear时用它自己的operator delete去释放，
// 就是[[memory:cross_dll_allocator_crash]]那种跨模块堆损坏——这次是真实踩过的坑：加了
// 键鼠输入之后，PIE/打包运行时按一次键就直接静默退出、连崩溃报告都不弹（堆损坏往往来不及
// 走正常的异常处理）。定长数组+首尾游标完全不涉及堆，两侧谁调用都只是读写栈上/成员内的
// 固定内存，天然规避这个问题——原理上和SGL自己老工程Canvas实现里的IntRingBuf是同一个考虑。
template<typename T, int Capacity>
class RingBuffer {
public:
	bool Empty() const { return count == 0; }

	void Push(const T& value) {
		if (count >= Capacity) return; // 满了就丢弃这次新事件，简单处理，不追求"挤掉最老的"
		data[tail] = value;
		tail = (tail + 1) % Capacity;
		count++;
	}

	T Pop() {
		if (count == 0) return T();
		T value = data[head];
		head = (head + 1) % Capacity;
		count--;
		return value;
	}

	void Clear() { head = tail = count = 0; }

private:
	T data[Capacity]{};
	int head = 0, tail = 0, count = 0;
};

// 颜色（RGBA，0~255）。
struct Color {
	uint8_t r = 0, g = 0, b = 0, a = 255;
};

// 内存位图（BGRA，和Canvas像素缓冲区同一种格式），供GetImage/PutImage/PutRawImage这几个
// "内存位图搬运"操作用——不涉及任何文件编解码，纯内存拷贝/缩放。这几个方法目前没有被
// TetrisPuzzle用到，暂时还没有实际跨模块传递Bitmap的场景；但如果以后哪个PuzzleMod要跨
// Init/Loop之外的方式在两个模块间传递同一个Bitmap实例并互相扩容/释放，会撞上跟RingBuffer
// 类注释里同一个跨模块堆分配问题——保持"哪个模块的调用创建的Bitmap，就只由这个模块自己的
// 调用去扩容/释放"这个约定，不要让另一侧对同一个vector做mutable操作。
struct Bitmap {
	int width = 0, height = 0;
	std::vector<uint8_t> data;
};

// 鼠标实时状态快照。
struct MouseState {
	int x = 0, y = 0;
	bool buttons[3] = { false, false, false }; // 0=左键 1=右键 2=中键
};

// 一次鼠标按键事件。
struct MouseEvent {
	int x = 0, y = 0;
	int button = 0;
	bool down = false;
};

enum MOUSE_BUTTON : int { MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE };

// 画布尺寸变化回调——参考SGL的resize注册机制。裸函数指针+void* userData，不用std::function：
// 原因跟PuzzleFactory的CreateFunc/DestroyFunc一样，Canvas可能横跨UE模块和运行时加载的
// Basic.dll两侧使用（见RingBuffer类注释里的跨模块堆分配问题），std::function内部的闭包一旦
// 发生堆分配就会撞上同一个坑。userData让PuzzleMod的实现能把回调转发回自己的实例（注册一个
// static转发函数+this指针即可，不需要真正的成员函数指针类型）。
using ResizeFunc = void(*)(void* userData, int width, int height);

// 键盘扩展键码——常规字符键(字母/数字/空格等)直接用其ASCII值('a'/' '这种)传给PushKey/
// PopKey比较，只有没有对应ASCII字符的键才需要专门常量，数值特意选在0x100以上避免跟ASCII
// 范围重叠。
enum KEY_CODE : int {
	KEY_BACKSPACE = '\b', KEY_TAB = '\t', KEY_ENTER = '\r', KEY_ESCAPE = 0x1b,
	KEY_LEFT = 0x100, KEY_UP, KEY_RIGHT, KEY_DOWN,
	KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
};

// PopKey()对"抬起"事件在键码上按位或这个标志区分"按下"/"抬起"，按下时PopKey()直接返回键码
// 本身。
constexpr int KEY_RELEASED_FLAG = 0x8000;

// 通用2D图形/输入画布——参考SGL(github.com/SuperABC/SGL)的接口覆盖范围重新设计实现，不是
// 照抄，命名和这个项目其它类保持一致的PascalCase风格。纯C++、不#include任何UE头，供
// Basic.dll这类运行时加载的Mod使用（见[[memory:basic_is_runtime_dll]]）。
//
// 文字渲染：靠内置的5x7点阵ASCII字体（只覆盖数字/大写字母/常用标点，小写字母按大写渲染，
// 未覆盖的字符留空——见canvas.cpp里的字体表），不支持真正的字体切换/斜体/下划线（SGL的
// setFontName/setFontStyle这两个这次不做）。SGL自己独立的TEXT_MAP字符网格模式（
// setBfc/writeChar/writeString等一整套）也不做，这次只有BIT_MAP画布这一种。
class Canvas {
public:
	Canvas();

	void Init(int width, int height); // 分配像素缓冲区，填充黑色不透明背景；不会触发ResizeFunc
									   // (第一次创建画布，PuzzleMod::Init里直接用
									   // canvas->GetWidth()/GetHeight()拿初始尺寸即可，不需要
									   // 回调通知)

	// 宿主(UPuzzleWidget)侦测到真实视口尺寸变化后调用：重新分配像素缓冲区(旧内容不保留——
	// 反正PuzzleMod::Loop每帧都会整张重画)，再调用已注册的ResizeFunc(如果有)通知PuzzleMod
	// 调整内部布局(比如棋盘格子大小/居中偏移)。尺寸没变化时直接跳过，不会误触发回调。
	void Resize(int width, int height);

	// 注册尺寸变化回调，PuzzleMod::Init里调用。同一时刻只支持一个回调(和这个项目其它单播
	// 回调场景一致)，重复调用会覆盖前一个；传nullptr取消注册。
	void SetResizeFunc(ResizeFunc func, void* userData);

	int GetWidth() const;
	int GetHeight() const;
	const uint8_t* GetData() const; // BGRA，供UE桥接层贴图用
	int GetDataSize() const;

	void SetColor(int r, int g, int b);
	void SetAlpha(float alpha); // 0~1，之后的绘图操作跟画布已有像素按这个比例混合
	float GetAlpha() const;
	void ClearScreen();

	void PutPixel(int x, int y);
	Color GetPixel(int x, int y) const;
	void PutLine(int x1, int y1, int x2, int y2);
	void PutRect(int x1, int y1, int x2, int y2, bool fill);
	void PutTriangle(int x1, int y1, int x2, int y2, int x3, int y3, bool fill);
	void PutCircle(int x, int y, int radius, bool fill);
	void PutEllipse(int x, int y, int radiusX, int radiusY, bool fill);
	void FloodFill(int x, int y, Color color);

	void GetImage(int left, int top, int right, int bottom, Bitmap& outBitmap) const;
	void PutImage(int left, int top, const Bitmap& bitmap);
	void PutRawImage(const uint8_t* srcBGRA, int srcWidth, int srcHeight, int x, int y, int dstWidth, int dstHeight);

	// 文字：见类注释里"文字渲染"一节。SetFontSize按内置字体基准高度(7像素)的整数倍缩放。
	void SetFontSize(int size);
	void PutChar(char ch, int x, int y);
	int PutString(const std::string& text, int x, int y); // 支持'\n'换行，返回渲染总高度(像素)
	int StringWidth(const std::string& text) const; // 多行时返回最长一行的宽度

	// 键盘：宿主(UE桥接层)收到真实按键事件后调PushKey注入，小游戏逻辑调剩下三个消费。
	void PushKey(int code);
	bool HasPendingKey() const;
	int PopKey();
	void ClearKeyBuffer();

	// 鼠标：宿主调SetMousePos/PushMouseButton注入，小游戏逻辑调剩下几个消费。
	void SetMousePos(int x, int y);
	void PushMouseButton(int button, bool down);
	MouseState GetMouseState() const;
	bool HasPendingMouseEvent() const;
	MouseEvent PopMouseEvent();
	void ClearMouseBuffer();

private:
	int width = 0, height = 0;
	std::vector<uint8_t> buffer; // BGRA

	uint8_t brushR = 255, brushG = 255, brushB = 255;
	float brushAlpha = 1.f;
	int fontSize = 7; // 内置点阵字体基准高度是7像素，SetFontSize按比例整数倍缩放

	// 用RingBuffer而不是std::deque——见RingBuffer类注释里跨模块堆分配的说明。
	RingBuffer<int, 128> keyQueue;
	MouseState mouseState;
	RingBuffer<MouseEvent, 32> mouseQueue;

	ResizeFunc resizeFunc = nullptr;
	void* resizeUserData = nullptr;

	bool InBounds(int x, int y) const;
	void BlendPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, float alpha); // 按alpha跟已有像素混合后写入
	void FastLine(int x1, int x2, int y); // 内部辅助：填充图形用的水平扫描线(直接按当前画笔颜色/透明度画)
	void AllocateBuffer(); // 内部辅助：按当前width/height重新分配buffer并填充黑色不透明背景，Init/Resize共用
};
