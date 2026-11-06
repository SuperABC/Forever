#pragma once

#include "puzzle_common.h"


// 黄金矿工(矿场)：矿工在顶部，钩子绕转轴来回摆动；点击/按任意键放出钩子，碰到物体后按物体重量决定收回速度。
// 不分关卡不开商店，限时time秒，时间到结算。只有金块计入"金币"，钻石单独记"额外收入"，石块等其他物品不计分。
// 物体：大/中/小金块(500/250/100)、石块(重、慢、0分)、钻石(600，轻)；一部分金块会左右缓慢移动；
// 石块多且可能遮挡金块(钩子碰到最前面的那个就停)。难度越高摆得越快、物体越多、石块越多、移动的越多、遮挡越多。
// 每局在靠近底部放moving_gems(默认2)个左右移动的钻石，金块都不动；勾住东西往回拉的速度是原来的一半。
// 参数(key=value)：time(默认60)、moving_gems、difficulty(1~3，默认2)、hook(放钩速度，占画布高度每秒的比例，默认0.7)、seed、
// swing(摆速倍率，默认1，仅调试用)、preset(固定物体，仅测试用："类型,x,y;..."，类型g1/g2/g3/s1/s2/d，x,y是0~1的画布比例)。
// 返回：{"gold","diamond","completed"(是否正常到时结束)}。
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
