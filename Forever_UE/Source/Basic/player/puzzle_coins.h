#pragma once

#include "puzzle_common.h"


class CoinsPuzzle : public PuzzleMod {
public:
	virtual void SetParam(const std::string& param) override;
	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) override;

	PUZZLE_IDENTITY(CoinsPuzzle, "puzzle_coins")

private:
	JsonValue* Finish(bool completed);
	void NewGame();
	void Shift();
	void Grab();
	void Drop();
	void MergeBottom(int col);
	void Layout(int w, int h);
	void Render(Canvas* canvas);
	void DrawCoin(Canvas* canvas, int cx, int cy, int radius, int value) const;

	PuzzleParams params;
	PuzzleRng rng;
	JsonValue result;

	int cols = 10, maxh = 10;
	double interval0 = 5, minInterval = 1.5, decay = 0.97;

	// 每列：front=远端(上)，back=近端(下，玩家一侧)
	std::vector<std::vector<int>> columns;
	int selected = 0;
	int handValue = 0, handCount = 0;
	double interval = 5, shiftTimer = 0, elapsed = 0;
	int score = 0;
	bool failed = false;
	double failTime = 0;
	std::string message;
	double messageTime = 0;

	// 布局
	int cell = 20, gridX = 0, gridBottom = 0, gridTop = 0;
	PuzzleRect grabButton, dropButton, restartButton, exitButton, handArea;
	int unit = 14;
};
