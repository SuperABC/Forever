#include "app_music.h"

#include "player/canvas.h"
#include "common/json.h"

#include <algorithm>
#include <cmath>
#include <cstdio>


using namespace std;

namespace {
	const char* kTrack = "phone_music";   // 这个app控制的音轨名
	const char* kFolder = "Music";        // resource_paths各根目录下的子文件夹

	struct Rgb { int r, g, b; };
	const Rgb kBackground{ 16, 16, 22 };
	const Rgb kAccent{ 252, 61, 96 };
	const Rgb kText{ 245, 245, 248 };
	const Rgb kSubText{ 150, 150, 162 };
	const Rgb kError{ 255, 110, 110 };
	const Rgb kCoverPalette[] = {
		{ 236, 72, 110 }, { 255, 140, 66 }, { 66, 165, 245 }, { 102, 187, 106 },
		{ 171, 71, 188 }, { 38, 198, 218 }, { 255, 202, 40 }, { 120, 134, 255 },
	};

	void SetRgb(Canvas* canvas, const Rgb& c) { canvas->SetColor(c.r, c.g, c.b); }
	Rgb Scale(const Rgb& c, float k) {
		auto f = [k](int v) { return max(0, min(255, static_cast<int>(v * k))); };
		return { f(c.r), f(c.g), f(c.b) };
	}
	Rgb Mix(const Rgb& a, const Rgb& b, float t) {
		return { static_cast<int>(a.r + (b.r - a.r) * t), static_cast<int>(a.g + (b.g - a.g) * t), static_cast<int>(a.b + (b.b - a.b) * t) };
	}
	uint32_t Hash(const string& text) {
		uint32_t h = 2166136261u;
		for (unsigned char c : text) { h ^= c; h *= 16777619u; }
		return h;
	}
	Rgb CoverColor(const string& seed) {
		return kCoverPalette[Hash(seed) % (sizeof(kCoverPalette) / sizeof(kCoverPalette[0]))];
	}
	string Fit(Canvas* canvas, const string& text, int maxWidth) {
		return MusicApp::Ellipsize(text, maxWidth, [canvas](const string& s) { return canvas->StringWidth(s); });
	}
	void PutCentered(Canvas* canvas, const string& text, int cx, int y) {
		canvas->PutString(text, cx - canvas->StringWidth(text) / 2, y);
	}
	const char* ModeName(MusicApp::Mode mode) {
		switch (mode) {
		case MusicApp::Mode::ListLoop: return "列表循环";
		case MusicApp::Mode::SingleLoop: return "单曲循环";
		case MusicApp::Mode::Shuffle: return "随机播放";
		}
		return "";
	}
}

int MusicApp::count = 0;

MusicApp::MusicApp() : id(count++) {
}

const char* MusicApp::GetName() {
	name = "Music" + to_string(id);
	return name.data();
}

// ---- 纯逻辑 ----

string MusicApp::DisplayName(const string& fileName) {
	const size_t dot = fileName.find_last_of('.');
	if (dot == string::npos || dot == 0) return fileName;
	return fileName.substr(0, dot);
}

int MusicApp::NextIndex(Mode mode, int current, int count, bool userSkip, uint32_t random) {
	if (count <= 0) return -1;
	if (current < 0 || current >= count) return mode == Mode::Shuffle ? static_cast<int>(random % count) : 0;
	switch (mode) {
	case Mode::ListLoop: return (current + 1) % count;
	case Mode::SingleLoop: return userSkip ? (current + 1) % count : current;
	case Mode::Shuffle: {
		if (count == 1) return 0;
		const int pick = static_cast<int>(random % static_cast<uint32_t>(count - 1)); // 在"除了当前这首"的count-1首里挑
		return pick >= current ? pick + 1 : pick;
	}
	}
	return 0;
}

string MusicApp::FormatTime(double seconds) {
	const int total = max(0, static_cast<int>(seconds));
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%d:%02d", total / 60, total % 60);
	return buffer;
}

uint32_t MusicApp::NextRandom() {
	// xorshift32：自己的随机数，不动全局随机流(不影响经济模拟的可复现性)。
	randomState ^= randomState << 13;
	randomState ^= randomState >> 17;
	randomState ^= randomState << 5;
	return randomState;
}

bool MusicApp::UnitTest(TestPrintFn print) {
	if (!CheckModIdentity("app", GetType(), GetName(), print)) return false;
	bool ok = true;
	auto check = [&](bool condition, const char* what) {
		if (!condition) { print(what); ok = false; }
	};
	check(DisplayName("\xE5\x86\xB0\xE5\xBF\x83\xE6\x9C\x88\xE5\x85\x89.mp3") == "\xE5\x86\xB0\xE5\xBF\x83\xE6\x9C\x88\xE5\x85\x89", "app_music: DisplayName should strip .mp3");
	check(DisplayName("a.b.ogg") == "a.b", "app_music: DisplayName should strip only the last extension");
	check(DisplayName("noext") == "noext", "app_music: DisplayName without extension");
	check(NextIndex(Mode::ListLoop, 2, 3, false, 0) == 0, "app_music: list loop wraps to the first song");
	check(NextIndex(Mode::ListLoop, -1, 3, false, 7) == 0, "app_music: list loop starts from the first song");
	check(NextIndex(Mode::SingleLoop, 1, 3, false, 0) == 1, "app_music: single loop repeats when a song ends");
	check(NextIndex(Mode::SingleLoop, 1, 3, true, 0) == 2, "app_music: single loop still skips on user next");
	check(NextIndex(Mode::Shuffle, 0, 1, false, 5) == 0, "app_music: shuffle with one song");
	check(NextIndex(Mode::ListLoop, 0, 0, false, 0) == -1, "app_music: empty library has no next");
	bool shuffleRepeats = false, shuffleInRange = true;
	for (uint32_t r = 0; r < 100; r++) {
		const int next = NextIndex(Mode::Shuffle, 2, 5, false, r);
		if (next == 2) shuffleRepeats = true;
		if (next < 0 || next >= 5) shuffleInRange = false;
	}
	check(!shuffleRepeats, "app_music: shuffle must not repeat the current song");
	check(shuffleInRange, "app_music: shuffle index out of range");
	check(FormatTime(0) == "0:00" && FormatTime(65.4) == "1:05" && FormatTime(198.6) == "3:18", "app_music: FormatTime");
	auto measure = [](const string& s) { // 每个UTF-8字符算10像素
		int n = 0;
		for (unsigned char c : s) if ((c & 0xC0) != 0x80) n++;
		return n * 10;
	};
	check(Ellipsize(string("abcdef"), 40, measure) == "abc\xE2\x80\xA6", "app_music: Ellipsize ascii");
	check(Ellipsize(string("abc"), 40, measure) == "abc", "app_music: Ellipsize keeps short text");
	check(Ellipsize(string("\xE5\x86\xB0\xE5\xBF\x83\xE6\x9C\x88\xE5\x85\x89"), 30, measure) == "\xE5\x86\xB0\xE5\xBF\x83\xE2\x80\xA6", "app_music: Ellipsize must cut whole UTF-8 characters");
	return ok;
}

// ---- 生命周期 ----

void MusicApp::Init(Canvas* canvas, PostHandle* post) {
	Layout(canvas);
	ScanLibrary(post);
	PollStatus(post); // 自己的音轨已经在放(读档后、或者关掉app又重新打开)就接上
	screen = Screen::Library;
	selection = current >= 0 ? current : 0;
	scroll = 0;
	EnsureVisible(selection);
}

void MusicApp::Refresh(Canvas* canvas, PostHandle* post) {
	// 切回前台：页面保持原样，重新扫一遍文件夹(期间新放进去的歌也能看到)。
	Layout(canvas);
	ScanLibrary(post);
	PollStatus(post);
	selection = min(selection, max(0, static_cast<int>(songs.size()) - 1));
	ClampScroll();
}

void MusicApp::Exit(PostHandle* post) {
	// 音轨在Core里，app实例销毁了也会接着放，所以退出时删掉(没有音轨时stop返回fail，无所谓)。
	PostMusic(post, "stop", "", 0.0);
	current = -1;
	state.clear();
}

void MusicApp::Back(Canvas* canvas, PostHandle* post) {
	if (screen == Screen::NowPlaying) screen = Screen::Library;
}

void MusicApp::Loop(Canvas* canvas, int ms, PostHandle* post) {
	Layout(canvas);
	animTime += ms / 1000.0;
	PollStatus(post); // 也负责放完自动切歌，后台(不画)时一样跑
	HandleInput(canvas, post);
	if (canvas->IsDrawEnabled()) Render(canvas);
}

// ---- 数据 ----

void MusicApp::ScanLibrary(PostHandle* post) {
	songs.clear();
	libraryError = false;
	if (!post) { libraryError = true; return; }
	JsonValue request(DATA_OBJECT);
	request["post"] = "list resource files";
	request["folder"] = kFolder;
	JsonValue extensions(DATA_ARRAY);
	extensions.append(JsonValue("mp3"));
	extensions.append(JsonValue("wav"));
	extensions.append(JsonValue("ogg"));
	request["extensions"] = extensions;
	post->Post(request);
	const JsonValue& result = post->GetResult();
	if (result["result"].AsString() != "success" || !result["files"].IsArray()) { libraryError = true; return; }
	for (unsigned int i = 0; i < result["files"].size(); i++) {
		Song song;
		song.file = result["files"][i].AsString();
		song.title = DisplayName(song.file);
		if (!song.file.empty()) songs.push_back(song);
	}
}

void MusicApp::PollStatus(PostHandle* post) {
	if (!post) return;
	JsonValue request(DATA_OBJECT);
	request["post"] = "music status";
	request["track"] = kTrack;
	post->Post(request);
	const JsonValue& result = post->GetResult();
	if (result["result"].AsString() != "success") { // 没有音轨(还没放过，或者被脚本stop了)
		current = -1;
		state.clear();
		position = duration = 0.0;
		return;
	}
	state = result["state"].AsString();
	position = result["position"].AsDouble();
	duration = result["duration"].AsDouble();
	const string asset = result["asset"].AsString();
	current = -1;
	for (size_t i = 0; i < songs.size(); i++) {
		if (songs[i].file == asset) { current = static_cast<int>(i); break; }
	}
	if (state == "ended" && current >= 0) Next(post, false);
}

bool MusicApp::PostMusic(PostHandle* post, const char* op, const string& asset, double seconds) {
	if (!post) return false;
	JsonValue request(DATA_OBJECT);
	request["post"] = "music";
	request["track"] = kTrack;
	request["op"] = op;
	if (!asset.empty()) request["asset"] = asset;
	request["loop"] = false; // 单曲循环由app放完重播，不用Core的loop
	request["position"] = seconds;
	post->Post(request);
	return post->GetResult()["result"].AsString() == "success";
}

void MusicApp::PlayIndex(PostHandle* post, int index, bool remember) {
	if (index < 0 || index >= static_cast<int>(songs.size())) return;
	if (remember && mode == Mode::Shuffle && current >= 0 && current != index) {
		history.push_back(current);
		if (history.size() > 64) history.erase(history.begin());
	}
	if (!PostMusic(post, "play", songs[index].file, 0.0)) {
		lastError = "无法播放";
		return;
	}
	lastError.clear();
	current = index;
	state = "loading";
	position = duration = 0.0;
	selection = index;
	EnsureVisible(index);
}

void MusicApp::TogglePause(PostHandle* post) {
	if (current < 0) {
		if (!songs.empty()) PlayIndex(post, mode == Mode::Shuffle ? NextIndex(mode, -1, static_cast<int>(songs.size()), true, NextRandom()) : selection);
		return;
	}
	if (state == "ended" || state == "error") { PlayIndex(post, current, false); return; }
	if (state == "paused") { if (PostMusic(post, "resume", "", 0.0)) state = "playing"; }
	else if (PostMusic(post, "pause", "", 0.0)) state = "paused";
}

void MusicApp::SeekTo(PostHandle* post, double seconds) {
	if (current < 0) return;
	seconds = max(0.0, duration > 0.0 ? min(seconds, duration) : seconds);
	if (PostMusic(post, "seek", "", seconds)) position = seconds;
}

void MusicApp::Next(PostHandle* post, bool userSkip) {
	PlayIndex(post, NextIndex(mode, current, static_cast<int>(songs.size()), userSkip, NextRandom()));
}

void MusicApp::Previous(PostHandle* post) {
	const int count = static_cast<int>(songs.size());
	if (count <= 0) return;
	if (current >= 0 && position > 3.0) { SeekTo(post, 0.0); return; } // 放了一会儿：回到开头，和常见播放器一样
	if (mode == Mode::Shuffle && !history.empty()) {
		const int back = history.back();
		history.pop_back();
		PlayIndex(post, back, false);
		return;
	}
	PlayIndex(post, current < 0 ? 0 : (current - 1 + count) % count, false);
}

// ---- 布局 ----

void MusicApp::Layout(Canvas* canvas) {
	width = canvas->GetWidth();
	height = canvas->GetHeight();
	const int barHeight = max(20, height * 625 / 10000); // 和Phone::RecomputeLayout一致：底部导航栏Phone自己画、点击也不给app
	usableBottom = height - barHeight;

	titleFont = max(26, height / 17);
	contentFont = max(21, height / 23);
	smallFont = max(17, height / 32);

	headerTop = height / 60;
	playAllTop = headerTop + titleFont * 14 / 10 + height / 100;
	playAllHeight = max(32, height / 14);
	listTop = playAllTop + playAllHeight + height / 120;
	rowHeight = max(40, height / 11);

	miniHeight = max(40, height / 10);
	miniLine = max(2, height / 300);
	miniIcon = max(12, miniHeight * 2 / 5);
	miniNextX = width - miniHeight * 7 / 10;
	miniPlayX = width - miniHeight * 16 / 10;
	miniCenterY = MiniBarTop() + miniLine + (miniHeight - miniLine) / 2;

	coverSize = max(40, min(width * 68 / 100, usableBottom * 40 / 100));
	coverTop = height * 9 / 100;
	npTitleTop = coverTop + coverSize + height / 30;
	progressY = npTitleTop + titleFont * 13 / 10 + smallFont * 2 + height / 30;
	progressLeft = width / 10;
	progressRight = width - width / 10;
	controlsY = progressY + smallFont + height / 11;
	ClampScroll();
}

int MusicApp::MaxScroll() const {
	const int view = ListBottom() - ListTop();
	return max(0, static_cast<int>(songs.size()) * rowHeight - view);
}

void MusicApp::ClampScroll() {
	scroll = max(0, min(scroll, MaxScroll()));
}

void MusicApp::EnsureVisible(int index) {
	if (index < 0 || rowHeight <= 0) return;
	const int top = index * rowHeight;
	const int view = ListBottom() - ListTop();
	if (top < scroll) scroll = top;
	else if (top + rowHeight > scroll + view) scroll = top + rowHeight - view;
	ClampScroll();
}

int MusicApp::HitRow(int x, int y) const {
	if (y < ListTop() || y >= ListBottom() || rowHeight <= 0) return -1;
	const int row = (y - ListTop() + scroll) / rowHeight;
	if (row < 0 || row >= static_cast<int>(songs.size())) return -1;
	return row;
}

double MusicApp::DisplayedPosition() const {
	if (seeking) return seekRatio * duration;
	return position;
}

// ---- 输入 ----

void MusicApp::HandleInput(Canvas* canvas, PostHandle* post) {
	while (canvas->HasPendingKey()) {
		const int raw = canvas->PopKey();
		if (raw & KEY_RELEASED_FLAG) continue;
		const int key = raw & ~KEY_RELEASED_FLAG;
		const int count = static_cast<int>(songs.size());
		if (screen == Screen::Library) {
			if (key == KEY_UP && selection > 0) { selection--; EnsureVisible(selection); }
			else if (key == KEY_DOWN && selection + 1 < count) { selection++; EnsureVisible(selection); }
			else if (key == KEY_ENTER && selection < count) {
				if (selection == current && state != "ended" && state != "error") screen = Screen::NowPlaying;
				else PlayIndex(post, selection);
			}
			else if (key == KEY_RIGHT && HasMiniBar()) screen = Screen::NowPlaying;
		}
		else {
			if (key == KEY_LEFT) SeekTo(post, position - 5.0);
			else if (key == KEY_RIGHT) SeekTo(post, position + 5.0);
			else if (key == KEY_ENTER) TogglePause(post);
			else if (key == KEY_UP) Previous(post);
			else if (key == KEY_DOWN) Next(post, true);
		}
	}

	while (canvas->HasPendingMouseEvent()) {
		const MouseEvent event = canvas->PopMouseEvent();
		if (event.button != 0) continue;
		if (event.down) OnPress(event.x, event.y, post);
		else OnRelease(event.x, event.y, post);
	}
	if (pressed) {
		const MouseState mouse = canvas->GetMouseState();
		OnDrag(mouse.x, mouse.y);
	}

	const int wheel = canvas->ConsumeMouseWheel();
	if (wheel != 0 && screen == Screen::Library) {
		scroll -= wheel * rowHeight; // 向上滚为正：列表往回
		ClampScroll();
	}
}

void MusicApp::OnPress(int x, int y, PostHandle* post) {
	pressed = true;
	dragging = false;
	seeking = false;
	pressX = x;
	pressY = y;
	pressScroll = scroll;
	pressInList = screen == Screen::Library && y >= ListTop() && y < ListBottom();
	if (screen == Screen::NowPlaying && current >= 0 && duration > 0.0) {
		const int slack = max(smallFont, height / 30);
		if (abs(y - progressY) <= slack && x >= progressLeft - slack && x <= progressRight + slack) {
			seeking = true;
			OnDrag(x, y);
		}
	}
}

void MusicApp::OnDrag(int x, int y) {
	if (seeking) {
		const int span = max(1, progressRight - progressLeft);
		seekRatio = max(0.0, min(1.0, static_cast<double>(x - progressLeft) / span));
		return;
	}
	if (!pressInList) return;
	if (!dragging && abs(y - pressY) > max(6, rowHeight / 6)) dragging = true;
	if (dragging) {
		scroll = pressScroll - (y - pressY);
		ClampScroll();
	}
}

void MusicApp::OnRelease(int x, int y, PostHandle* post) {
	if (!pressed) return;
	pressed = false;
	if (seeking) {
		seeking = false;
		SeekTo(post, seekRatio * duration);
		return;
	}
	if (dragging) { dragging = false; return; }

	// 点击：按按下的位置判定(按下和松开之间没怎么动)。
	x = pressX;
	y = pressY;
	const int hitRadius = max(miniIcon, miniHeight / 2);
	if (screen == Screen::Library) {
		if (HasMiniBar() && y >= MiniBarTop() && y < usableBottom) {
			if (abs(x - miniPlayX) <= hitRadius * 6 / 10) TogglePause(post);
			else if (abs(x - miniNextX) <= hitRadius * 6 / 10) Next(post, true);
			else screen = Screen::NowPlaying;
			return;
		}
		if (y >= playAllTop && y < playAllTop + playAllHeight && !songs.empty()) {
			history.clear();
			PlayIndex(post, mode == Mode::Shuffle ? NextIndex(mode, -1, static_cast<int>(songs.size()), true, NextRandom()) : 0, false);
			return;
		}
		const int row = HitRow(x, y);
		if (row < 0) return;
		selection = row;
		if (row == current && state != "ended" && state != "error") screen = Screen::NowPlaying; // 点正在放的那首：打开播放页
		else PlayIndex(post, row);
		return;
	}

	// 正在播放页
	if (y < coverTop && x < width / 5) { screen = Screen::Library; return; }
	const int controlRadius = max(height / 17, width / 12);
	if (abs(y - controlsY) <= controlRadius) {
		const int slot = max(0, min(4, x * 5 / max(1, width)));
		switch (slot) {
		case 0: mode = mode == Mode::ListLoop ? Mode::SingleLoop : (mode == Mode::SingleLoop ? Mode::Shuffle : Mode::ListLoop); history.clear(); break;
		case 1: Previous(post); break;
		case 2: TogglePause(post); break;
		case 3: Next(post, true); break;
		case 4: screen = Screen::Library; break;
		}
	}
}

// ---- 绘制 ----

void MusicApp::Render(Canvas* canvas) {
	if (screen == Screen::NowPlaying) RenderNowPlaying(canvas);
	else RenderLibrary(canvas);
}

void MusicApp::RenderLibrary(Canvas* canvas) {
	SetRgb(canvas, kBackground); // ClearScreen固定清成黑色，底色自己铺
	canvas->PutRect(0, 0, width - 1, height - 1, true);
	const int left = width / 20;

	// 列表(先画，标题区和迷你条之后盖在上面，滚动时不会画到它们上面)。
	canvas->SetFontSize(contentFont);
	const int contentHeight = canvas->GetFontHeight();
	canvas->SetFontSize(smallFont);
	const int smallHeight = canvas->GetFontHeight();
	for (size_t i = 0; i < songs.size(); i++) {
		const int index = static_cast<int>(i);
		const int y = ListTop() + index * rowHeight - scroll;
		if (y + rowHeight <= ListTop() || y >= ListBottom()) continue;
		const bool isCurrent = index == current;
		if (index == selection) {
			canvas->SetColor(28, 28, 38);
			canvas->PutRect(0, y, width - 1, y + rowHeight - 1, true);
		}
		const int indexWidth = width / 10;
		if (isCurrent) {
			SetRgb(canvas, kAccent);
			const int size = min(rowHeight / 3, indexWidth / 2);
			DrawEqualizer(canvas, left + (indexWidth - size) / 2 - indexWidth / 4, y + (rowHeight - size) / 2, size);
		}
		else {
			canvas->SetFontSize(smallFont);
			SetRgb(canvas, kSubText);
			const string number = to_string(index + 1);
			canvas->PutString(number, left + indexWidth / 4 - canvas->StringWidth(number) / 2, y + (rowHeight - smallHeight) / 2);
		}
		const int textLeft = left + indexWidth;
		const int textTop = y + (rowHeight - contentHeight - smallHeight) / 2;
		canvas->SetFontSize(contentFont);
		SetRgb(canvas, isCurrent ? kAccent : kText);
		canvas->PutString(Fit(canvas, songs[i].title, width - textLeft - left), textLeft, textTop);
		canvas->SetFontSize(smallFont);
		SetRgb(canvas, kSubText);
		string subtitle = "本地音乐";
		if (isCurrent && duration > 0.0) subtitle += " · " + FormatTime(duration);
		canvas->PutString(subtitle, textLeft, textTop + contentHeight);
		canvas->SetColor(32, 32, 42);
		canvas->PutLine(textLeft, y + rowHeight - 1, width - left, y + rowHeight - 1);
	}
	if (MaxScroll() > 0) { // 滚动条
		const int view = ListBottom() - ListTop();
		const int total = static_cast<int>(songs.size()) * rowHeight;
		const int barLength = max(20, view * view / max(1, total));
		const int barTop = ListTop() + (view - barLength) * scroll / max(1, MaxScroll());
		canvas->SetColor(80, 80, 92);
		canvas->PutRect(width - 4, barTop, width - 2, barTop + barLength, true);
	}

	// 标题区
	SetRgb(canvas, kBackground);
	canvas->PutRect(0, 0, width - 1, ListTop() - 1, true);
	canvas->SetFontSize(titleFont);
	SetRgb(canvas, kText);
	canvas->PutString("音乐", left, headerTop);
	const int titleHeight = canvas->GetFontHeight();
	canvas->SetFontSize(smallFont);
	SetRgb(canvas, kSubText);
	const string countText = to_string(songs.size()) + " 首";
	canvas->PutString(countText, width - left - canvas->StringWidth(countText), headerTop + titleHeight - canvas->GetFontHeight());

	// 播放全部
	const int radius = playAllHeight * 38 / 100;
	const int circleX = left + radius, circleY = playAllTop + playAllHeight / 2;
	SetRgb(canvas, songs.empty() ? Scale(kAccent, 0.4f) : kAccent);
	canvas->PutCircle(circleX, circleY, radius, true);
	SetRgb(canvas, kText);
	DrawPlayIcon(canvas, circleX + radius / 10, circleY, radius);
	canvas->SetFontSize(contentFont);
	const string playAll = mode == Mode::Shuffle ? "随机播放全部" : "播放全部";
	canvas->PutString(playAll, circleX + radius + width / 30, circleY - canvas->GetFontHeight() / 2);
	canvas->SetColor(36, 36, 46);
	canvas->PutLine(0, ListTop() - 1, width - 1, ListTop() - 1);

	// 空列表/出错
	if (songs.empty()) {
		canvas->SetFontSize(contentFont);
		SetRgb(canvas, libraryError ? kError : kSubText);
		PutCentered(canvas, libraryError ? "无法读取音乐列表" : "Music 文件夹里没有音乐", width / 2, (ListTop() + ListBottom()) / 2 - canvas->GetFontHeight() / 2);
	}

	if (HasMiniBar()) RenderMiniBar(canvas);
}

void MusicApp::RenderMiniBar(Canvas* canvas) {
	const int top = MiniBarTop();
	canvas->SetColor(30, 30, 40);
	canvas->PutRect(0, top, width - 1, usableBottom - 1, true);

	// 顶边进度线
	canvas->SetColor(58, 58, 70);
	canvas->PutRect(0, top, width - 1, top + miniLine - 1, true);
	if (duration > 0.0) {
		SetRgb(canvas, kAccent);
		const int filled = static_cast<int>(width * min(1.0, position / duration));
		if (filled > 0) canvas->PutRect(0, top, filled - 1, top + miniLine - 1, true);
	}

	const int pad = miniHeight / 8;
	const int coverSide = miniHeight - miniLine - pad * 2;
	const int coverX = width / 30;
	const Song& song = songs[current];
	DrawCover(canvas, coverX, top + miniLine + pad, coverSide, song.title);

	const int textLeft = coverX + coverSide + width / 30;
	const int textRight = miniPlayX - miniIcon - width / 40;
	canvas->SetFontSize(contentFont);
	const int contentHeight = canvas->GetFontHeight();
	canvas->SetFontSize(smallFont);
	const int smallHeight = canvas->GetFontHeight();
	const int textTop = miniCenterY - (contentHeight + smallHeight) / 2;
	canvas->SetFontSize(contentFont);
	SetRgb(canvas, kText);
	canvas->PutString(Fit(canvas, song.title, textRight - textLeft), textLeft, textTop);
	canvas->SetFontSize(smallFont);
	string subtitle;
	if (state == "error" || !lastError.empty()) { SetRgb(canvas, kError); subtitle = "无法播放"; }
	else if (state == "loading") { SetRgb(canvas, kSubText); subtitle = "加载中…"; }
	else { SetRgb(canvas, kSubText); subtitle = FormatTime(position) + " / " + FormatTime(duration); }
	canvas->PutString(Fit(canvas, subtitle, textRight - textLeft), textLeft, textTop + contentHeight);

	SetRgb(canvas, kText);
	if (state == "playing" || state == "loading") DrawPauseIcon(canvas, miniPlayX, miniCenterY, miniIcon);
	else DrawPlayIcon(canvas, miniPlayX, miniCenterY, miniIcon);
	DrawSkipIcon(canvas, miniNextX, miniCenterY, miniIcon * 8 / 10, true);
}

void MusicApp::RenderNowPlaying(Canvas* canvas) {
	const string title = current >= 0 ? songs[current].title : string();
	// 背景：顶部带一点封面色，往下渐变到底色。
	const Rgb tint = current >= 0 ? Scale(CoverColor(title), 0.45f) : kBackground;
	const int band = max(2, height / 120);
	for (int y = 0; y < height; y += band) {
		const float t = min(1.f, static_cast<float>(y) / max(1, usableBottom * 7 / 10));
		SetRgb(canvas, Mix(tint, kBackground, t));
		canvas->PutRect(0, y, width - 1, min(height - 1, y + band - 1), true);
	}

	// 顶栏：收起箭头 + "正在播放"
	canvas->SetFontSize(smallFont);
	const int barCenter = coverTop / 2;
	SetRgb(canvas, kText);
	const int chevron = max(8, smallFont / 2);
	const int chevronX = width / 12;
	canvas->PutLine(chevronX - chevron, barCenter - chevron / 2, chevronX, barCenter + chevron / 2, 2);
	canvas->PutLine(chevronX, barCenter + chevron / 2, chevronX + chevron, barCenter - chevron / 2, 2);
	PutCentered(canvas, "正在播放", width / 2, barCenter - canvas->GetFontHeight() / 2);

	DrawCover(canvas, (width - coverSize) / 2, coverTop, coverSize, current >= 0 ? title : string("music"));

	canvas->SetFontSize(titleFont);
	SetRgb(canvas, kText);
	PutCentered(canvas, Fit(canvas, current >= 0 ? title : string("未在播放"), width * 9 / 10), width / 2, npTitleTop);
	const int subtitleTop = npTitleTop + canvas->GetFontHeight() + height / 120;
	canvas->SetFontSize(smallFont);
	string subtitle = "本地音乐";
	if (current >= 0 && (state == "error" || !lastError.empty())) { SetRgb(canvas, kError); subtitle = "无法播放"; }
	else if (state == "loading") { SetRgb(canvas, kSubText); subtitle = "加载中…"; }
	else SetRgb(canvas, kSubText);
	PutCentered(canvas, subtitle, width / 2, subtitleTop);

	// 进度条
	const int thickness = max(3, height / 250);
	const double shown = DisplayedPosition();
	const double ratio = duration > 0.0 ? max(0.0, min(1.0, shown / duration)) : 0.0;
	const int knobX = progressLeft + static_cast<int>((progressRight - progressLeft) * ratio);
	canvas->SetColor(70, 70, 82);
	canvas->PutRect(progressLeft, progressY - thickness / 2, progressRight, progressY + thickness - thickness / 2, true);
	SetRgb(canvas, kAccent);
	if (knobX > progressLeft) canvas->PutRect(progressLeft, progressY - thickness / 2, knobX, progressY + thickness - thickness / 2, true);
	SetRgb(canvas, kText);
	canvas->PutCircle(knobX, progressY, max(5, height / (seeking ? 80 : 110)), true);
	canvas->SetFontSize(smallFont);
	SetRgb(canvas, kSubText);
	const int timeTop = progressY + height / 60;
	canvas->PutString(FormatTime(shown), progressLeft, timeTop);
	const string total = FormatTime(duration);
	canvas->PutString(total, progressRight - canvas->StringWidth(total), timeTop);

	// 控制行：模式 / 上一首 / 播放暂停 / 下一首 / 列表
	const int slot = width / 5;
	const int iconSize = max(14, height / 26);
	SetRgb(canvas, kText);
	DrawModeIcon(canvas, slot / 2, controlsY, iconSize);
	canvas->SetFontSize(max(14, smallFont * 8 / 10));
	SetRgb(canvas, kSubText);
	PutCentered(canvas, ModeName(mode), slot / 2, controlsY + iconSize * 8 / 10);
	SetRgb(canvas, kText);
	DrawSkipIcon(canvas, slot + slot / 2, controlsY, iconSize, false);
	const int playRadius = max(18, height / 17);
	SetRgb(canvas, kAccent);
	canvas->PutCircle(width / 2, controlsY, playRadius, true);
	SetRgb(canvas, kText);
	if (state == "playing" || state == "loading") DrawPauseIcon(canvas, width / 2, controlsY, playRadius * 9 / 10);
	else DrawPlayIcon(canvas, width / 2 + playRadius / 10, controlsY, playRadius * 9 / 10);
	DrawSkipIcon(canvas, slot * 3 + slot / 2, controlsY, iconSize, true);
	DrawListIcon(canvas, slot * 4 + slot / 2, controlsY, iconSize);
}

void MusicApp::DrawCover(Canvas* canvas, int x, int y, int size, const string& seed) const {
	if (size <= 0) return;
	const Rgb base = CoverColor(seed);
	const int band = max(1, size / 32);
	for (int i = 0; i < size; i += band) { // 左上亮、右下暗的斜向渐变，按行近似
		SetRgb(canvas, Scale(base, 1.15f - 0.45f * i / size));
		canvas->PutRect(x, y + i, x + size - 1, min(y + size - 1, y + i + band - 1), true);
	}
	// 音符：符头椭圆 + 符干 + 符尾
	canvas->SetAlpha(0.85f);
	canvas->SetColor(255, 255, 255);
	const int stemWidth = max(2, size / 28);
	const int headX = x + size * 42 / 100, headY = y + size * 66 / 100;
	canvas->PutEllipse(headX, headY, max(2, size * 11 / 100), max(2, size * 8 / 100), true);
	const int stemX = headX + size * 10 / 100;
	canvas->PutLine(stemX, headY, stemX, y + size * 28 / 100, stemWidth);
	canvas->PutLine(stemX, y + size * 28 / 100, x + size * 66 / 100, y + size * 38 / 100, stemWidth);
	canvas->SetAlpha(1.f);
}

void MusicApp::DrawPlayIcon(Canvas* canvas, int cx, int cy, int size) const {
	const int half = size / 2;
	canvas->PutTriangle(cx - size * 35 / 100, cy - half, cx - size * 35 / 100, cy + half, cx + size * 55 / 100, cy, true);
}

void MusicApp::DrawPauseIcon(Canvas* canvas, int cx, int cy, int size) const {
	const int half = size / 2;
	const int barWidth = max(2, size * 30 / 100);
	const int gap = max(2, size * 12 / 100);
	canvas->PutRect(cx - gap - barWidth, cy - half, cx - gap, cy + half, true);
	canvas->PutRect(cx + gap, cy - half, cx + gap + barWidth, cy + half, true);
}

void MusicApp::DrawSkipIcon(Canvas* canvas, int cx, int cy, int size, bool forward) const {
	const int half = size / 2;
	const int dir = forward ? 1 : -1;
	const int barWidth = max(2, size / 7);
	canvas->PutTriangle(cx - dir * half, cy - half, cx - dir * half, cy + half, cx + dir * size * 30 / 100, cy, true);
	const int barX = cx + dir * size * 30 / 100;
	canvas->PutRect(min(barX, barX + dir * barWidth), cy - half, max(barX, barX + dir * barWidth), cy + half, true);
}

void MusicApp::DrawModeIcon(Canvas* canvas, int cx, int cy, int size) const {
	const int half = size / 2;
	const int lineWidth = max(2, size / 10);
	const int arrow = max(3, size / 5);
	if (mode == Mode::Shuffle) {
		// 两条交叉线，右端各带箭头
		const int x1 = cx - half, x2 = cx + half - arrow;
		canvas->PutLine(x1, cy - size * 30 / 100, x2, cy + size * 30 / 100, lineWidth);
		canvas->PutLine(x1, cy + size * 30 / 100, x2, cy - size * 30 / 100, lineWidth);
		canvas->PutTriangle(x2, cy - size * 30 / 100 - arrow, x2, cy - size * 30 / 100 + arrow, cx + half, cy - size * 30 / 100, true);
		canvas->PutTriangle(x2, cy + size * 30 / 100 - arrow, x2, cy + size * 30 / 100 + arrow, cx + half, cy + size * 30 / 100, true);
		return;
	}
	// 循环：一个环(上边往右、下边往左的箭头)
	const int top = cy - size * 32 / 100, bottom = cy + size * 32 / 100;
	canvas->PutLine(cx - half, top, cx + half, top, lineWidth);
	canvas->PutLine(cx + half, top, cx + half, bottom, lineWidth);
	canvas->PutLine(cx + half, bottom, cx - half, bottom, lineWidth);
	canvas->PutLine(cx - half, bottom, cx - half, top, lineWidth);
	canvas->PutTriangle(cx + size / 10, top - arrow, cx + size / 10, top + arrow, cx + size / 10 + arrow * 3 / 2, top, true);
	canvas->PutTriangle(cx - size / 10, bottom - arrow, cx - size / 10, bottom + arrow, cx - size / 10 - arrow * 3 / 2, bottom, true);
	if (mode == Mode::SingleLoop) { // 右上角一个带"1"的小圆标
		const int badge = max(6, size * 32 / 100);
		const int bx = cx + half, by = top;
		SetRgb(canvas, kAccent);
		canvas->PutCircle(bx, by, badge, true);
		SetRgb(canvas, kText);
		canvas->SetFontSize(max(10, badge * 17 / 10));
		PutCentered(canvas, "1", bx, by - canvas->GetFontHeight() / 2);
	}
}

void MusicApp::DrawListIcon(Canvas* canvas, int cx, int cy, int size) const {
	const int half = size / 2;
	const int lineWidth = max(2, size / 10);
	for (int i = -1; i <= 1; i++) {
		const int y = cy + i * size * 32 / 100;
		canvas->PutRect(cx - half, y - lineWidth / 2, cx - half + lineWidth, y + lineWidth - lineWidth / 2, true); // 圆点
		canvas->PutLine(cx - half + lineWidth * 3, y, cx + half, y, lineWidth);
	}
}

void MusicApp::DrawEqualizer(Canvas* canvas, int x, int y, int size) const {
	const bool moving = state == "playing";
	const int barWidth = max(2, size / 5);
	const int gap = max(1, size / 8);
	static const float rest[3] = { 0.35f, 0.75f, 0.5f };
	for (int i = 0; i < 3; i++) {
		const float level = moving ? 0.25f + 0.75f * static_cast<float>(fabs(sin(animTime * (5.0 + i * 1.7) + i * 1.3))) : rest[i];
		const int barHeight = max(2, static_cast<int>(size * level));
		const int left = x + i * (barWidth + gap);
		canvas->PutRect(left, y + size - barHeight, left + barWidth - 1, y + size - 1, true);
	}
}
