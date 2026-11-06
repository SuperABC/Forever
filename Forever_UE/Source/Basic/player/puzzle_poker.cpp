#include "puzzle_poker.h"


using namespace std;

int PokerPuzzle::count = 0;

// ---------- 牌型评估 ----------

static int Eval5(const int cards[5]) {
	int ranks[5], suits[5];
	for (int i = 0; i < 5; i++) { ranks[i] = cards[i] % 13; suits[i] = cards[i] / 13; }
	sort(ranks, ranks + 5, greater<int>());
	bool flush = true;
	for (int i = 1; i < 5; i++) if (suits[i] != suits[0]) flush = false;

	int cnt[13] = { 0 };
	for (int i = 0; i < 5; i++) cnt[ranks[i]]++;
	// 按(张数降序, 点数降序)排出比较序列
	int order[5], n = 0;
	for (int c = 4; c >= 1; c--)
		for (int r = 12; r >= 0; r--)
			if (cnt[r] == c) for (int k = 0; k < c; k++) order[n++] = r;

	bool straight = false;
	int top = ranks[0];
	if (cnt[ranks[0]] == 1 && cnt[ranks[1]] == 1 && cnt[ranks[2]] == 1 && cnt[ranks[3]] == 1 && cnt[ranks[4]] == 1) {
		if (ranks[0] - ranks[4] == 4) straight = true;
		else if (ranks[0] == 12 && ranks[1] == 3 && ranks[2] == 2 && ranks[3] == 1 && ranks[4] == 0) { straight = true; top = 3; } // A-5
	}
	int maxCount = cnt[order[0]];
	int secondCount = n > maxCount ? cnt[order[maxCount]] : 0;

	int category;
	if (straight && flush) category = 8;
	else if (maxCount == 4) category = 7;
	else if (maxCount == 3 && secondCount == 2) category = 6;
	else if (flush) category = 5;
	else if (straight) category = 4;
	else if (maxCount == 3) category = 3;
	else if (maxCount == 2 && secondCount == 2) category = 2;
	else if (maxCount == 2) category = 1;
	else category = 0;

	int key[5];
	if (straight) { for (int i = 0; i < 5; i++) key[i] = i == 0 ? top : 0; }
	else {
		// 对子/三条/葫芦等：order已经是比较序列(对子在前)；去重成"每种点数一个位置"会丢信息，所以直接用5个位置
		for (int i = 0; i < 5; i++) key[i] = order[i];
	}
	int score = category << 20;
	for (int i = 0; i < 5; i++) score |= key[i] << (16 - 4 * i);
	return score;
}

int PokerPuzzle::Evaluate(const vector<int>& cards) {
	int n = static_cast<int>(cards.size());
	if (n < 5) return 0;
	int best = -1;
	int pick[5];
	for (int a = 0; a < n - 4; a++)
		for (int b = a + 1; b < n - 3; b++)
			for (int c = b + 1; c < n - 2; c++)
				for (int d = c + 1; d < n - 1; d++)
					for (int e = d + 1; e < n; e++) {
						pick[0] = cards[a]; pick[1] = cards[b]; pick[2] = cards[c]; pick[3] = cards[d]; pick[4] = cards[e];
						best = max(best, Eval5(pick));
					}
	return best;
}

string PokerPuzzle::HandName(int score) {
	static const char* names[9] = { "High Card", "Pair", "Two Pair", "Three of a Kind", "Straight", "Flush", "Full House", "Four of a Kind", "Straight Flush" };
	return names[clamp(score >> 20, 0, 8)];
}

vector<int> PokerPuzzle::Settle(const vector<int>& contrib, const vector<bool>& folded, const vector<int>& score) {
	int n = static_cast<int>(contrib.size());
	vector<int> win(n, 0);
	vector<int> levels;
	for (int c : contrib) if (c > 0) levels.push_back(c);
	sort(levels.begin(), levels.end());
	levels.erase(unique(levels.begin(), levels.end()), levels.end());
	int prev = 0;
	for (int level : levels) {
		vector<int> slice(n, 0);
		int total = 0;
		for (int i = 0; i < n; i++) { slice[i] = max(0, min(contrib[i], level) - min(contrib[i], prev)); total += slice[i]; }
		vector<int> eligible;
		for (int i = 0; i < n; i++) if (!folded[i] && contrib[i] >= level) eligible.push_back(i);
		if (eligible.empty()) {
			for (int i = 0; i < n; i++) win[i] += slice[i]; // 没人能赢这一档(多投部分没被跟)：退还
		}
		else {
			int best = -1;
			for (int i : eligible) best = max(best, score[i]);
			vector<int> winners;
			for (int i : eligible) if (score[i] == best) winners.push_back(i);
			int share = total / static_cast<int>(winners.size());
			int remainder = total - share * static_cast<int>(winners.size());
			for (size_t k = 0; k < winners.size(); k++) win[winners[k]] += share + (static_cast<int>(k) < remainder ? 1 : 0);
		}
		prev = level;
	}
	return win;
}

// ---------- 流程 ----------

void PokerPuzzle::SetParam(const string& param) {
	params.Parse(param);
}

void PokerPuzzle::Init(Canvas* canvas, PostHandle* post) {
	rng.Seed(params);
	playerCount = clamp(params.GetInt("players", 3), 2, 6);
	startChips = max(10, params.GetInt("chips", 2000));
	ante = max(1, params.GetInt("ante", 5));
	string level = params.GetString("ai", "normal");
	if (level == "easy") { aiSims = 60; aiMargin = 0.30; aiBluff = 0.02; }
	else if (level == "hard") { aiSims = 300; aiMargin = 0.17; aiBluff = 0.08; }
	else { aiSims = 150; aiMargin = 0.22; aiBluff = 0.05; }
	players.assign(playerCount, Player());
	for (Player& p : players) p.chips = p.start = startChips;
	hands = 0;
	dealer = -1;
	bust = false;
	toastTime = 0;
	StartHand();
}

JsonValue* PokerPuzzle::Finish(bool completed) {
	result = JsonValue(DATA_OBJECT);
	JsonValue profit(DATA_ARRAY);
	for (const Player& p : players) profit.append(JsonValue(p.chips - p.start));
	result["profit"] = profit;
	result["hands"] = hands;
	result["completed"] = completed;
	return &result;
}

string PokerPuzzle::PlayerName(int p) const {
	return p == 0 ? "You" : "AI" + to_string(p);
}

void PokerPuzzle::StartHand() {
	deck.clear();
	for (int i = 0; i < 52; i++) deck.push_back(i);
	rng.Shuffle(deck);
	deckPos = 0;
	communityCount = 0;
	revealHands = false;
	resultLines.clear();
	dealer = (dealer + 1) % playerCount;
	for (Player& p : players) {
		p.folded = false; p.allIn = false; p.acted = false; p.bet = 0; p.contrib = 0; p.last.clear();
		p.cards[0] = deck[deckPos++];
		p.cards[1] = deck[deckPos++];
	}
	for (int i = 0; i < playerCount; i++) {
		Pay(i, min(ante, players[i].chips));
		players[i].bet = 0; // 底注不算下注轮里的投入
	}
	round = 0;
	state = BETTING;
	BeginRound();
	if (toAct < 0) { state = RUNOUT; runoutTimer = 0; } // 没人能行动(都全下了)：直接发完公共牌
}

void PokerPuzzle::BeginRound() {
	currentBet = 0;
	for (Player& p : players) { p.bet = 0; p.acted = false; if (!p.folded && !p.allIn) p.last.clear(); }
	toAct = -1;
	for (int k = 1; k <= playerCount; k++) {
		int i = (dealer + k) % playerCount;
		if (NeedsAction(i)) { toAct = i; break; }
	}
	aiTimer = 0;
}

int PokerPuzzle::RaiseTarget() const {
	return currentBet == 0 ? ante * 2 : currentBet * 2;
}

int PokerPuzzle::CallCost(int p) const {
	return min(players[p].chips, max(0, currentBet - players[p].bet));
}

bool PokerPuzzle::CanRaise(int p) const {
	// 能付得起翻倍后的注额(否则只能全下)
	return players[p].chips > RaiseTarget() - players[p].bet;
}

void PokerPuzzle::Pay(int p, int amount) {
	Player& pl = players[p];
	amount = min(amount, pl.chips);
	pl.chips -= amount;
	pl.bet += amount;
	pl.contrib += amount;
	if (pl.chips == 0) pl.allIn = true;
}

int PokerPuzzle::AbleCount() const {
	int n = 0;
	for (const Player& p : players) if (!p.folded && !p.allIn) n++;
	return n;
}

bool PokerPuzzle::NeedsAction(int p) const {
	const Player& pl = players[p];
	if (pl.folded || pl.allIn) return false;
	if (pl.bet < currentBet) return true;
	return !pl.acted && AbleCount() > 1;
}

void PokerPuzzle::Apply(int p, Action action) {
	Player& pl = players[p];
	pl.acted = true;
	switch (action) {
	case FOLD:
		pl.folded = true;
		pl.last = "Fold";
		break;
	case CHECK:
		pl.last = "Check";
		break;
	case CALL: {
		int cost = CallCost(p);
		Pay(p, cost);
		pl.last = pl.allIn ? "All-in" : "Call " + to_string(pl.bet);
		break;
	}
	case RAISE: {
		int target = RaiseTarget();
		Pay(p, target - pl.bet);
		currentBet = target;
		pl.last = "Raise " + to_string(target);
		for (int i = 0; i < playerCount; i++) if (i != p) players[i].acted = false;
		pl.acted = true;
		break;
	}
	case ALLIN: {
		Pay(p, pl.chips);
		pl.last = "All-in " + to_string(pl.bet);
		if (pl.bet > currentBet) {
			currentBet = pl.bet;
			for (int i = 0; i < playerCount; i++) if (i != p) players[i].acted = false;
			pl.acted = true;
		}
		break;
	}
	}
	Advance();
}

void PokerPuzzle::Advance() {
	int alive = 0;
	for (const Player& p : players) if (!p.folded) alive++;
	if (alive <= 1) { Showdown(); return; }
	for (int k = 1; k <= playerCount; k++) {
		int i = (toAct + k) % playerCount;
		if (NeedsAction(i)) { toAct = i; aiTimer = 0; return; }
	}
	EndRound();
}

void PokerPuzzle::EndRound() {
	round++;
	if (round >= 4) { Showdown(); return; }
	int deal = round == 1 ? 3 : 1;
	for (int i = 0; i < deal; i++) community[communityCount++] = deck[deckPos++];
	BeginRound();
	if (toAct < 0) { state = RUNOUT; runoutTimer = 0; }
}

void PokerPuzzle::Showdown() {
	// 没人下注的收尾阶段：把剩余公共牌补齐用于比牌(只有摊牌时才有意义)
	int alive = 0;
	for (const Player& p : players) if (!p.folded) alive++;
	while (alive > 1 && communityCount < 5) community[communityCount++] = deck[deckPos++];
	revealHands = alive > 1;

	vector<int> contrib(playerCount), score(playerCount, 0);
	vector<bool> folded(playerCount);
	for (int i = 0; i < playerCount; i++) {
		contrib[i] = players[i].contrib;
		folded[i] = players[i].folded;
		if (!players[i].folded && alive > 1) {
			vector<int> cards = { players[i].cards[0], players[i].cards[1] };
			for (int c = 0; c < communityCount; c++) cards.push_back(community[c]);
			score[i] = Evaluate(cards);
		}
	}
	vector<int> win = Settle(contrib, folded, score);
	resultLines.clear();
	for (int i = 0; i < playerCount; i++) {
		players[i].chips += win[i];
		int net = win[i] - contrib[i];
		if (win[i] > 0 && !players[i].folded) {
			string line = PlayerName(i) + " wins " + to_string(win[i]);
			if (alive > 1) line += " (" + HandName(score[i]) + ")";
			resultLines.push_back(line);
		}
		(void)net;
	}
	hands++;
	state = SHOWDOWN;
	bust = false;
	for (const Player& p : players) if (p.chips <= 0) bust = true;
	overTime = 0;
}

// ---------- AI ----------

double PokerPuzzle::Equity(int p, int sims) {
	vector<int> known = { players[p].cards[0], players[p].cards[1] };
	for (int c = 0; c < communityCount; c++) known.push_back(community[c]);
	vector<int> rest;
	for (int c = 0; c < 52; c++) if (find(known.begin(), known.end(), c) == known.end()) rest.push_back(c);
	int opponents = 0;
	for (int i = 0; i < playerCount; i++) if (i != p && !players[i].folded) opponents++;
	if (opponents == 0) return 1.0;
	double wins = 0;
	for (int s = 0; s < sims; s++) {
		// 部分洗牌：取出补齐公共牌和对手底牌所需的张数
		int need = (5 - communityCount) + 2 * opponents;
		for (int i = 0; i < need; i++) swap(rest[i], rest[i + rng.Int(static_cast<int>(rest.size()) - i)]);
		vector<int> mine = known;
		int idx = 0;
		vector<int> board(community, community + communityCount);
		for (int c = communityCount; c < 5; c++) { board.push_back(rest[idx]); mine.push_back(rest[idx]); idx++; }
		int myScore = Evaluate(mine);
		int better = 0, tied = 0;
		for (int o = 0; o < opponents; o++) {
			vector<int> theirs = board;
			theirs.push_back(rest[idx++]);
			theirs.push_back(rest[idx++]);
			int score = Evaluate(theirs);
			if (score > myScore) better++;
			else if (score == myScore) tied++;
		}
		if (better == 0) wins += 1.0 / (tied + 1);
	}
	return wins / sims;
}

PokerPuzzle::Action PokerPuzzle::Decide(int p) {
	const Player& pl = players[p];
	int opponents = 0;
	for (int i = 0; i < playerCount; i++) if (i != p && !players[i].folded) opponents++;
	double base = 1.0 / (opponents + 1);
	double e = Equity(p, aiSims);
	int pot = 0;
	for (const Player& q : players) pot += q.contrib;
	int toCall = max(0, currentBet - pl.bet);
	bool bluff = rng.Chance(aiBluff);

	if (toCall == 0) {
		if (e > base + aiMargin || bluff) {
			if (CanRaise(p)) return RAISE;
			if (e > 0.8) return ALLIN;
		}
		return CHECK;
	}
	if (toCall >= pl.chips) { // 跟注就是全下
		double odds = static_cast<double>(pl.chips) / (pot + pl.chips);
		return (e > odds + 0.02 || bluff) ? CALL : FOLD;
	}
	double odds = static_cast<double>(toCall) / (pot + toCall);
	if (e > base + aiMargin + 0.08 && CanRaise(p)) return RAISE;
	if (e > 0.85 && !CanRaise(p)) return ALLIN;
	if (e >= odds + 0.03) return CALL;
	if (bluff) return CALL;
	return FOLD;
}

void PokerPuzzle::RunInstant() {
	for (int guard = 0; guard < 2000 && state != SHOWDOWN && state != OVER; guard++) {
		if (state == RUNOUT) { EndRound(); continue; }
		if (toAct < 0) break;
		Action a = toAct == 0 ? FOLD : Decide(toAct);
		Apply(toAct, a);
	}
}

// ---------- 输入与绘制 ----------

void PokerPuzzle::Layout(int width, int height) {
	w = width; h = height;
	unit = max(10, min(w, h) / 34);
	int bw = (w - unit * 2 - unit * 3) / 4;
	int by = static_cast<int>(h * 0.86), bh = static_cast<int>(h * 0.1);
	foldButton = MakeRect(unit, by, bw, bh);
	callButton = MakeRect(unit + (bw + unit), by, bw, bh);
	raiseButton = MakeRect(unit + (bw + unit) * 2, by, bw, bh);
	allInButton = MakeRect(unit + (bw + unit) * 3, by, bw, bh);
	quitButton = MakeRect(w - unit * 6, 2, unit * 5, unit * 2);
	nextButton = MakeRect(w / 2 - unit * 6, static_cast<int>(h * 0.74), unit * 12, unit * 3);
}

JsonValue* PokerPuzzle::Loop(Canvas* canvas, int ms, PostHandle* post) {
	PuzzleInput input;
	input.Poll(canvas);
	Layout(canvas->GetWidth(), canvas->GetHeight());
	double dt = ms / 1000.0;
	toastTime = max(0.0, toastTime - dt);

	bool wantQuit = input.Key(KEY_ESCAPE);
	for (const MouseEvent& e : input.events) if (e.down && quitButton.Hit(e.x, e.y) && state != OVER) wantQuit = true;
	if (wantQuit) {
		if (state == OVER) return Finish(true);
		if (state == BETTING || state == RUNOUT) {
			if (!players[0].folded) { players[0].folded = true; players[0].last = "Fold"; }
			if (state == BETTING && toAct == 0) Advance();
			RunInstant();
		}
		return Finish(false);
	}

	if (state == BETTING) {
		if (toAct == 0) {
			Action chosen = FOLD;
			bool act = false;
			bool canCheck = currentBet <= players[0].bet;
			bool canRaise = CanRaise(0);
			for (int key : input.keys) {
				if (key == 'f') { chosen = FOLD; act = true; }
				else if (key == 'c') { chosen = canCheck ? CHECK : CALL; act = true; }
				else if (key == 'r' && canRaise) { chosen = RAISE; act = true; }
				else if (key == 'a') { chosen = ALLIN; act = true; }
				if (act) break;
			}
			if (!act) for (const MouseEvent& e : input.events) {
				if (!e.down) continue;
				if (foldButton.Hit(e.x, e.y)) { chosen = FOLD; act = true; }
				else if (callButton.Hit(e.x, e.y)) { chosen = canCheck ? CHECK : CALL; act = true; }
				else if (raiseButton.Hit(e.x, e.y) && canRaise) { chosen = RAISE; act = true; }
				else if (allInButton.Hit(e.x, e.y)) { chosen = ALLIN; act = true; }
				if (act) break;
			}
			if (act) Apply(0, chosen);
		}
		else if (toAct >= 0) {
			aiTimer += dt;
			if (aiTimer >= 0.7) Apply(toAct, Decide(toAct));
		}
	}
	else if (state == RUNOUT) {
		runoutTimer += dt;
		if (runoutTimer >= 0.9) { runoutTimer = 0; EndRound(); }
	}
	else if (state == SHOWDOWN) {
		overTime += dt;
		bool next = false;
		if (overTime > 0.4) {
			if (!input.keys.empty()) next = true;
			for (const MouseEvent& e : input.events) if (e.down) next = true;
		}
		if (next) {
			if (bust) { state = OVER; overTime = 0; }
			else StartHand();
		}
	}
	else {
		overTime += dt;
		if (overTime > 0.5 && input.anyPress) return Finish(true);
	}

	Render(canvas);
	return nullptr;
}

static const char* RankText(int rank) {
	static const char* t[13] = { "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A" };
	return t[rank];
}

// 画花色图形(0黑桃 1红桃 2方块 3梅花)，s为半高。全部用圆+实心三角拼出。
static void DrawSuit(Canvas* canvas, int suit, int cx, int cy, int s, int r, int g, int b) {
	canvas->SetColor(r, g, b);
	canvas->SetAlpha(1.f);
	s = max(3, s);
	auto circle = [&](int x, int y, int rad) { canvas->PutCircle(x, y, max(1, rad), true); };
	auto tri = [&](int x1, int y1, int x2, int y2, int x3, int y3) { canvas->PutTriangle(x1, y1, x2, y2, x3, y3, true); };
	if (suit == 1) {
		circle(cx - s * 45 / 100, cy - s * 35 / 100, s * 55 / 100);
		circle(cx + s * 45 / 100, cy - s * 35 / 100, s * 55 / 100);
		tri(cx - s * 98 / 100, cy - s * 10 / 100, cx + s * 98 / 100, cy - s * 10 / 100, cx, cy + s);
	}
	else if (suit == 2) {
		tri(cx, cy - s, cx + s * 70 / 100, cy, cx - s * 70 / 100, cy);
		tri(cx, cy + s, cx + s * 70 / 100, cy, cx - s * 70 / 100, cy);
	}
	else if (suit == 0) {
		circle(cx - s * 45 / 100, cy + s * 15 / 100, s * 55 / 100);
		circle(cx + s * 45 / 100, cy + s * 15 / 100, s * 55 / 100);
		tri(cx - s * 98 / 100, cy + s * 5 / 100, cx + s * 98 / 100, cy + s * 5 / 100, cx, cy - s);
		tri(cx, cy, cx - s * 35 / 100, cy + s, cx + s * 35 / 100, cy + s);
	}
	else {
		circle(cx, cy - s * 50 / 100, s * 42 / 100);
		circle(cx - s * 50 / 100, cy + s * 20 / 100, s * 42 / 100);
		circle(cx + s * 50 / 100, cy + s * 20 / 100, s * 42 / 100);
		tri(cx, cy - s * 10 / 100, cx - s * 45 / 100, cy + s * 30 / 100, cx + s * 45 / 100, cy + s * 30 / 100);
		tri(cx, cy, cx - s * 35 / 100, cy + s, cx + s * 35 / 100, cy + s);
	}
}

void PokerPuzzle::DrawCard(Canvas* canvas, int x, int y, int cw, int ch, int card, bool faceUp) const {
	PuzzleRect r = MakeRect(x, y, cw, ch);
	if (!faceUp) {
		FillRect(canvas, r, 50, 80, 170);
		StrokeRect(canvas, r, 240, 240, 255, 2);
		StrokeRect(canvas, MakeRect(x + cw / 5, y + ch / 5, cw * 3 / 5, ch * 3 / 5), 150, 170, 230, 1);
		return;
	}
	FillRect(canvas, r, 252, 252, 248);
	StrokeRect(canvas, r, 60, 60, 60, 1);
	int suit = card / 13;
	bool red = suit == 1 || suit == 2;
	int cr = red ? 200 : 20, cg = red ? 30 : 20, cb = red ? 30 : 30;
	int rank = card % 13;
	string rankText = RankText(rank);
	bool big = cw >= 36;
	int fs = max(9, big ? cw * 26 / 100 : cw * 40 / 100);
	// 左上角索引(点数+小花色)，大牌右下角再镜像一份
	DrawText(canvas, rankText, x + cw * 7 / 100 + 1, y + 2, fs, cr, cg, cb);
	DrawSuit(canvas, suit, x + cw * 7 / 100 + 1 + TextWidth(canvas, rankText, fs) / 2, y + fs * 15 / 10 + fs * 4 / 10, max(3, fs * 35 / 100), cr, cg, cb);
	if (!big) {
		DrawSuit(canvas, suit, x + cw / 2, y + ch * 6 / 10, max(4, cw * 28 / 100), cr, cg, cb);
		return;
	}
	int rw = TextWidth(canvas, rankText, fs);
	DrawText(canvas, rankText, x + cw - rw - cw * 7 / 100 - 1, y + ch - fs * 13 / 10 - 2, fs, cr, cg, cb);
	DrawSuit(canvas, suit, x + cw - rw / 2 - cw * 7 / 100 - 1, y + ch - fs * 19 / 10 - fs * 4 / 10 - 2, max(3, fs * 35 / 100), cr, cg, cb);

	// 中间：J/Q/K画框和大字母，其余按真实纸牌排布花色
	int ps = max(4, cw * 13 / 100);
	int mx = x + cw / 2, colStep = cw * 20 / 100;
	int top = y + ch * 18 / 100, span = ch * 64 / 100;
	if (rank >= 9 && rank <= 11) {
		PuzzleRect frame = MakeRect(x + cw * 24 / 100, y + ch * 14 / 100, cw * 52 / 100, ch * 72 / 100);
		StrokeRect(canvas, frame, cr, cg, cb, 1);
		DrawTextCentered(canvas, rankText, mx, y + ch / 2, max(12, cw * 40 / 100), cr, cg, cb);
		DrawSuit(canvas, suit, mx, y + ch * 26 / 100, ps, cr, cg, cb);
		DrawSuit(canvas, suit, mx, y + ch * 74 / 100, ps, cr, cg, cb);
		return;
	}
	struct Pip { int col; double row; };   // col: -1/0/1，row: 0~1
	vector<Pip> pips;
	switch (rank) {
	case 12: pips = { {0, .5} }; break;                                                    // A
	case 0: pips = { {0, 0}, {0, 1} }; break;                                              // 2
	case 1: pips = { {0, 0}, {0, .5}, {0, 1} }; break;
	case 2: pips = { {-1, 0}, {1, 0}, {-1, 1}, {1, 1} }; break;
	case 3: pips = { {-1, 0}, {1, 0}, {0, .5}, {-1, 1}, {1, 1} }; break;
	case 4: pips = { {-1, 0}, {1, 0}, {-1, .5}, {1, .5}, {-1, 1}, {1, 1} }; break;
	case 5: pips = { {-1, 0}, {1, 0}, {0, .25}, {-1, .5}, {1, .5}, {-1, 1}, {1, 1} }; break;
	case 6: pips = { {-1, 0}, {1, 0}, {0, .25}, {-1, .5}, {1, .5}, {0, .75}, {-1, 1}, {1, 1} }; break;
	case 7: pips = { {-1, 0}, {1, 0}, {-1, 1. / 3}, {1, 1. / 3}, {0, .5}, {-1, 2. / 3}, {1, 2. / 3}, {-1, 1}, {1, 1} }; break;
	default: pips = { {-1, 0}, {1, 0}, {0, 1. / 6}, {-1, 1. / 3}, {1, 1. / 3}, {-1, 2. / 3}, {1, 2. / 3}, {0, 5. / 6}, {-1, 1}, {1, 1} }; break;
	}
	for (const Pip& p : pips) DrawSuit(canvas, suit, mx + p.col * colStep, top + static_cast<int>(p.row * span), (rank == 12) ? ps * 2 : ps, cr, cg, cb);
}

void PokerPuzzle::Render(Canvas* canvas) {
	FillRect(canvas, MakeRect(0, 0, w, h), 30, 100, 60);
	FillRect(canvas, MakeRect(0, 0, w, unit * 3 / 2 + 4), 20, 60, 40);
	DrawText(canvas, "Hand " + to_string(hands + (state == SHOWDOWN || state == OVER ? 0 : 1)), unit, 3, unit * 3 / 2, 240, 240, 200);
	DrawButton(canvas, quitButton, "Quit", unit, 120, 50, 50);

	int pot = 0;
	for (const Player& p : players) pot += p.contrib;

	// AI座位：每行最多3个
	int cols = min(3, playerCount - 1);
	int seatW = (w - unit * 2) / max(1, cols) - unit / 2;
	int seatH = static_cast<int>(h * 0.14);
	for (int i = 1; i < playerCount; i++) {
		int col = (i - 1) % 3, row = (i - 1) / 3;
		int x = unit + col * (seatW + unit / 2), y = static_cast<int>(h * 0.07) + row * (seatH + unit / 2);
		PuzzleRect box = MakeRect(x, y, seatW, seatH);
		const Player& p = players[i];
		FillRect(canvas, box, p.folded ? 40 : 25, p.folded ? 70 : 75, p.folded ? 55 : 45, 0.9f);
		StrokeRect(canvas, box, toAct == i && state == BETTING ? 255 : 90, toAct == i && state == BETTING ? 230 : 160, toAct == i && state == BETTING ? 90 : 120, 2);
		DrawText(canvas, PlayerName(i) + (dealer == i ? " (D)" : ""), x + 5, y + 3, unit, 240, 240, 220);
		DrawText(canvas, "$" + to_string(p.chips), x + 5, y + 3 + unit * 5 / 4, unit, 255, 220, 100);
		DrawText(canvas, p.last, x + 5, y + 3 + unit * 5 / 2, unit * 9 / 10, 200, 230, 255);
		int cw = max(14, seatW / 6), ch = cw * 3 / 2;
		bool up = revealHands && !p.folded;
		DrawCard(canvas, x + seatW - cw * 2 - 8, y + 4, cw, ch, p.cards[0], up);
		DrawCard(canvas, x + seatW - cw - 4, y + 4, cw, ch, p.cards[1], up);
		if (up) {
			vector<int> cards = { p.cards[0], p.cards[1] };
			for (int c = 0; c < communityCount; c++) cards.push_back(community[c]);
			DrawText(canvas, HandName(Evaluate(cards)), x + 5, y + seatH - unit * 3 / 2, unit * 8 / 10, 255, 255, 180);
		}
		if (p.folded) DrawText(canvas, "folded", x + seatW - cw * 2 - 8, y + ch + 8, unit * 8 / 10, 170, 170, 170);
	}

	// 公共牌
	int cw = static_cast<int>(w * 0.15), ch = cw * 3 / 2;
	int cy = static_cast<int>(h * 0.36);
	int totalW = cw * 5 + unit * 4;
	int cx0 = (w - totalW) / 2;
	DrawTextCentered(canvas, "Pot " + to_string(pot), w / 2, cy - unit * 3 / 2, unit * 3 / 2, 255, 235, 120);
	for (int i = 0; i < 5; i++) {
		int x = cx0 + i * (cw + unit);
		if (i < communityCount) DrawCard(canvas, x, cy, cw, ch, community[i], true);
		else FillRect(canvas, MakeRect(x, cy, cw, ch), 20, 70, 45, 0.7f);
	}

	// 玩家
	const Player& me = players[0];
	int py = static_cast<int>(h * 0.54);
	int mw = static_cast<int>(w * 0.14), mh = mw * 3 / 2;
	DrawTextCentered(canvas, "You" + string(dealer == 0 ? " (D)" : "") + "   $" + to_string(me.chips), w / 2, py + unit, unit * 3 / 2, 255, 235, 150);
	int cardY = py + unit * 3;
	DrawCard(canvas, w / 2 - mw - unit / 2, cardY, mw, mh, me.cards[0], true);
	DrawCard(canvas, w / 2 + unit / 2, cardY, mw, mh, me.cards[1], true);
	DrawText(canvas, me.last, unit, cardY, unit, 200, 230, 255);
	if (me.folded) DrawText(canvas, "folded", unit, cardY + unit * 2, unit, 200, 200, 200);
	if (state == SHOWDOWN || state == OVER) {
		vector<int> cards = { me.cards[0], me.cards[1] };
		for (int c = 0; c < communityCount; c++) cards.push_back(community[c]);
		if (revealHands && !me.folded) DrawText(canvas, HandName(Evaluate(cards)), w / 2 + mw + unit * 2, cardY, unit, 255, 255, 190);
	}

	// 按钮
	if (state == BETTING && toAct == 0) {
		bool canCheck = currentBet <= me.bet;
		DrawButton(canvas, foldButton, "Fold", unit, 150, 50, 50);
		DrawButton(canvas, callButton, canCheck ? "Check" : "Call " + to_string(CallCost(0)), unit, 50, 110, 70);
		bool canRaise = CanRaise(0);
		DrawButton(canvas, raiseButton, canRaise ? "Raise " + to_string(RaiseTarget()) : "Raise", unit, 70, 90, 170, canRaise);
		DrawButton(canvas, allInButton, "All-in " + to_string(me.chips), unit, 160, 100, 30);
	}
	else if (state == BETTING) {
		DrawTextCentered(canvas, PlayerName(toAct) + " thinking...", w / 2, static_cast<int>(h * 0.9), unit * 3 / 2, 230, 230, 200);
	}
	else if (state == RUNOUT) {
		DrawTextCentered(canvas, "All in - dealing...", w / 2, static_cast<int>(h * 0.9), unit * 3 / 2, 230, 230, 200);
	}

	if (state == SHOWDOWN) {
		int y = static_cast<int>(h * 0.8);
		PuzzleRect panel = MakeRect(unit, static_cast<int>(h * 0.775), w - unit * 2, static_cast<int>(h * 0.21));
		FillRect(canvas, panel, 10, 20, 15, 0.82f);
		int line = 0;
		for (const string& text : resultLines) { DrawTextCentered(canvas, text, w / 2, y + line * unit * 2, unit * 3 / 2, 255, 240, 140); line++; }
		DrawTextCentered(canvas, bust ? "Someone is out of chips - click to finish" : "Click / any key: next hand", w / 2, y + max(line, 1) * unit * 2 + unit, unit, 210, 230, 210);
	}
	if (state == OVER) {
		string text;
		for (int i = 0; i < playerCount; i++) {
			int profit = players[i].chips - players[i].start;
			text += PlayerName(i) + ": " + (profit > 0 ? "+" : "") + to_string(profit) + (i + 1 < playerCount ? "\n" : "");
		}
		DrawResultPanel(canvas, "Game over", text, "Press any key to exit");
	}
}
