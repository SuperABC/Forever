#pragma once

#include "puzzle_common.h"


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
