#include "puzzle_miner.h"


using namespace std;

int MinerPuzzle::count = 0;

static const double kPi = 3.14159265358979323846;
static const double kMaxAngle = 1.15;

void MinerPuzzle::SetParam(const string& param) {
	params.Parse(param);
}

double MinerPuzzle::Radius(Kind kind) const {
	static const double r[KIND_COUNT] = { 0.035, 0.05, 0.078, 0.045, 0.07, 0.028 };
	return r[kind];
}

static int Value(int kind) {
	static const int v[6] = { 100, 250, 500, 0, 0, 600 };
	return v[kind];
}

static double Weight(int kind) {
	static const double wgt[6] = { 1.0, 1.8, 3.0, 2.5, 4.0, 0.8 };
	return wgt[kind];
}

// 收回速度(画布高度*hook的倍数)：小金块快，大金块/中金块/小石头中，大石头慢，钻石轻所以快；勾住东西后的速度整体是原来的一半。
static double Pace(int kind) {
	static const double pace[6] = { 0.65, 0.35, 0.35, 0.35, 0.15, 0.65 };
	return pace[kind];
}

void MinerPuzzle::Init(Canvas* canvas, PostHandle* post) {
	rng.Seed(params);
	timeLimit = max(1.0, params.GetDouble("time", 60));
	difficulty = clamp(params.GetInt("difficulty", 2), 1, 3);
	hookSpeed = max(0.1, params.GetDouble("hook", 0.7));
	swingScale = max(0.01, params.GetDouble("swing", 1.0));
	movingGems = clamp(params.GetInt("moving_gems", 2), 0, 8);
	preset = params.GetString("preset", "");
	timeLeft = timeLimit;
	clock = 0;
	state = SWINGING;
	angle = 0; length = 0;
	hauledIndex = -1;
	gold = diamond = 0;
	endTime = 0;
	popups.clear();
	items.clear();
	if (preset.empty() || !ParsePreset(preset)) Generate();
}

void MinerPuzzle::Place(Kind kind, double x, double y, bool moving) {
	Item item;
	item.kind = kind;
	item.x = item.baseX = x;
	item.y = y;
	if (moving) {
		item.amp = rng.Real(0.06, 0.12);
		item.speed = rng.Real(0.5, 0.9);
		item.phase = rng.Real(0, 2 * kPi);
	}
	items.push_back(item);
}

bool MinerPuzzle::ParsePreset(const string& text) {
	static const char* names[KIND_COUNT] = { "g1", "g2", "g3", "s1", "s2", "d" };
	istringstream all(text);
	string part;
	while (getline(all, part, ';')) {
		for (char& c : part) if (c == ',') c = ' ';
		istringstream one(part);
		string type;
		double x = 0, y = 0;
		if (!(one >> type >> x >> y)) continue;
		for (int k = 0; k < KIND_COUNT; k++) if (type == names[k]) Place(static_cast<Kind>(k), x, y, false);
	}
	return !items.empty();
}

void MinerPuzzle::Generate() {
	// 各种类的权重；难度越高石块越多
	double d = difficulty;
	double weights[KIND_COUNT] = { 0.22 + 0.06 * d, 0.16, 0.08, 0.20 + 0.04 * d, 0.10 + 0.03 * d, 0.04 };
	double total = 0;
	for (double v : weights) total += v;
	int n = 16 + 4 * difficulty;
	double overlapAllow = (difficulty - 1) * 0.12; // 难度高时允许物体叠一点，形成遮挡
	double s = min(w, h);
	// 每局在靠近底部的位置放movingGems个左右移动的钻石
	for (int i = 0; i < movingGems; i++) {
		for (int attempt = 0; attempt < 60; attempt++) {
			double x = rng.Real(0.2, 0.8), y = rng.Real(0.84, 0.94);
			bool ok = true;
			for (const Item& other : items) {
				double dx = (x - other.x) * w, dy = (y - other.y) * h;
				if (sqrt(dx * dx + dy * dy) < (Radius(DIAMOND) + Radius(other.kind)) * s * 0.8) { ok = false; break; }
			}
			if (!ok && attempt < 59) continue;
			Place(DIAMOND, x, y, true);
			items.back().amp = rng.Real(0.12, 0.2);
			break;
		}
	}
	for (int i = 0; i < n; i++) {
		double pick = rng.Real(0, total);
		int kind = 0;
		for (; kind < KIND_COUNT - 1; kind++) { if (pick < weights[kind]) break; pick -= weights[kind]; }
		for (int attempt = 0; attempt < 60; attempt++) {
			double x = rng.Real(0.08, 0.92), y = rng.Real(0.3, 0.94);
			bool ok = true;
			for (const Item& other : items) {
				double dx = (x - other.x) * w, dy = (y - other.y) * h;
				double need = (Radius(static_cast<Kind>(kind)) + Radius(other.kind)) * s * (1.0 - overlapAllow);
				if (sqrt(dx * dx + dy * dy) < need) { ok = false; break; }
			}
			if (!ok) continue;
			Place(static_cast<Kind>(kind), x, y, false);   // 金块不动
			break;
		}
	}
}

JsonValue* MinerPuzzle::Finish(bool completed) {
	result = JsonValue(DATA_OBJECT);
	result["gold"] = gold;
	result["diamond"] = diamond;
	result["completed"] = completed;
	return &result;
}

JsonValue* MinerPuzzle::Loop(Canvas* canvas, int ms, PostHandle* post) {
	PuzzleInput input;
	input.Poll(canvas);
	if (input.Key(KEY_ESCAPE)) return Finish(state == ENDED);

	w = canvas->GetWidth(); h = canvas->GetHeight();
	unit = max(12, min(w, h) / 22);
	pivotX = w / 2.0;
	pivotY = h * 0.15;
	minLen = unit * 1.6;
	double dt = ms / 1000.0;
	double s = min(w, h);

	clock += dt;
	itemClock += dt;
	for (Item& item : items) if (item.amp > 0 && !item.hauled) item.x = item.baseX + item.amp * sin(itemClock * item.speed * 2 + item.phase);
	for (Popup& p : popups) p.life -= dt;
	popups.erase(remove_if(popups.begin(), popups.end(), [](const Popup& p) { return p.life <= 0; }), popups.end());

	if (state != ENDED) {
		timeLeft -= dt;
		if (timeLeft <= 0) { timeLeft = 0; state = ENDED; endTime = 0; }
	}

	if (state == SWINGING) {
		double omega = (1.3 + 0.5 * difficulty) * swingScale;
		angle = kMaxAngle * sin(clock * omega);
		length = minLen;
		if (input.anyPress) { state = EXTENDING; }
	}
	else if (state == EXTENDING) {
		length += h * hookSpeed * dt;
		double tipX = pivotX + sin(angle) * length, tipY = pivotY + cos(angle) * length;
		int hit = -1;
		double best = 1e18;
		for (int i = 0; i < static_cast<int>(items.size()); i++) {
			double dx = tipX - items[i].x * w, dy = tipY - items[i].y * h;
			double dist = sqrt(dx * dx + dy * dy);
			if (dist <= Radius(items[i].kind) * s + unit * 0.3 && dist < best) { best = dist; hit = i; }
		}
		if (hit >= 0) {
			hauledIndex = hit;
			items[hit].hauled = true;
			retractSpeed = h * hookSpeed * Pace(items[hit].kind);
			state = RETRACTING;
		}
		else if (tipX < 0 || tipX > w || tipY > h) {
			hauledIndex = -1;
			retractSpeed = h * hookSpeed * 1.4;
			state = RETRACTING;
		}
	}
	else if (state == RETRACTING) {
		length -= retractSpeed * dt;
		if (hauledIndex >= 0) {
			items[hauledIndex].x = (pivotX + sin(angle) * length) / w;
			items[hauledIndex].y = (pivotY + cos(angle) * length) / h;
		}
		if (length <= minLen) {
			length = minLen;
			if (hauledIndex >= 0) {
				Kind kind = items[hauledIndex].kind;
				int value = Value(kind);
				if (kind == DIAMOND) { diamond += value; popups.push_back({ "+" + to_string(value) + " gem", pivotX, pivotY + unit * 2.0, 1.0 }); }
				else if (value > 0) { gold += value; popups.push_back({ "+" + to_string(value), pivotX, pivotY + unit * 2.0, 1.0 }); }
				else popups.push_back({ "Junk", pivotX, pivotY + unit * 2.0, 1.0 });
				items.erase(items.begin() + hauledIndex);
				hauledIndex = -1;
				bool anyGold = false;
				for (const Item& item : items) if (item.kind <= GOLD_L || item.kind == DIAMOND) anyGold = true;
				if (!anyGold && preset.empty()) Generate(); // 场上没有可挖的东西了，补一批
			}
			state = SWINGING;
			clock = 0; // 摆动从正下方重新开始
		}
	}
	else {
		endTime += dt;
		if (endTime > 0.5 && input.anyPress) return Finish(true);
	}

	Render(canvas);
	return nullptr;
}

void MinerPuzzle::DrawItem(Canvas* canvas, const Item& item) const {
	int r = static_cast<int>(Radius(item.kind) * min(w, h));
	int cx = static_cast<int>(item.x * w), cy = static_cast<int>(item.y * h);
	switch (item.kind) {
	case GOLD_S: case GOLD_M: case GOLD_L:
		FillCircle(canvas, cx, cy, r, 200, 150, 20);
		FillCircle(canvas, cx, cy, r * 85 / 100, 245, 200, 50);
		FillCircle(canvas, cx - r / 3, cy - r / 3, r / 4, 255, 245, 170);
		break;
	case STONE_S: case STONE_L:
		FillCircle(canvas, cx, cy, r, 90, 90, 100);
		FillCircle(canvas, cx, cy, r * 85 / 100, 130, 130, 140);
		FillCircle(canvas, cx + r / 4, cy + r / 4, r / 3, 105, 105, 115);
		break;
	default: {
		canvas->SetColor(60, 210, 235);
		canvas->PutTriangle(cx, cy - r, cx - r, cy, cx + r, cy, true);
		canvas->PutTriangle(cx - r, cy, cx + r, cy, cx, cy + r, true);
		canvas->SetColor(220, 255, 255);
		canvas->PutTriangle(cx, cy - r / 2, cx - r / 2, cy, cx, cy, true);
		break;
	}
	}
}

void MinerPuzzle::Render(Canvas* canvas) {
	// 天空与地下
	FillRect(canvas, MakeRect(0, 0, w, h), 130, 90, 55);
	FillRect(canvas, MakeRect(0, 0, w, static_cast<int>(pivotY + unit)), 150, 205, 245);
	FillRect(canvas, MakeRect(0, static_cast<int>(pivotY + unit), w, unit / 2), 70, 120, 60);

	for (const Item& item : items) if (!item.hauled) DrawItem(canvas, item);

	// 矿工(一个红色小人+绞盘)
	int mx = static_cast<int>(pivotX), my = static_cast<int>(pivotY);
	FillRect(canvas, MakeRect(mx - unit * 2, my - unit, unit * 2, unit * 2), 200, 60, 50);
	FillCircle(canvas, mx - unit, my - unit - unit / 2, unit / 2, 245, 210, 170);
	FillRect(canvas, MakeRect(mx - unit * 3 / 2, my - unit * 2, unit, unit / 2), 250, 210, 60);
	FillRect(canvas, MakeRect(mx, my - unit / 2, unit, unit), 90, 90, 100);

	// 绳与钩
	double tipX = pivotX + sin(angle) * length, tipY = pivotY + cos(angle) * length;
	canvas->SetColor(40, 40, 40);
	canvas->PutLine(mx, my, static_cast<int>(tipX), static_cast<int>(tipY), max(2, unit / 6));
	FillCircle(canvas, static_cast<int>(tipX), static_cast<int>(tipY), max(3, unit / 3), 220, 220, 230);
	if (hauledIndex >= 0) DrawItem(canvas, items[hauledIndex]);

	for (const Popup& p : popups) DrawTextCentered(canvas, p.text, static_cast<int>(p.x), static_cast<int>(p.y - (1.0 - p.life) * unit * 2), unit * 3 / 2, 255, 240, 120);

	DrawText(canvas, "Gold " + to_string(gold), unit / 2, unit / 3, unit * 3 / 2, 20, 30, 60);
	string timeText = to_string(static_cast<int>(ceil(timeLeft))) + "s";
	DrawText(canvas, timeText, w - TextWidth(canvas, timeText, unit * 3 / 2) - unit / 2, unit / 3, unit * 3 / 2, 20, 30, 60);
	if (diamond > 0) DrawText(canvas, "Gem " + to_string(diamond), unit / 2, unit / 3 + unit * 2, unit, 20, 90, 120);

	if (state == ENDED) DrawResultPanel(canvas, "Time up", "Gold " + to_string(gold) + "\nGem bonus " + to_string(diamond), "Press any key to exit");
}
