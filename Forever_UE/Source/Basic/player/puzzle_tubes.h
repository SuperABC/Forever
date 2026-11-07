#pragma once

#include "puzzle_common.h"


class TubesPuzzle : public PuzzleMod {
public:
	virtual void SetParam(const std::string& param) override;
	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) override;

	typedef std::vector<std::vector<int>> Tubes; // 每管从底到顶

	// 倒水规则(玩法和求解器共用)：把from倒进to，返回实际倒的层数(0=不允许)
	static int Pour(Tubes& tubes, int from, int to, int capacity);
	static bool IsSolved(const Tubes& tubes, int capacity);
	// 求解器：nodeLimit内能否搜到完成状态(用于验证开局可解)
	static bool IsSolvable(const Tubes& tubes, int capacity, int nodeLimit = 200000);
	// 生成开局；adjacent返回相邻同色对数(越少越乱)
	static Tubes Generate(int colors, int capacity, int empty, PuzzleRng& rng, int* adjacent = nullptr);

	PUZZLE_IDENTITY(TubesPuzzle, "puzzle_tubes")

private:
	JsonValue* Finish(bool completed);
	void Layout(int w, int h);
	void Render(Canvas* canvas);

	PuzzleParams params;
	PuzzleRng rng;
	JsonValue result;

	int colors = 6, capacity = 4, emptyTubes = 2;
	Tubes tubes;
	int selected = -1;
	int steps = 0;
	bool solved = false;
	double solvedTime = 0;
	double messageTime = 0;

	std::vector<PuzzleRect> tubeRects;
	PuzzleRect exitButton;
	int layerH = 20, unit = 14;
};
