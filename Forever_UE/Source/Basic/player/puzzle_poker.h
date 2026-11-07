#pragma once

#include "puzzle_common.h"


class PokerPuzzle : public PuzzleMod {
public:
	// 牌：0~51，rank=c%13(0=2...12=A)，suit=c/13。牌型分数越大越好：类别(0高牌..8同花顺)<<20 | 5个比较位(每个4位)。
	static int Evaluate(const std::vector<int>& cards);              // 5~7张取最大5张组合
	static std::string HandName(int score);
	// 结算：contrib=每人本手总投入，folded=是否弃牌，score=每人牌型分数(弃牌者无所谓)；返回每人赢回的筹码(总和=总投入)。
	static std::vector<int> Settle(const std::vector<int>& contrib, const std::vector<bool>& folded, const std::vector<int>& score);

	virtual void SetParam(const std::string& param) override;
	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) override;

	PUZZLE_IDENTITY(PokerPuzzle, "puzzle_poker")

private:
	enum Action { FOLD, CHECK, CALL, RAISE, ALLIN };
	enum State { BETTING, RUNOUT, SHOWDOWN, OVER } state = BETTING;
	struct Player {
		int chips = 0, start = 0;
		int bet = 0;       // 本轮投入
		int contrib = 0;   // 本手总投入
		bool folded = false, allIn = false, acted = false;
		int cards[2] = { 0, 0 };
		std::string last;
	};

	JsonValue* Finish(bool completed);
	void StartHand();
	void BeginRound();
	int RaiseTarget() const;                    // 本轮翻倍后的注额
	bool CanRaise(int p) const;
	int CallCost(int p) const;
	void Pay(int p, int amount);
	void Apply(int p, Action action);
	bool NeedsAction(int p) const;
	int AbleCount() const;                      // 没弃牌也没全下的人数
	void Advance();                             // 一次行动后：找下一个人/结束这一轮/结算
	void EndRound();
	void Showdown();
	Action Decide(int p);
	double Equity(int p, int sims);
	void RunInstant();                          // 退出时把剩余的AI行动自动打完
	void Layout(int width, int height);
	void Render(Canvas* canvas);
	void DrawCard(Canvas* canvas, int x, int y, int cw, int ch, int card, bool faceUp) const;
	std::string PlayerName(int p) const;

	PuzzleParams params;
	JsonValue result;
	PuzzleRng rng;

	int playerCount = 3, startChips = 2000, ante = 5;
	int aiSims = 150;
	double aiMargin = 0.22, aiBluff = 0.05;

	std::vector<Player> players;
	std::vector<int> deck;
	int deckPos = 0;
	int community[5] = { 0, 0, 0, 0, 0 };
	int communityCount = 0;
	int round = 0, dealer = -1, toAct = 0, currentBet = 0;
	int hands = 0;
	bool revealHands = false;
	std::vector<std::string> resultLines;
	double aiTimer = 0, runoutTimer = 0, overTime = 0;
	bool bust = false;
	std::string toast;
	double toastTime = 0;

	int w = 480, h = 640, unit = 14;
	PuzzleRect foldButton, callButton, raiseButton, allInButton, quitButton, nextButton;
};
