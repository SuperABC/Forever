#pragma once

#include "puzzle_common.h"


class ChasePuzzle : public PuzzleMod {
public:
	virtual void SetParam(const std::string& param) override;
	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) override;

	PUZZLE_IDENTITY(ChasePuzzle, "puzzle_chase")

private:
	JsonValue* Finish(bool completed);
	void Wrap(Canvas* canvas, int width, int fontSize);
	void Render(Canvas* canvas);
	void DrawRunner(Canvas* canvas, int cx, int groundY, int size, bool thief, double phase) const;

	PuzzleParams params;
	JsonValue result;

	std::string article;
	double thiefSpeed = 3.33; // 字符/秒
	double lead = 15, penaltyMs = 300;

	int typed = 0, wrong = 0;
	double thiefPos = 15;
	double elapsed = 0, stunned = 0, flash = 0;
	enum State { RUNNING, WON, LOST } state = RUNNING;
	double endTime = 0;
	bool loadFailed = false;

	// 排版缓存：每行的起止字符下标
	struct Line { int begin, end; };
	std::vector<Line> lines;
	int wrapWidth = -1, wrapFont = -1;
	int unit = 16;
};
