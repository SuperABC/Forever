#pragma once

#include "player/puzzle_mod.h"
#include "common/utility.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>


// Basic里这批小游戏共用的小工具，全部inline，不依赖Core。详见puzzle_games.md。

// 每个puzzle类声明里用一次：GetId/GetType/GetName(全局唯一，static计数器+id)/存档计数器钩子。
// 类里必须有`std::string name;`以外的成员自己声明；count要在.cpp里`int Class::count = 0;`。
#define PUZZLE_IDENTITY(Class, IdString) \
public: \
	static const char* GetId() { return IdString; } \
	virtual const char* GetType() const override { return IdString; } \
	virtual const char* GetName() override { name = std::string(#Class) + std::to_string(id); return name.data(); } \
	virtual int GetInstanceCounter() const override { return count; } \
	virtual void SetInstanceCounter(int value) override { count = value; } \
private: \
	static int count; \
	int id = count++; \
	std::string name;

// SetParam传进来的"key=value key=value"。空格分隔，value里不能有空格。
class PuzzleParams {
public:
	void Parse(const std::string& text) {
		values.clear();
		std::istringstream stream(text);
		std::string token;
		while (stream >> token) {
			size_t eq = token.find('=');
			if (eq == std::string::npos || eq == 0) continue;
			values[token.substr(0, eq)] = token.substr(eq + 1);
		}
	}
	bool Has(const std::string& key) const { return values.count(key) > 0; }
	std::string GetString(const std::string& key, const std::string& fallback) const {
		auto it = values.find(key);
		return it == values.end() ? fallback : it->second;
	}
	int GetInt(const std::string& key, int fallback) const {
		auto it = values.find(key);
		if (it == values.end()) return fallback;
		char* end = nullptr;
		long v = std::strtol(it->second.c_str(), &end, 10);
		return end == it->second.c_str() ? fallback : static_cast<int>(v);
	}
	double GetDouble(const std::string& key, double fallback) const {
		auto it = values.find(key);
		if (it == values.end()) return fallback;
		char* end = nullptr;
		double v = std::strtod(it->second.c_str(), &end);
		return end == it->second.c_str() ? fallback : v;
	}

private:
	std::map<std::string, std::string> values;
};

// 每个游戏自己一个随机数发生器；seed=N参数固定种子，方便复现测试。
class PuzzleRng {
public:
	void Seed(const PuzzleParams& params) {
		generator.seed(static_cast<unsigned>(params.Has("seed") ? params.GetInt("seed", 1) : GetRandom(1000000) + 1));
	}
	int Int(int n) { return n <= 1 ? 0 : static_cast<int>(generator() % static_cast<unsigned>(n)); }
	double Real(double a, double b) { return a + (b - a) * (generator() / 4294967296.0); }
	bool Chance(double p) { return Real(0, 1) < p; }
	template <class T> void Shuffle(std::vector<T>& v) { std::shuffle(v.begin(), v.end(), generator); }

private:
	std::mt19937 generator;
};

struct PuzzleRect {
	int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	bool Hit(int x, int y) const { return x >= x1 && x <= x2 && y >= y1 && y <= y2; }
	int Width() const { return x2 - x1; }
	int Height() const { return y2 - y1; }
	int CenterX() const { return (x1 + x2) / 2; }
	int CenterY() const { return (y1 + y2) / 2; }
};

inline PuzzleRect MakeRect(int x, int y, int w, int h) { return PuzzleRect{ x, y, x + w, y + h }; }

// 一帧里的输入：按下的键(已去掉抬起事件)+按序的鼠标事件+当前鼠标状态，读完就清掉画布缓冲。
struct PuzzleInput {
	std::vector<int> keys;       // 本帧按下的键(原始码，不含KEY_RELEASED_FLAG)
	std::vector<int> keyUps;     // 本帧抬起的键
	std::vector<MouseEvent> events;
	MouseState mouse;
	bool anyPress = false;       // 本帧有任意按键按下或鼠标/触摸按下

	void Poll(Canvas* canvas) {
		keys.clear(); keyUps.clear(); events.clear(); anyPress = false;
		while (canvas->HasPendingKey()) {
			int raw = canvas->PopKey();
			if (raw & KEY_RELEASED_FLAG) keyUps.push_back(raw & ~KEY_RELEASED_FLAG);
			else { keys.push_back(raw); anyPress = true; }
		}
		while (canvas->HasPendingMouseEvent()) {
			MouseEvent e = canvas->PopMouseEvent();
			if (e.button != MOUSE_LEFT) continue;
			events.push_back(e);
			if (e.down) anyPress = true;
		}
		mouse = canvas->GetMouseState();
	}
	bool Key(int code) const { return std::find(keys.begin(), keys.end(), code) != keys.end(); }
};

// ---- 绘图小工具(都会把画笔恢复成不透明) ----
inline void FillRect(Canvas* canvas, const PuzzleRect& r, int red, int green, int blue, float alpha = 1.f) {
	canvas->SetColor(red, green, blue);
	canvas->SetAlpha(alpha);
	canvas->PutRect(r.x1, r.y1, r.x2, r.y2, true);
	canvas->SetAlpha(1.f);
}

inline void StrokeRect(Canvas* canvas, const PuzzleRect& r, int red, int green, int blue, int thickness = 1, float alpha = 1.f) {
	canvas->SetColor(red, green, blue);
	canvas->SetAlpha(alpha);
	for (int i = 0; i < thickness; i++) canvas->PutRect(r.x1 + i, r.y1 + i, r.x2 - i, r.y2 - i, false);
	canvas->SetAlpha(1.f);
}

inline int TextWidth(Canvas* canvas, const std::string& text, int fontSize) {
	canvas->SetFontSize(fontSize);
	return canvas->StringWidth(text);
}

// 以(cx,cy)为中心画一行(或多行)文字。
inline void DrawTextCentered(Canvas* canvas, const std::string& text, int cx, int cy, int fontSize, int red, int green, int blue) {
	canvas->SetFontSize(fontSize);
	canvas->SetColor(red, green, blue);
	canvas->SetAlpha(1.f);
	int w = canvas->StringWidth(text);
	int h = canvas->GetFontHeight() * (1 + static_cast<int>(std::count(text.begin(), text.end(), '\n')));
	canvas->PutString(text, cx - w / 2, cy - h / 2);
}

inline void DrawText(Canvas* canvas, const std::string& text, int x, int y, int fontSize, int red, int green, int blue) {
	canvas->SetFontSize(fontSize);
	canvas->SetColor(red, green, blue);
	canvas->SetAlpha(1.f);
	canvas->PutString(text, x, y);
}

inline void DrawButton(Canvas* canvas, const PuzzleRect& r, const std::string& label, int fontSize,
	int red = 60, int green = 70, int blue = 100, bool enabled = true) {
	FillRect(canvas, r, enabled ? red : red / 2, enabled ? green : green / 2, enabled ? blue : blue / 2);
	StrokeRect(canvas, r, 230, 230, 240, 2);
	DrawTextCentered(canvas, label, r.CenterX(), r.CenterY(), fontSize, enabled ? 255 : 150, enabled ? 255 : 150, enabled ? 255 : 150);
}

// 白色铺满，再把图片按透明度铺满area(没有图片就只留白)。
inline void DrawBackdrop(Canvas* canvas, const PuzzleRect& area, const Bitmap* image, float alpha) {
	FillRect(canvas, area, 255, 255, 255);
	if (image && image->width > 0) {
		canvas->SetAlpha(alpha);
		canvas->PutBitmap(*image, area.x1, area.y1, area.Width() + 1, area.Height() + 1);
		canvas->SetAlpha(1.f);
	}
}

inline void FillCircle(Canvas* canvas, int x, int y, int radius, int red, int green, int blue, float alpha = 1.f) {
	canvas->SetColor(red, green, blue);
	canvas->SetAlpha(alpha);
	canvas->PutCircle(x, y, radius, true);
	canvas->SetAlpha(1.f);
}

// 读mod随包文本文件(谱面、文章等)：post "resource path"按文件名找到路径再读。文件名要以puzzle id加下划线开头。
inline bool LoadModTextFile(PostHandle* post, const std::string& fileName, std::string& outText) {
	if (!post) return false;
	JsonValue request(DATA_OBJECT);
	request["post"] = "resource path";
	request["name"] = fileName;
	post->Post(request);
	const JsonValue& found = post->GetResult();
	if (found["result"].AsString() != "success") return false;
	std::ifstream file(found["path"].AsString(), std::ios::binary);
	if (!file) return false;
	std::ostringstream buffer;
	buffer << file.rdbuf();
	outText = buffer.str();
	return true;
}

inline std::string FormatSeconds(double seconds) {
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%.1f", seconds);
	return buffer;
}

// 结算页公共部分：半透明遮罩+标题+若干行文字+提示。lines用'\n'分隔。
inline void DrawResultPanel(Canvas* canvas, const std::string& title, const std::string& lines, const std::string& hint) {
	int w = canvas->GetWidth(), h = canvas->GetHeight();
	PuzzleRect panel = MakeRect(w / 8, h / 5, w * 3 / 4, h * 3 / 5);
	FillRect(canvas, panel, 0, 0, 0, 0.78f);
	StrokeRect(canvas, panel, 255, 255, 255, 2);
	int unit = std::max(14, std::min(w, h) / 18);
	DrawTextCentered(canvas, title, panel.CenterX(), panel.y1 + unit * 2, unit * 3 / 2, 255, 220, 80);
	DrawTextCentered(canvas, lines, panel.CenterX(), panel.CenterY(), unit, 255, 255, 255);
	DrawTextCentered(canvas, hint, panel.CenterX(), panel.y2 - unit * 3 / 2, unit * 3 / 4, 200, 200, 200);
}
