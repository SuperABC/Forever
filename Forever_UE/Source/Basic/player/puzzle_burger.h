#pragma once

#include "puzzle_common.h"


class BurgerPuzzle : public PuzzleMod {
public:
	virtual void SetParam(const std::string& param) override;
	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) override;

	PUZZLE_IDENTITY(BurgerPuzzle, "puzzle_burger")

private:
	enum Ing { BUN, BEEF, CHICKEN, LETTUCE, TOMATO, ONION, KETCHUP, MAYO, SALAD, MUSTARD, ING_COUNT };
	struct Cell { int food = -1; double side[2] = { 0, 0 }; int down = 0; };
	struct Order { int mask = 0, value = 0; double remaining = 0, total = 0; };
	struct Drag { enum Src { NONE, PANTRY, GRILL, PLATE } src = NONE; int index = 0; int kind = 0; int startX = 0, startY = 0; bool moved = false; };

	JsonValue* Finish(bool completed);
	void SpawnOrder();
	int SideState(double t) const;               // 0生 1熟 2糊
	bool Ready(const Cell& cell) const;
	void Layout(int width, int height);
	PuzzleRect PantryRect(int ing) const;
	PuzzleRect CellRect(int index) const;
	PuzzleRect PlateRect(int index) const;
	void Press(int x, int y);
	void Release(int x, int y);
	void Say(const std::string& text);
	void Serve(int plate);
	void Render(Canvas* canvas);
	void DrawIngredient(Canvas* canvas, const PuzzleRect& r, int ing, bool small = false) const;
	void DrawTag(Canvas* canvas, int x, int y, int size, int ing) const;
	static void IngColor(int ing, int& r, int& g, int& b);
	static const char* IngName(int ing);

	PuzzleParams params;
	JsonValue result;
	PuzzleRng rng;

	double timeLimit = 60, cookTime = 6, burnTime = 5, orderMin = 6, orderMax = 12;
	int maxOrders = 4;
	std::vector<int> scripted;       // 测试：开头几单的固定集合
	size_t scriptedUsed = 0;

	Bitmap icons[ING_COUNT];         // puzzle_burger_<食材>.png 卡通图标
	bool hasIcon[ING_COUNT] = {};

	Cell cells[8];
	int plates[4] = { 0, 0, 0, 0 };
	std::vector<Order> orders;
	double nextOrder = 0;

	Drag drag;
	int pointerX = 0, pointerY = 0;
	std::string message;
	double messageTime = 0;

	double timeLeft = 60;
	int score = 0, served = 0, expired = 0;
	bool ended = false;
	double endTime = 0;

	int w = 480, h = 640, unit = 14;
	int margin = 14;
	PuzzleRect orderArea, grillArea, plateArea, serveRect, trashRect, pantryRow1, pantryRow2;
	double cellW = 0, cellH = 0, plateW = 0;
};
