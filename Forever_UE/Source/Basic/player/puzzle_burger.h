#pragma once

#include "puzzle_common.h"


// 汉堡店(餐厅后厨)：限时time秒内尽量多地按订单做汉堡。全部拖拽操作(指针按下-移动-松开)。
// 屏幕自上而下：订单栏、2x4平板锅(8格)、4个盘位、出餐区+垃圾桶、食材区(面包/牛肉/鸡肉/生菜/西红柿/洋葱)和酱料区(4种)。
//  - 面包/牛肉/鸡肉先拖进空锅格；生菜/西红柿/洋葱和酱料直接拖到盘里。点一下锅里的格子翻面。
//  - 锅里每格两面各有自己的计时(只有朝下的一面累计，翻面后暂停这面、累计另一面)：累计达到cook秒变"熟"，
//    再过burn秒变"糊"。面包单面熟了就行(任一面熟且没有糊的面)；牛肉/鸡肉要两面都熟且都没糊。
//    没熟或糊了的不能拖到盘里(只能扔垃圾桶)。
//  - 4个盘位各放一个汉堡(食材集合，不分顺序，同种只能一份)。把盘拖到出餐区：与某个订单集合完全一致就得分并移除该订单；
//    不一致则出餐无效，盘被清空(不扣分)。盘拖到垃圾桶也是清空。
//  - 订单：每隔order_min~order_max秒来一单，必含面包+牛肉或鸡肉，生菜/西红柿/洋葱、4种酱料各自随机；
//    分值=10+3x额外食材数，倒计时=40秒+5秒x额外食材数，过期删除；同时最多max_orders单。
// 参数(key=value)：time(默认60)、cook(默认6)、burn(默认5)、order_min(默认6)、order_max(默认12)、max_orders(默认4)、seed、
// orders(测试用，开头几单固定："bun+beef+tom+ketchup;bun+chick"，名字bun/beef/chick/lett/tom/onion/ketchup/mayo/salad/mustard)。
// 返回：{"score","served","expired","completed"}。
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
