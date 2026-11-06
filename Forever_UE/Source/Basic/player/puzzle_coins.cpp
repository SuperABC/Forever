#include "puzzle_coins.h"


using namespace std;

int CoinsPuzzle::count = 0;

namespace {
	struct MergeRule { int value, need, next, points; };
	const MergeRule kRules[] = {
		{ 1, 5, 5, 5 }, { 5, 2, 10, 10 }, { 10, 5, 50, 50 },
		{ 50, 2, 100, 100 }, { 100, 5, 500, 500 }, { 500, 2, 0, 1000 },
	};
	const int kNatural[4] = { 1, 5, 10, 50 };
}

void CoinsPuzzle::SetParam(const string& param) {
	params.Parse(param);
}

void CoinsPuzzle::Init(Canvas* canvas, PostHandle* post) {
	rng.Seed(params);
	cols = clamp(params.GetInt("cols", 10), 2, 20);
	maxh = clamp(params.GetInt("maxh", 10), 3, 30);
	interval0 = max(0.2, params.GetDouble("interval", 5));
	minInterval = max(0.1, params.GetDouble("min_interval", 1.5));
	decay = clamp(params.GetDouble("decay", 0.97), 0.5, 1.0);
	NewGame();
}

void CoinsPuzzle::NewGame() {
	columns.assign(cols, vector<int>());
	selected = cols / 2;
	handValue = handCount = 0;
	interval = interval0;
	shiftTimer = 0;
	elapsed = 0;
	score = 0;
	failed = false;
	message.clear();
	// 测试用：preset="1,1,5;10;;..."，列之间分号，列内从远端到近端；给了就不做开局下移
	string preset = params.GetString("preset", "");
	if (!preset.empty()) {
		stringstream cs(preset);
		string item;
		int c = 0;
		while (getline(cs, item, ';') && c < cols) {
			stringstream is(item);
			string v;
			while (getline(is, v, ',')) if (!v.empty()) columns[c].push_back(atoi(v.c_str()));
			c++;
		}
		return;
	}
	// 开局先下移几次，让盘面不是空的
	Shift();
	Shift();
}

void CoinsPuzzle::Shift() {
	for (auto& column : columns) {
		column.insert(column.begin(), kNatural[rng.Int(4)]); // 远端新增一枚，下移本身不触发合成
	}
	for (auto& column : columns) {
		if (static_cast<int>(column.size()) > maxh) { failed = true; failTime = 0; }
	}
}

void CoinsPuzzle::Grab() {
	vector<int>& column = columns[selected];
	if (column.empty()) return;
	int value = column.back();
	if (handCount > 0 && handValue != value) { message = "Hand holds another value"; messageTime = 1.0; return; }
	int run = 0;
	while (!column.empty() && column.back() == value) { column.pop_back(); run++; }
	handValue = value;
	handCount += run;   // 手里已有同面值时累加
}

void CoinsPuzzle::MergeBottom(int col) {
	vector<int>& column = columns[col];
	while (!column.empty()) {
		int value = column.back();
		int run = 0;
		for (int i = static_cast<int>(column.size()) - 1; i >= 0 && column[i] == value; i--) run++;
		const MergeRule* rule = nullptr;
		for (const MergeRule& r : kRules) if (r.value == value) rule = &r;
		if (!rule || run < rule->need) break;
		for (int i = 0; i < run; i++) column.pop_back(); // 整块消耗，多余的不保留
		if (rule->next > 0) column.push_back(rule->next);
		score += rule->points;
		message = "+" + to_string(rule->points);
		messageTime = 1.2;
	}
}

void CoinsPuzzle::Drop() {
	if (handCount <= 0) return;
	vector<int>& column = columns[selected];
	if (static_cast<int>(column.size()) + handCount > maxh) {
		message = "Column too full";
		messageTime = 1.0;
		return;
	}
	for (int i = 0; i < handCount; i++) column.push_back(handValue);
	handValue = handCount = 0;
	MergeBottom(selected);
}

JsonValue* CoinsPuzzle::Finish(bool completed) {
	result = JsonValue(DATA_OBJECT);
	result["score"] = score;
	result["time"] = elapsed;
	result["completed"] = completed;
	return &result;
}

void CoinsPuzzle::Layout(int w, int h) {
	unit = max(12, min(w, h) / 22);
	int controlsH = max(70, h * 16 / 100);
	int hudH = unit * 3;
	gridTop = hudH;
	gridBottom = h - controlsH - unit;
	cell = max(8, min(w / cols, (gridBottom - gridTop) / maxh));
	gridX = (w - cell * cols) / 2;

	int gap = max(6, w / 60);
	int btnW = (w - gap * 3 - unit * 2) / 3;
	int y = h - controlsH + gap;
	handArea = MakeRect(unit, y, btnW, controlsH - gap * 2);
	grabButton = MakeRect(unit + btnW + gap, y, btnW, controlsH - gap * 2);
	dropButton = MakeRect(unit + (btnW + gap) * 2, y, btnW, controlsH - gap * 2);

	int pw = w * 3 / 4, bw = pw / 2 - gap;
	int py = h * 3 / 5;
	restartButton = MakeRect(w / 8 + gap, py, bw, unit * 3);
	exitButton = MakeRect(w / 8 + pw / 2 + gap / 2, py, bw, unit * 3);
}

JsonValue* CoinsPuzzle::Loop(Canvas* canvas, int ms, PostHandle* post) {
	PuzzleInput input;
	input.Poll(canvas);
	if (input.Key(KEY_ESCAPE)) return Finish(failed);

	int w = canvas->GetWidth(), h = canvas->GetHeight();
	double dt = ms / 1000.0;
	Layout(w, h);

	if (failed) {
		failTime += dt;
		if (failTime > 0.5) {
			for (int key : input.keys) {
				if (key == 'r') { NewGame(); break; }
				return Finish(true); // 其他按键退出
			}
			for (const MouseEvent& e : input.events) {
				if (!e.down) continue;
				if (restartButton.Hit(e.x, e.y)) { NewGame(); break; }
				if (exitButton.Hit(e.x, e.y)) return Finish(true);
			}
		}
		Render(canvas);
		return nullptr;
	}

	for (int key : input.keys) {
		if (key == KEY_LEFT || key == 'a') selected = (selected + cols - 1) % cols;
		else if (key == KEY_RIGHT) selected = (selected + 1) % cols;
		else if (key == 's' || key == KEY_DOWN) Grab();
		else if (key == 'd' || key == KEY_UP || key == ' ') Drop();
	}
	for (const MouseEvent& e : input.events) {
		if (!e.down) continue;
		if (grabButton.Hit(e.x, e.y)) Grab();
		else if (dropButton.Hit(e.x, e.y)) Drop();
		else if (e.y >= gridTop && e.y <= gridBottom + unit && e.x >= gridX && e.x < gridX + cell * cols) {
			selected = (e.x - gridX) / cell;
		}
	}

	elapsed += dt;
	messageTime = max(0.0, messageTime - dt);
	shiftTimer += dt;
	while (shiftTimer >= interval && !failed) {
		shiftTimer -= interval;
		Shift();
		interval = max(minInterval, interval * decay);
	}

	Render(canvas);
	return nullptr;
}

void CoinsPuzzle::DrawCoin(Canvas* canvas, int cx, int cy, int radius, int value) const {
	int r = 200, g = 200, b = 210;
	switch (value) {
	case 1: r = 190; g = 192; b = 200; break;
	case 5: r = 205; g = 127; b = 50; break;
	case 10: r = 240; g = 200; b = 60; break;
	case 50: r = 90; g = 200; b = 120; break;
	case 100: r = 80; g = 150; b = 235; break;
	case 500: r = 190; g = 100; b = 225; break;
	}
	FillCircle(canvas, cx, cy, radius, 40, 40, 50);
	FillCircle(canvas, cx, cy, max(1, radius - 2), r, g, b);
	canvas->SetFontSize(max(8, radius));
	string label = to_string(value);
	DrawTextCentered(canvas, label, cx, cy, max(8, radius), 20, 20, 30);
}

void CoinsPuzzle::Render(Canvas* canvas) {
	int w = canvas->GetWidth(), h = canvas->GetHeight();
	FillRect(canvas, MakeRect(0, 0, w - 1, h - 1), 30, 36, 52);

	// 列：硬币从顶部(远端)往下排，新硬币从顶部进来；一列里最下面的一枚(vector末尾)是离玩家最近、可操作的。
	for (int c = 0; c < cols; c++) {
		int colTop = gridBottom - cell * maxh;
		PuzzleRect colRect = MakeRect(gridX + c * cell, colTop, cell, cell * maxh);
		FillRect(canvas, colRect, c % 2 ? 42 : 48, c % 2 ? 50 : 58, c % 2 ? 72 : 80);
		StrokeRect(canvas, colRect, 70, 80, 105, 1);
		const vector<int>& column = columns[c];
		int n = static_cast<int>(column.size());
		for (int k = 0; k < n; k++) {
			int cx = gridX + c * cell + cell / 2;
			int cy = colTop + k * cell + cell / 2;   // k=0在最顶上
			DrawCoin(canvas, cx, cy, cell / 2 - 2, column[k]);
		}
		if (c == selected) StrokeRect(canvas, colRect, 255, 230, 80, 3);
	}
	// 满高度警示线
	canvas->SetColor(220, 60, 60);
	canvas->PutLine(gridX, gridBottom - cell * maxh, gridX + cell * cols, gridBottom - cell * maxh);

	// HUD
	DrawText(canvas, "Score " + to_string(score), unit / 2, unit / 2, unit, 255, 255, 255);
	string timeText = FormatSeconds(elapsed) + "s";
	DrawText(canvas, timeText, w - TextWidth(canvas, timeText, unit) - unit / 2, unit / 2, unit, 200, 200, 210);
	PuzzleRect bar = MakeRect(w / 4, unit / 2 + unit / 4, w / 2, unit / 2);
	FillRect(canvas, bar, 20, 20, 30);
	PuzzleRect fill = bar;
	fill.x2 = bar.x1 + static_cast<int>(bar.Width() * clamp(shiftTimer / interval, 0.0, 1.0));
	FillRect(canvas, fill, 230, 120, 60);
	StrokeRect(canvas, bar, 200, 200, 210, 1);
	if (messageTime > 0 && !message.empty()) {
		DrawTextCentered(canvas, message, w / 2, unit * 2 + unit / 2, unit, 255, 230, 80);
	}

	// 手持 + 按钮
	FillRect(canvas, handArea, 20, 24, 36);
	StrokeRect(canvas, handArea, 120, 130, 160, 2);
	if (handCount > 0) {
		DrawCoin(canvas, handArea.x1 + handArea.Height() / 2, handArea.CenterY(), handArea.Height() / 2 - 6, handValue);
		DrawText(canvas, "x" + to_string(handCount), handArea.x1 + handArea.Height(), handArea.CenterY() - unit / 2, unit, 255, 255, 255);
	}
	else {
		DrawTextCentered(canvas, "Hand empty", handArea.CenterX(), handArea.CenterY(), unit * 3 / 4, 140, 150, 170);
	}
	DrawButton(canvas, grabButton, "Grab", unit, 50, 110, 170);
	DrawButton(canvas, dropButton, "Drop", unit, 50, 150, 90, handCount > 0);

	if (failed) {
		DrawResultPanel(canvas, "Overflow!", "Score " + to_string(score) + "\nTime " + FormatSeconds(elapsed) + " s", "");
		DrawButton(canvas, restartButton, "Restart", unit, 50, 130, 80);
		DrawButton(canvas, exitButton, "Exit", unit, 130, 70, 70);
	}
}
