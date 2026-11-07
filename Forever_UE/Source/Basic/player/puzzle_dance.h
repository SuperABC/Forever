#pragma once

#include "puzzle_common.h"


class DancePuzzle : public PuzzleMod {
public:
	virtual void SetParam(const std::string& param) override;
	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) override;

	PUZZLE_IDENTITY(DancePuzzle, "puzzle_dance")

private:
	enum Phase { PHASE_PLAY, PHASE_FLASH, PHASE_END };
	enum Dir { DIR_LEFT, DIR_UP, DIR_RIGHT, DIR_DOWN };

	JsonValue* Finish(bool completed);
	void StartRound();
	void FinishRound(int points, bool perfect, bool failed);
	void PressDir(int dir);
	void PressDone();
	void Layout(int w, int h);
	void Render(Canvas* canvas);
	void DrawArrow(Canvas* canvas, int cx, int cy, int size, int dir, int r, int g, int b) const;

	PuzzleParams params;
	PuzzleRng rng;
	Bitmap image;
	bool hasImage = false;
	JsonValue result;

	std::vector<int> lengths;
	double baseTime = 6, perLen = 0.6, fixedTime = 0, perfectStart = 0.7, perfectEnd = 0.9;

	Phase phase = PHASE_PLAY;
	int round = 0;
	std::vector<int> sequence;
	int typed = 0;           // 已按对的个数
	double roundTime = 6, elapsed = 0, flashTime = 0;
	std::string flashText;
	int flashR = 255, flashG = 255, flashB = 255;
	bool lastPerfect = false;
	int streak = 0, maxStreak = 0;
	int score = 0, perfectRounds = 0, normalRounds = 0, failedRounds = 0;
	double endTime = 0;
	double shake = 0;

	PuzzleRect barArea, timerArea, dirButtons[4], doneButton;
	int arrowSize = 40, unit = 16;
};
