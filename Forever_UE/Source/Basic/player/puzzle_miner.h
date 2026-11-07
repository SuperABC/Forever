#pragma once

#include "puzzle_common.h"


class MinerPuzzle : public PuzzleMod {
public:
	virtual void SetParam(const std::string& param) override;
	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) override;

	PUZZLE_IDENTITY(MinerPuzzle, "puzzle_miner")

private:
	enum Kind { GOLD_S, GOLD_M, GOLD_L, STONE_S, STONE_L, DIAMOND, KIND_COUNT };
	struct Item {
		Kind kind;
		double x, y;          // 画布比例坐标
		double baseX;
		double amp = 0, speed = 0, phase = 0; // 移动物体左右摆动(幅度为画布宽比例)
		bool hauled = false;
	};
	struct Popup { std::string text; double x, y, life; };

	JsonValue* Finish(bool completed);
	void Generate();
	bool ParsePreset(const std::string& text);
	void Place(Kind kind, double x, double y, bool moving);
	double Radius(Kind kind) const;                // 占min(w,h)的比例
	void Render(Canvas* canvas);
	void DrawItem(Canvas* canvas, const Item& item) const;

	PuzzleParams params;
	JsonValue result;
	PuzzleRng rng;

	double timeLimit = 60, hookSpeed = 0.7, swingScale = 1;
	int movingGems = 2;
	int difficulty = 2;
	std::string preset;

	std::vector<Item> items;
	std::vector<Popup> popups;
	double clock = 0, timeLeft = 60;   // clock：钩子摆动用，每次收回后归零
	double itemClock = 0;              // 物体左右移动用，一直连续走，不跟钩子重置
	enum State { SWINGING, EXTENDING, RETRACTING, ENDED } state = SWINGING;
	double angle = 0, length = 0;
	int hauledIndex = -1;
	double retractSpeed = 0;
	int gold = 0, diamond = 0;
	double endTime = 0;

	int w = 480, h = 640, unit = 16;
	double pivotX = 0, pivotY = 0, minLen = 0;
};
