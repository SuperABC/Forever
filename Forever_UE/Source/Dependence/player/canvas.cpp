#include "canvas.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#define NOMINMAX
#include <windows.h>

#pragma comment(lib, "gdi32.lib")


using namespace std;

namespace {
	// GDI字体渲染——逐参数照抄老工程E:\Projects\Forever_UE\Source\Dependence\player\canvas.cpp
	// 的PutString实现(建内存DC+HFONT，黑底白字TextOutW，GetDIBits读回灰度当覆盖率混合进
	// 画布)。第一版这里自己换了几个参数(负数高度/FW_NORMAL/OUT_DEFAULT_PRECIS/
	// ANTIALIASED_QUALITY/FF_DONTCARE)，结果实际渲染出来每个字母粗细/字形都不一致，明显不对；
	// 老工程这一套参数组合是经过验证的，这次原样照抄，不再自己"改进"。
	//
	// 这些辅助函数不做成Canvas的成员方法——HDC/HFONT这些Windows类型不能出现在canvas.h的
	// 声明里(会把<windows.h>连带min/max/TEXT等宏定义带进每一个#include了canvas.h的文件，
	// 这个头文件被Forever模块的PuzzleWidget.h/PhoneWidget.h广泛引用，跟UE自己的头文件混在
	// 一起容易冲突)，所以GDI相关的实现细节整个封在这个.cpp的匿名namespace里。
	const wchar_t* kDefaultFontName = L"Microsoft YaHei"; // 中英文都覆盖，跟老工程默认字体一致

	HFONT CreateCanvasFont(int pixelHeight) {
		return CreateFontW(max(1, pixelHeight), 0, 0, 0, FW_THIN, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_CHARACTER_PRECIS, CLIP_CHARACTER_PRECIS,
			DEFAULT_QUALITY, FF_MODERN, kDefaultFontName);
	}

	wstring Utf8ToWide(const string& text) {
		if (text.empty()) return wstring();
		int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
		if (wlen <= 1) return wstring();
		wstring result(static_cast<size_t>(wlen) - 1, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), wlen);
		return result;
	}

	int MeasureLineHeight(int pixelHeight) {
		HDC screenDC = GetDC(nullptr);
		HDC memDC = CreateCompatibleDC(screenDC);
		HFONT font = CreateCanvasFont(pixelHeight);
		HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, font));

		TEXTMETRICW metrics;
		GetTextMetricsW(memDC, &metrics);
		int lineHeight = metrics.tmHeight + metrics.tmExternalLeading;

		SelectObject(memDC, oldFont);
		DeleteObject(font);
		DeleteDC(memDC);
		ReleaseDC(nullptr, screenDC);
		return lineHeight > 0 ? lineHeight : pixelHeight;
	}

	int MeasureLineWidth(const wstring& wline, int pixelHeight) {
		if (wline.empty()) return 0;

		HDC screenDC = GetDC(nullptr);
		HDC memDC = CreateCompatibleDC(screenDC);
		HFONT font = CreateCanvasFont(pixelHeight);
		HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, font));

		SIZE sz;
		GetTextExtentPoint32W(memDC, wline.c_str(), static_cast<int>(wline.size()), &sz);

		SelectObject(memDC, oldFont);
		DeleteObject(font);
		DeleteDC(memDC);
		ReleaseDC(nullptr, screenDC);
		return sz.cx;
	}

	// 画一行文字(不含'\n')到(x,y)：黑底白字渲染到一张内存位图上，读回灰度值当覆盖率，按
	// 当前画笔颜色/alpha跟buffer已有像素混合——和Canvas::BlendPixel同样的"按alpha插值"算法，
	// 这里内联展开(BlendPixel是Canvas的私有成员方法，这里是.cpp里的自由函数，直接操作
	// buffer/canvasWidth/canvasHeight这几个传进来的引用/值)。
	void DrawLine(vector<uint8_t>& buffer, int canvasWidth, int canvasHeight,
		const wstring& wline, int x, int y, int pixelHeight,
		uint8_t brushR, uint8_t brushG, uint8_t brushB, float brushAlpha) {
		if (wline.empty()) return;

		HDC screenDC = GetDC(nullptr);
		HDC memDC = CreateCompatibleDC(screenDC);
		HFONT font = CreateCanvasFont(pixelHeight);
		HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, font));

		SIZE sz;
		GetTextExtentPoint32W(memDC, wline.c_str(), static_cast<int>(wline.size()), &sz);
		if (sz.cx <= 0 || sz.cy <= 0) {
			SelectObject(memDC, oldFont);
			DeleteObject(font);
			DeleteDC(memDC);
			ReleaseDC(nullptr, screenDC);
			return;
		}

		BITMAPINFOHEADER bi = {};
		bi.biSize = sizeof(BITMAPINFOHEADER);
		bi.biWidth = sz.cx;
		bi.biHeight = sz.cy; // 正数=自底向上存储(GDI默认)，下面读回时按这个顺序翻转
		bi.biPlanes = 1;
		bi.biBitCount = 24;
		bi.biCompression = BI_RGB;

		HBITMAP bitmap = CreateCompatibleBitmap(screenDC, sz.cx, sz.cy);
		HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDC, bitmap));

		RECT rc = { 0, 0, sz.cx, sz.cy };
		FillRect(memDC, &rc, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
		SetTextColor(memDC, RGB(255, 255, 255));
		SetBkMode(memDC, TRANSPARENT);
		TextOutW(memDC, 0, 0, wline.c_str(), static_cast<int>(wline.size()));

		int rowStride = (sz.cx * 3 + 3) & ~3; // DIB每行按4字节对齐
		vector<uint8_t> bits(static_cast<size_t>(rowStride) * sz.cy);
		GetDIBits(memDC, bitmap, 0, sz.cy, bits.data(), reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);

		for (int row = 0; row < sz.cy; row++) {
			int srcRow = sz.cy - 1 - row; // DIB自底向上存储，第0行对应位图最下面一行
			for (int col = 0; col < sz.cx; col++) {
				uint8_t gray = bits[static_cast<size_t>(srcRow) * rowStride + col * 3];
				if (!gray) continue;

				int px = x + col, py = y + row;
				if (px < 0 || px >= canvasWidth || py < 0 || py >= canvasHeight) continue;

				float alpha = (gray / 255.f) * brushAlpha;
				size_t idx = (static_cast<size_t>(py) * canvasWidth + px) * 4;
				buffer[idx + 0] = static_cast<uint8_t>(buffer[idx + 0] * (1.f - alpha) + brushB * alpha);
				buffer[idx + 1] = static_cast<uint8_t>(buffer[idx + 1] * (1.f - alpha) + brushG * alpha);
				buffer[idx + 2] = static_cast<uint8_t>(buffer[idx + 2] * (1.f - alpha) + brushR * alpha);
				buffer[idx + 3] = 255;
			}
		}

		SelectObject(memDC, oldBitmap);
		SelectObject(memDC, oldFont);
		DeleteObject(bitmap);
		DeleteObject(font);
		DeleteDC(memDC);
		ReleaseDC(nullptr, screenDC);
	}
}

Canvas::Canvas() {

}

void Canvas::Init(int inWidth, int inHeight) {
	width = inWidth;
	height = inHeight;
	AllocateBuffer();
}

void Canvas::Resize(int inWidth, int inHeight) {
	if (inWidth == width && inHeight == height) return;
	width = inWidth;
	height = inHeight;
	AllocateBuffer();
	if (resizeFunc) resizeFunc(resizeUserData, width, height);
}

void Canvas::SetResizeFunc(ResizeFunc func, void* userData) {
	resizeFunc = func;
	resizeUserData = userData;
}

void Canvas::AllocateBuffer() {
	buffer.assign(static_cast<size_t>(width) * height * 4, 0);
	for (size_t i = 3; i < buffer.size(); i += 4) buffer[i] = 255; // A通道恒为不透明
}

int Canvas::GetWidth() const { return width; }
int Canvas::GetHeight() const { return height; }
const uint8_t* Canvas::GetData() const { return buffer.data(); }
int Canvas::GetDataSize() const { return static_cast<int>(buffer.size()); }

void Canvas::SetColor(int r, int g, int b) {
	brushR = static_cast<uint8_t>(clamp(r, 0, 255));
	brushG = static_cast<uint8_t>(clamp(g, 0, 255));
	brushB = static_cast<uint8_t>(clamp(b, 0, 255));
}

void Canvas::SetAlpha(float alpha) { brushAlpha = clamp(alpha, 0.f, 1.f); }
float Canvas::GetAlpha() const { return brushAlpha; }

void Canvas::ClearScreen() {
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			size_t idx = (static_cast<size_t>(y) * width + x) * 4;
			buffer[idx + 0] = 0; buffer[idx + 1] = 0; buffer[idx + 2] = 0; buffer[idx + 3] = 255;
		}
	}
}

bool Canvas::InBounds(int x, int y) const {
	return x >= 0 && y >= 0 && x < width && y < height;
}

void Canvas::BlendPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, float alpha) {
	if (!InBounds(x, y)) return;
	size_t idx = (static_cast<size_t>(y) * width + x) * 4;
	if (alpha >= 1.f) {
		buffer[idx + 0] = b; buffer[idx + 1] = g; buffer[idx + 2] = r; buffer[idx + 3] = 255;
		return;
	}
	buffer[idx + 0] = static_cast<uint8_t>(buffer[idx + 0] * (1.f - alpha) + b * alpha);
	buffer[idx + 1] = static_cast<uint8_t>(buffer[idx + 1] * (1.f - alpha) + g * alpha);
	buffer[idx + 2] = static_cast<uint8_t>(buffer[idx + 2] * (1.f - alpha) + r * alpha);
	buffer[idx + 3] = 255;
}

void Canvas::PutPixel(int x, int y) {
	BlendPixel(x, y, brushR, brushG, brushB, brushAlpha);
}

Color Canvas::GetPixel(int x, int y) const {
	Color result;
	if (!InBounds(x, y)) return result;
	size_t idx = (static_cast<size_t>(y) * width + x) * 4;
	result.b = buffer[idx + 0]; result.g = buffer[idx + 1]; result.r = buffer[idx + 2]; result.a = buffer[idx + 3];
	return result;
}

void Canvas::PutLine(int x1, int y1, int x2, int y2) {
	// Bresenham直线算法
	int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
	int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
	int err = dx + dy;
	int x = x1, y = y1;
	while (true) {
		PutPixel(x, y);
		if (x == x2 && y == y2) break;
		int e2 = 2 * err;
		if (e2 >= dy) { err += dy; x += sx; }
		if (e2 <= dx) { err += dx; y += sy; }
	}
}

void Canvas::FastLine(int x1, int x2, int y) {
	if (x1 > x2) swap(x1, x2);
	for (int x = x1; x <= x2; x++) PutPixel(x, y);
}

void Canvas::PutRect(int x1, int y1, int x2, int y2, bool fill) {
	if (x1 > x2) swap(x1, x2);
	if (y1 > y2) swap(y1, y2);
	if (fill) {
		for (int y = y1; y <= y2; y++) FastLine(x1, x2, y);
	}
	else {
		FastLine(x1, x2, y1);
		FastLine(x1, x2, y2);
		for (int y = y1; y <= y2; y++) { PutPixel(x1, y); PutPixel(x2, y); }
	}
}

namespace {
	// 三角形重心坐标测试——PutTriangle填充用，简单直接、不追求扫描线效率(画布小、调用少)。
	float EdgeFunction(int ax, int ay, int bx, int by, int px, int py) {
		return static_cast<float>((bx - ax) * (py - ay) - (by - ay) * (px - ax));
	}
}

void Canvas::PutTriangle(int x1, int y1, int x2, int y2, int x3, int y3, bool fill) {
	if (!fill) {
		PutLine(x1, y1, x2, y2);
		PutLine(x2, y2, x3, y3);
		PutLine(x3, y3, x1, y1);
		return;
	}

	int minX = min({ x1, x2, x3 }), maxX = max({ x1, x2, x3 });
	int minY = min({ y1, y2, y3 }), maxY = max({ y1, y2, y3 });
	for (int y = minY; y <= maxY; y++) {
		for (int x = minX; x <= maxX; x++) {
			float w0 = EdgeFunction(x2, y2, x3, y3, x, y);
			float w1 = EdgeFunction(x3, y3, x1, y1, x, y);
			float w2 = EdgeFunction(x1, y1, x2, y2, x, y);
			bool hasNeg = (w0 < 0) || (w1 < 0) || (w2 < 0);
			bool hasPos = (w0 > 0) || (w1 > 0) || (w2 > 0);
			if (!(hasNeg && hasPos)) PutPixel(x, y);
		}
	}
}

void Canvas::PutCircle(int x, int y, int radius, bool fill) {
	if (radius <= 0) return;
	for (int dy = -radius; dy <= radius; dy++) {
		for (int dx = -radius; dx <= radius; dx++) {
			float dist = sqrtf(static_cast<float>(dx * dx + dy * dy));
			if (fill) {
				if (dist <= radius + 0.5f) PutPixel(x + dx, y + dy);
			}
			else {
				if (fabsf(dist - radius) < 0.75f) PutPixel(x + dx, y + dy);
			}
		}
	}
}

void Canvas::PutEllipse(int x, int y, int radiusX, int radiusY, bool fill) {
	if (radiusX <= 0 || radiusY <= 0) return;
	for (int dy = -radiusY; dy <= radiusY; dy++) {
		for (int dx = -radiusX; dx <= radiusX; dx++) {
			float value = (dx * dx) / static_cast<float>(radiusX * radiusX) + (dy * dy) / static_cast<float>(radiusY * radiusY);
			if (fill) {
				if (value <= 1.f) PutPixel(x + dx, y + dy);
			}
			else {
				if (fabsf(value - 1.f) < 0.08f) PutPixel(x + dx, y + dy);
			}
		}
	}
}

void Canvas::FloodFill(int x, int y, Color color) {
	if (!InBounds(x, y)) return;
	Color target = GetPixel(x, y);
	if (target.r == color.r && target.g == color.g && target.b == color.b) return;

	vector<pair<int, int>> stack;
	stack.emplace_back(x, y);
	while (!stack.empty()) {
		auto [cx, cy] = stack.back();
		stack.pop_back();
		if (!InBounds(cx, cy)) continue;

		Color current = GetPixel(cx, cy);
		if (current.r != target.r || current.g != target.g || current.b != target.b) continue;

		size_t idx = (static_cast<size_t>(cy) * width + cx) * 4;
		buffer[idx + 0] = color.b; buffer[idx + 1] = color.g; buffer[idx + 2] = color.r; buffer[idx + 3] = 255;

		stack.emplace_back(cx + 1, cy);
		stack.emplace_back(cx - 1, cy);
		stack.emplace_back(cx, cy + 1);
		stack.emplace_back(cx, cy - 1);
	}
}

void Canvas::GetImage(int left, int top, int right, int bottom, Bitmap& outBitmap) const {
	if (left > right) swap(left, right);
	if (top > bottom) swap(top, bottom);
	outBitmap.width = right - left + 1;
	outBitmap.height = bottom - top + 1;
	outBitmap.data.assign(static_cast<size_t>(outBitmap.width) * outBitmap.height * 4, 0);

	for (int y = 0; y < outBitmap.height; y++) {
		for (int x = 0; x < outBitmap.width; x++) {
			size_t dstIdx = (static_cast<size_t>(y) * outBitmap.width + x) * 4;
			int srcX = left + x, srcY = top + y;
			if (!InBounds(srcX, srcY)) continue;
			size_t srcIdx = (static_cast<size_t>(srcY) * width + srcX) * 4;
			memcpy(&outBitmap.data[dstIdx], &buffer[srcIdx], 4);
		}
	}
}

void Canvas::PutImage(int left, int top, const Bitmap& bitmap) {
	for (int y = 0; y < bitmap.height; y++) {
		for (int x = 0; x < bitmap.width; x++) {
			int dstX = left + x, dstY = top + y;
			if (!InBounds(dstX, dstY)) continue;
			size_t srcIdx = (static_cast<size_t>(y) * bitmap.width + x) * 4;
			size_t dstIdx = (static_cast<size_t>(dstY) * width + dstX) * 4;
			memcpy(&buffer[dstIdx], &bitmap.data[srcIdx], 4);
		}
	}
}

void Canvas::PutRawImage(const uint8_t* srcBGRA, int srcWidth, int srcHeight, int x, int y, int dstWidth, int dstHeight) {
	if (!srcBGRA || srcWidth <= 0 || srcHeight <= 0 || dstWidth <= 0 || dstHeight <= 0) return;

	for (int dy = 0; dy < dstHeight; dy++) {
		for (int dx = 0; dx < dstWidth; dx++) {
			int srcX = dx * srcWidth / dstWidth;
			int srcY = dy * srcHeight / dstHeight;
			size_t srcIdx = (static_cast<size_t>(srcY) * srcWidth + srcX) * 4;
			BlendPixel(x + dx, y + dy, srcBGRA[srcIdx + 2], srcBGRA[srcIdx + 1], srcBGRA[srcIdx + 0], brushAlpha);
		}
	}
}

void Canvas::SetFontSize(int size) {
	fontSize = max(1, size);
}

void Canvas::PutChar(char ch, int x, int y) {
	PutString(string(1, ch), x, y);
}

int Canvas::PutString(const string& text, int x, int y) {
	if (text.empty() || width <= 0 || height <= 0) return 0;

	int lineHeight = MeasureLineHeight(fontSize);
	int curY = y;
	size_t start = 0;
	for (size_t i = 0; i <= text.size(); i++) {
		if (i != text.size() && text[i] != '\n') continue;

		string line = text.substr(start, i - start);
		if (!line.empty()) {
			wstring wline = Utf8ToWide(line);
			DrawLine(buffer, width, height, wline, x, curY, fontSize, brushR, brushG, brushB, brushAlpha);
		}
		curY += lineHeight;
		start = i + 1;
	}
	return curY - y;
}

int Canvas::StringWidth(const string& text) const {
	int maxWidth = 0;
	size_t start = 0;
	for (size_t i = 0; i <= text.size(); i++) {
		if (i != text.size() && text[i] != '\n') continue;

		wstring wline = Utf8ToWide(text.substr(start, i - start));
		maxWidth = max(maxWidth, MeasureLineWidth(wline, fontSize));
		start = i + 1;
	}
	return maxWidth;
}

int Canvas::GetFontHeight() const {
	return MeasureLineHeight(fontSize);
}

void Canvas::PushKey(int code) {
	keyQueue.Push(code);
}

bool Canvas::HasPendingKey() const {
	return !keyQueue.Empty();
}

int Canvas::PopKey() {
	return keyQueue.Pop();
}

void Canvas::ClearKeyBuffer() {
	keyQueue.Clear();
}

void Canvas::SetMousePos(int x, int y) {
	mouseState.x = x;
	mouseState.y = y;
}

void Canvas::PushMouseButton(int button, bool down) {
	if (button < 0 || button > 2) return;
	mouseState.buttons[button] = down;
	mouseQueue.Push({ mouseState.x, mouseState.y, button, down });
}

MouseState Canvas::GetMouseState() const {
	return mouseState;
}

bool Canvas::HasPendingMouseEvent() const {
	return !mouseQueue.Empty();
}

MouseEvent Canvas::PopMouseEvent() {
	return mouseQueue.Pop();
}

void Canvas::ClearMouseBuffer() {
	mouseQueue.Clear();
}
