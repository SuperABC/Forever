#pragma once

#include "puzzle_common.h"


// 节奏大师(4键，只做视觉判断，没有背景音乐)：音符从上往下落，到底部判定线时按对应轨道的键
// (d f j k，或在屏幕上点/按住对应轨道)。支持两种音符：
//   tap  单点；
//   hold 长条：头按下后需要一直按住，过程中每半拍一个判定点，长条可以中途换轨(玩家随之换键/横向滑动)。
// 谱面是文本文件(通过post "resource path"按名字找，默认puzzle_rhythm_demo.chart)，格式(#开头是注释)：
//   bpm 120                      每分钟节拍数
//   offset 2000                  第0拍对应的毫秒
//   tap <拍> <轨0-3>             单点
//   hold <起拍> <止拍> <起轨> [<拍>:<轨> ...]    长条，后面的节点表示到该拍长条换到该轨
// 判定：tap/长条头误差<=perfect(默认45ms)为Perfect，<=normal(默认110ms)为Normal，更晚为Miss；
// 长条每个判定点按住了正确轨道算Perfect，否则Miss。总分100万平均分给所有判定单元，Perfect拿满，Normal拿60%。
// 参数(key=value)：chart(谱面文件名)、speed(音符从顶到判定线的毫秒，默认1500)、lead(开始前等待毫秒，默认2000)、
// perfect、normal。
// 返回：{"score","perfect","normal","miss","maxCombo","completed"}。
class RhythmPuzzle : public PuzzleMod {
public:
	virtual void SetParam(const std::string& param) override;
	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) override;

	PUZZLE_IDENTITY(RhythmPuzzle, "puzzle_rhythm")

private:
	struct Note {
		bool hold = false;
		double time = 0, endTime = 0;
		int lane = 0;
		std::vector<std::pair<double, int>> moves; // 长条：(毫秒, 新轨道)，按时间升序
		bool headDone = false;
		int ticks = 0, nextTick = 0;
		bool held = false; // 长条头已命中，且当前还在按着正确轨道
		bool alive = false; // 长条头命中后一直按住；中途松开(或头没命中)就永久失效，后面的判定点全算Miss
		double hitTime = 0;
		int LaneAt(double t) const;
	};

	bool ParseChart(const std::string& text);
	JsonValue* Finish(bool completed);
	void Judge(int grade, double points);          // grade: 0=miss 1=normal 2=perfect
	bool TryHit(int lane, double now);
	void Layout(int w, int h);
	void Render(Canvas* canvas);
	int LaneOfX(int x) const;
	double TimeToY(double t, double now) const;

	PuzzleParams params;
	JsonValue result;

	std::string chartName = "puzzle_rhythm_demo.chart";
	double speedMs = 1500, leadMs = 2000, perfectWin = 45, normalWin = 110;
	std::vector<Note> notes;
	double lastTime = 0;
	int units = 0;
	double unitScore = 0;

	double songTime = 0;
	bool loadFailed = false, finished = false;
	double finishTime = 0;
	double scoreValue = 0;
	int perfect = 0, normal = 0, miss = 0, combo = 0, maxCombo = 0;
	std::string popup;
	int popupGrade = 0;
	double popupTime = 0;

	double lastHeld[4] = { -1e9, -1e9, -1e9, -1e9 }; // 各轨道最近一次被按住的歌曲时间
	bool keyDown[4] = { false, false, false, false };
	bool pointerDown = false;
	int pointerLane = -1;

	int fieldX = 0, fieldW = 0, judgeY = 0, topY = 0, padY = 0, padH = 0, unit = 16;
};
