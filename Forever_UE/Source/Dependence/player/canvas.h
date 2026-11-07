#pragma once

#include <cstdint>
#include <string>
#include <vector>


// 定长环形队列,纯POD不涉及堆分配:Canvas横跨UE模块和Basic.dll两侧使用,事件队列若用std::deque,
// 一侧push分配的块被另一侧pop释放会堆损坏。队列满了丢弃新事件。见canvas.md。
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

// 内存位图(BGRA,同Canvas像素缓冲区),纯内存拷贝/缩放。约定哪个模块创建的Bitmap就只由它扩容/释放(跨模块堆分配,见canvas.md)。
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

// 画布尺寸变化回调:裸函数指针+userData,不用std::function(跨模块堆分配,见canvas.md)。
using ResizeFunc = void(*)(void* userData, int width, int height);

// 键盘扩展键码:常规字符键直接用ASCII值,无ASCII字符的键用下面的常量(0x100以上避免重叠)。
enum KEY_CODE : int {
	KEY_BACKSPACE = '\b', KEY_TAB = '\t', KEY_ENTER = '\r', KEY_ESCAPE = 0x1b,
	KEY_LEFT = 0x100, KEY_UP, KEY_RIGHT, KEY_DOWN,
	KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
};

// PopKey()对"抬起"事件在键码上按位或这个标志区分"按下"/"抬起"，按下时PopKey()直接返回键码
// 本身。
constexpr int KEY_RELEASED_FLAG = 0x8000;

// 通用2D图形/输入画布(接口参考SGL),不依赖UE头,供Basic.dll这类Mod使用;文字渲染的实现细节(GDI/网页)和限制见canvas.md。
class Canvas {
public:
	Canvas();

	void Init(int width, int height); // 分配像素缓冲区，填充黑色不透明背景；不会触发ResizeFunc
	// 宿主侦测到视口尺寸变化后调用:重新分配缓冲区(旧内容不保留)并调已注册的ResizeFunc;尺寸没变则跳过。

	// 注册尺寸变化回调(PuzzleMod::Init里调用),同时只支持一个,重复调用覆盖,传nullptr取消。
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

	// 有宽度的直线:按线宽算出矩形(两个填充三角形)实心无缝;width<=1退化为1像素版本。
	void PutLine(int x1, int y1, int x2, int y2, int width);
	void PutRect(int x1, int y1, int x2, int y2, bool fill);
	void PutTriangle(int x1, int y1, int x2, int y2, int x3, int y3, bool fill);
	void PutCircle(int x, int y, int radius, bool fill);
	void PutEllipse(int x, int y, int radiusX, int radiusY, bool fill);
	void FloodFill(int x, int y, Color color);

	void GetImage(int left, int top, int right, int bottom, Bitmap& outBitmap) const;
	void PutImage(int left, int top, const Bitmap& bitmap);
	void PutRawImage(const uint8_t* srcBGRA, int srcWidth, int srcHeight, int x, int y, int dstWidth, int dstHeight);

	// 从磁盘加载图片到outBitmap(BGRA,保留alpha,路径UTF-8),失败返回false且outBitmap不变,格式见canvas.md。
	bool LoadImageFile(const std::string& path, Bitmap& outBitmap) const;

	// 缩放到dstWidth x dstHeight(<=0=原尺寸)画在(x,y),双线性采样,按图片alpha(再乘SetAlpha)混合。
	void PutBitmap(const Bitmap& bitmap, int x, int y, int dstWidth = 0, int dstHeight = 0);

	// 文字:SetFontSize设置字体像素高度;PutString/StringWidth支持UTF-8和'\n'换行。
	void SetFontSize(int size);
	void PutChar(char ch, int x, int y); // 单字符，内部转发给PutString(std::string(1,ch), x, y)
	int PutString(const std::string& text, int x, int y); // 支持'\n'换行，返回渲染总高度(像素)
	int StringWidth(const std::string& text) const; // 多行时返回最长一行的宽度
	int GetFontHeight() const; // 当前字号对应的单行实际高度(像素，含字体自身行距)，不产生
							   // 绘制副作用，调用方用来算垂直居中之类的布局(比如列表行内文字)

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

	// 鼠标滚轮:宿主PushMouseWheel注入(向上为正),ConsumeMouseWheel取走累计值并清零;不进mouseQueue。
	void PushMouseWheel(int delta);
	int ConsumeMouseWheel();

private:
	int width = 0, height = 0;
	std::vector<uint8_t> buffer; // BGRA

	uint8_t brushR = 255, brushG = 255, brushB = 255;
	float brushAlpha = 1.f;
	int fontSize = 16; // GDI字体像素高度，默认给个正常能看清的初始值

	// 用RingBuffer而不是std::deque——见RingBuffer类注释里跨模块堆分配的说明。
	RingBuffer<int, 128> keyQueue;
	MouseState mouseState;
	RingBuffer<MouseEvent, 32> mouseQueue;
	int wheelDelta = 0;

	ResizeFunc resizeFunc = nullptr;
	void* resizeUserData = nullptr;

	bool InBounds(int x, int y) const;
	void BlendPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, float alpha); // 按alpha跟已有像素混合后写入
	void FastLine(int x1, int x2, int y); // 内部辅助：填充图形用的水平扫描线(直接按当前画笔颜色/透明度画)
	void AllocateBuffer(); // 内部辅助：按当前width/height重新分配buffer并填充黑色不透明背景，Init/Resize共用
};

class PostHandle;

// mod读自己随包图片的统一入口:经PostHandle的"resource path"查询按文件名找到图片再解码,文件名约定见canvas.md。
bool LoadModImage(PostHandle* post, const std::string& fileName, Bitmap& outBitmap);
