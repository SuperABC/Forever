#pragma once

#include "player/app_mod.h"

#include <cstdint>
#include <string>
#include <vector>


// 本地音乐播放器：扫描resource_paths下Music文件夹里的音频文件，用"music"/"music status"两个post控制一条音轨(phone_music)。
// 曲库页 + 底部迷你播放条 + 正在播放页，列表循环/单曲循环/随机三种模式，放完自动切歌(打开过的app在后台也跑Loop)。
// 详见app_music.md。
class MusicApp : public AppMod {
public:
	MusicApp();

	static const char* GetId() { return "app_music"; }
	virtual const char* GetType() const override { return "app_music"; }
	virtual const char* GetName() override;

	virtual bool UnitTest(TestPrintFn print) override;
	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual void Loop(Canvas* canvas, int ms, PostHandle* post) override;
	virtual void Back(Canvas* canvas, PostHandle* post) override; // 正在播放 -> 曲库
	virtual void Refresh(Canvas* canvas, PostHandle* post) override;
	virtual void Exit(PostHandle* post) override; // 在任务列表里关掉：停掉自己的音轨

	enum class Mode { ListLoop, SingleLoop, Shuffle };

	// ---- 纯逻辑(UnitTest直接测) ----
	// 文件名去掉扩展名当显示名。
	static std::string DisplayName(const std::string& fileName);
	// 放完/点下一首时该放哪首；count<=0返回-1。随机模式用random(0..count-1)，不连着重复同一首。
	static int NextIndex(Mode mode, int current, int count, bool userSkip, uint32_t random);
	// 秒 -> "m:ss"。
	static std::string FormatTime(double seconds);
	// 按UTF-8字符从尾部截断到不超过maxWidth像素，截了就补"…"。measure返回一段文字的像素宽。
	template <class Measure> static std::string Ellipsize(const std::string& text, int maxWidth, Measure measure);

private:
	enum class Screen { Library, NowPlaying };

	struct Song {
		std::string file;  // resource_paths里的文件名(含扩展名)，也是music post的asset
		std::string title; // 显示名
	};

	// ---- 数据 ----
	void ScanLibrary(PostHandle* post);
	void PollStatus(PostHandle* post);
	void PlayIndex(PostHandle* post, int index, bool remember = true); // remember：随机模式下把当前这首记进history
	void TogglePause(PostHandle* post);
	void SeekTo(PostHandle* post, double seconds);
	void Next(PostHandle* post, bool userSkip);
	void Previous(PostHandle* post);
	bool PostMusic(PostHandle* post, const char* op, const std::string& asset, double position);
	uint32_t NextRandom();

	// ---- 输入 ----
	void HandleInput(Canvas* canvas, PostHandle* post);
	void OnPress(int x, int y, PostHandle* post);
	void OnRelease(int x, int y, PostHandle* post);
	void OnDrag(int x, int y);

	// ---- 布局(只依赖画布尺寸，不画也能算，后台Loop时照样对) ----
	void Layout(Canvas* canvas);
	int MiniBarTop() const { return usableBottom - miniHeight; }
	bool HasMiniBar() const { return current >= 0; }
	int ListTop() const { return listTop; }
	int ListBottom() const { return HasMiniBar() ? MiniBarTop() : usableBottom; }
	int MaxScroll() const;
	void ClampScroll();
	void EnsureVisible(int index);
	int HitRow(int x, int y) const;
	double DisplayedPosition() const; // 拖进度条时显示拖到的位置

	// ---- 绘制 ----
	void Render(Canvas* canvas);
	void RenderLibrary(Canvas* canvas);
	void RenderMiniBar(Canvas* canvas);
	void RenderNowPlaying(Canvas* canvas);
	void DrawCover(Canvas* canvas, int x, int y, int size, const std::string& seed) const;
	void DrawPlayIcon(Canvas* canvas, int cx, int cy, int size) const;
	void DrawPauseIcon(Canvas* canvas, int cx, int cy, int size) const;
	void DrawSkipIcon(Canvas* canvas, int cx, int cy, int size, bool forward) const;
	void DrawModeIcon(Canvas* canvas, int cx, int cy, int size) const;
	void DrawListIcon(Canvas* canvas, int cx, int cy, int size) const;
	void DrawEqualizer(Canvas* canvas, int x, int y, int size) const;

	std::vector<Song> songs;
	bool libraryError = false;

	// 播放状态(每帧从music status同步)。
	int current = -1;          // 正在放/暂停的那首在songs里的下标，-1=没有音轨
	std::string state;         // loading/playing/paused/ended/error
	double position = 0.0;
	double duration = 0.0;
	std::string lastError;     // 最近一次post失败的提示
	Mode mode = Mode::ListLoop;
	std::vector<int> history;  // 随机模式下"上一首"往回退用
	uint32_t randomState = 0x9e3779b9u;

	Screen screen = Screen::Library;
	int selection = 0;         // 键盘选中的行
	int scroll = 0;            // 曲库列表滚动(像素)
	double animTime = 0.0;     // 正在播放的跳动音量柱

	// 鼠标
	bool pressed = false;
	bool dragging = false;     // 列表拖动
	bool seeking = false;      // 拖进度条
	double seekRatio = 0.0;
	bool pressInList = false;  // 按下点在曲库列表区域里(只有这时才能拖动列表)
	int pressX = 0, pressY = 0, pressScroll = 0;

	// 布局
	int width = 0, height = 0;
	int usableBottom = 0;      // 手机底部导航栏(Phone::barHeight)上沿
	int titleFont = 0, contentFont = 0, smallFont = 0;
	int headerTop = 0, playAllTop = 0, playAllHeight = 0, listTop = 0, rowHeight = 0;
	int miniHeight = 0, miniLine = 0, miniPlayX = 0, miniNextX = 0, miniCenterY = 0, miniIcon = 0;
	int coverSize = 0, coverTop = 0, npTitleTop = 0, progressY = 0, progressLeft = 0, progressRight = 0, controlsY = 0;

public:
	// 实例名唯一用的命名计数器(和NotesApp一样)，存档保存/恢复，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

template <class Measure>
std::string MusicApp::Ellipsize(const std::string& text, int maxWidth, Measure measure) {
	if (maxWidth <= 0) return std::string();
	if (measure(text) <= maxWidth) return text;
	static const std::string ellipsis = "\xE2\x80\xA6"; // …
	std::string cut = text;
	while (!cut.empty()) {
		// 去掉最后一个UTF-8字符(先去续字节10xxxxxx，再去首字节)。
		size_t end = cut.size() - 1;
		while (end > 0 && (static_cast<unsigned char>(cut[end]) & 0xC0) == 0x80) end--;
		cut.erase(end);
		if (measure(cut + ellipsis) <= maxWidth) return cut + ellipsis;
	}
	return ellipsis;
}
