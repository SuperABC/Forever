#include "canvas.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>


using namespace std;

namespace {
	// 内置5x7点阵ASCII字体——只覆盖数字/大写字母/几个常用标点，用ASCII艺术("."=不画,"#"=画)
	// 直接描字形，避免手工换算十六进制位掩码带来的转录错误。小写字母在PutChar里转大写渲染，
	// 表里没有的字符留空(相当于空格)，不是漏做——这次的目标只是"画布上能显示可读的分数/
	// 提示文字"，不追求覆盖完整ASCII表。
	struct GlyphArt {
		char ch;
		const char* rows[7];
	};

	constexpr int kGlyphWidth = 5;
	constexpr int kGlyphHeight = 7;

	const GlyphArt kGlyphs[] = {
		{' ', {"     ","     ","     ","     ","     ","     ","     "}},
		{'0', {".###.","#...#","#..##","#.#.#","##..#","#...#",".###."}},
		{'1', {"..#..",".##..","..#..","..#..","..#..","..#..",".###."}},
		{'2', {".###.","#...#","....#","...#.","..#..",".#...","#####"}},
		{'3', {".###.","#...#","....#","..##.","....#","#...#",".###."}},
		{'4', {"...#.","..##.",".#.#.","#..#.","#####","...#.","...#."}},
		{'5', {"#####","#....","####.","....#","....#","#...#",".###."}},
		{'6', {"..##.",".#...","#....","####.","#...#","#...#",".###."}},
		{'7', {"#####","....#","...#.","..#..",".#...",".#...",".#..."}},
		{'8', {".###.","#...#","#...#",".###.","#...#","#...#",".###."}},
		{'9', {".###.","#...#","#...#",".####","....#","...#.","..##."}},
		{'A', {"..#..",".#.#.","#...#","#...#","#####","#...#","#...#"}},
		{'B', {"####.","#...#","#...#","####.","#...#","#...#","####."}},
		{'C', {".###.","#...#","#....","#....","#....","#...#",".###."}},
		{'D', {"####.","#...#","#...#","#...#","#...#","#...#","####."}},
		{'E', {"#####","#....","#....","####.","#....","#....","#####"}},
		{'F', {"#####","#....","#....","####.","#....","#....","#...."}},
		{'G', {".###.","#...#","#....","#.###","#...#","#...#",".###."}},
		{'H', {"#...#","#...#","#...#","#####","#...#","#...#","#...#"}},
		{'I', {".###.","..#..","..#..","..#..","..#..","..#..",".###."}},
		{'J', {"....#","....#","....#","....#","....#","#...#",".###."}},
		{'K', {"#...#","#..#.","#.#..","##...","#.#..","#..#.","#...#"}},
		{'L', {"#....","#....","#....","#....","#....","#....","#####"}},
		{'M', {"#...#","##.##","#.#.#","#...#","#...#","#...#","#...#"}},
		{'N', {"#...#","##..#","#.#.#","#..##","#...#","#...#","#...#"}},
		{'O', {".###.","#...#","#...#","#...#","#...#","#...#",".###."}},
		{'P', {"####.","#...#","#...#","####.","#....","#....","#...."}},
		{'Q', {".###.","#...#","#...#","#...#","#.#.#","#..#.",".##.#"}},
		{'R', {"####.","#...#","#...#","####.","#.#..","#..#.","#...#"}},
		{'S', {".###.","#...#","#....",".###.","....#","#...#",".###."}},
		{'T', {"#####","..#..","..#..","..#..","..#..","..#..","..#.."}},
		{'U', {"#...#","#...#","#...#","#...#","#...#","#...#",".###."}},
		{'V', {"#...#","#...#","#...#","#...#","#...#",".#.#.","..#.."}},
		{'W', {"#...#","#...#","#...#","#.#.#","#.#.#","##.##","#...#"}},
		{'X', {"#...#","#...#",".#.#.","..#..",".#.#.","#...#","#...#"}},
		{'Y', {"#...#","#...#",".#.#.","..#..","..#..","..#..","..#.."}},
		{'Z', {"#####","....#","...#.","..#..",".#...","#....","#####"}},
		{':', {".....","..#..",".....",".....",".....","..#..","....."}},
		{'-', {".....",".....",".....","#####",".....",".....","....."}},
		{'!', {"..#..","..#..","..#..","..#..","..#..",".....","..#.."}},
		{'.', {".....",".....",".....",".....",".....",".....","..#.."}},
		{'?', {".###.","#...#","....#","...#.","..#..",".....","..#.."}},
	};

	const GlyphArt* FindGlyph(char ch) {
		for (const GlyphArt& glyph : kGlyphs) {
			if (glyph.ch == ch) return &glyph;
		}
		return nullptr;
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
	char upper = static_cast<char>(toupper(static_cast<unsigned char>(ch)));
	const GlyphArt* glyph = FindGlyph(upper);
	if (!glyph) return;

	int scale = max(1, fontSize / kGlyphHeight);
	for (int row = 0; row < kGlyphHeight; row++) {
		for (int col = 0; col < kGlyphWidth; col++) {
			if (glyph->rows[row][col] != '#') continue;
			for (int sy = 0; sy < scale; sy++) {
				for (int sx = 0; sx < scale; sx++) {
					PutPixel(x + col * scale + sx, y + row * scale + sy);
				}
			}
		}
	}
}

int Canvas::PutString(const string& text, int x, int y) {
	int scale = max(1, fontSize / kGlyphHeight);
	int advanceX = (kGlyphWidth + 1) * scale;
	int advanceY = (kGlyphHeight + 1) * scale;

	int curX = x, curY = y;
	for (char ch : text) {
		if (ch == '\n') {
			curX = x;
			curY += advanceY;
			continue;
		}
		PutChar(ch, curX, curY);
		curX += advanceX;
	}
	return curY + advanceY - y;
}

int Canvas::StringWidth(const string& text) const {
	int scale = max(1, fontSize / kGlyphHeight);
	int advanceX = (kGlyphWidth + 1) * scale;

	int maxLineLength = 0, currentLineLength = 0;
	for (char ch : text) {
		if (ch == '\n') {
			maxLineLength = max(maxLineLength, currentLineLength);
			currentLineLength = 0;
			continue;
		}
		currentLineLength++;
	}
	maxLineLength = max(maxLineLength, currentLineLength);
	return maxLineLength * advanceX;
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
