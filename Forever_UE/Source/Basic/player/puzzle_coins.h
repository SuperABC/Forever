#pragma once

#include "puzzle_common.h"


// 零花钱大作战：cols列硬币，每列在玩家一侧(画面下方)靠底堆叠，列太高(超过maxh)就输。
// 每隔interval秒所有列整体下移一格，等价于每列远端(顶部)新增1枚随机自然硬币(1/5/10/50)；
// 玩家只能操作每列最靠近自己的那一枚(底部)：吸取=取走底部连续同面值的一整组拿在手里；
// 放下=把手里整组放进当前列底部(不能让该列超过maxh)，然后只检查该列底部连续同面值块能否合成：
//   1x>=5→5(+5) 5x>=2→10(+10) 10x>=5→50(+50) 50x>=2→100(+100) 100x>=5→500(+500) 500x>=2→消除(+1000)
// 整块消耗、只生成1枚上级币放回底部，多余的不保留，之后继续检查新的底部块，直到不能合成。
// 操作：←→换列(或点列)，s/↓吸取，d/↑/空格放下，屏幕有"Grab/Drop"按钮。失败后可重新开始或退出。
// 参数(key=value)：cols(10)、maxh(10)、interval(5)秒、min_interval(1.5)、decay(0.97每次下移乘一次)、seed。
// 返回：{"score","time"存活秒数,"completed"是否因失败结束}。
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
