#include "puzzle_dance.h"


using namespace std;

int DancePuzzle::count = 0;

void DancePuzzle::SetParam(const string& param) {
	params.Parse(param);
}

static vector<int> ParseIntList(const string& text) {
	vector<int> values;
	stringstream stream(text);
	string item;
	while (getline(stream, item, ',')) {
		if (!item.empty()) values.push_back(atoi(item.c_str()));
	}
	return values;
}

void DancePuzzle::Init(Canvas* canvas, PostHandle* post) {
	rng.Seed(params);
	lengths = ParseIntList(params.GetString("lengths", "3,3,3,4,4,4,5,5,5,6,6,7,7,8,8"));
	if (lengths.empty()) lengths = { 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 7, 7, 8, 8 };
	for (int& l : lengths) l = max(1, l);
	baseTime = params.GetDouble("base", 4);
	perLen = params.GetDouble("per", 0.4);
	fixedTime = params.GetDouble("time", 0);
	perfectStart = clamp(params.GetDouble("pstart", 0.7), 0.0, 1.0);
	perfectEnd = clamp(params.GetDouble("pend", 0.9), perfectStart, 1.0);
	hasImage = LoadModImage(post, "puzzle_dance_bg.png", image);

	round = -1;
	score = perfectRounds = normalRounds = failedRounds = 0;
	streak = maxStreak = 0;
	lastPerfect = false;
	StartRound();
}

void DancePuzzle::StartRound() {
	round++;
	if (round >= static_cast<int>(lengths.size())) {
		phase = PHASE_END;
		endTime = 0;
		return;
	}
	phase = PHASE_PLAY;
	sequence.clear();
	string fixed = params.GetString("fixed", ""); // 测试用：固定箭头序列(LURD)，按回合长度循环取
	for (int i = 0; i < lengths[round]; i++) {
		if (!fixed.empty()) {
			char c = fixed[i % fixed.size()];
			sequence.push_back(c == 'L' ? DIR_LEFT : c == 'U' ? DIR_UP : c == 'R' ? DIR_RIGHT : DIR_DOWN);
		}
		else sequence.push_back(rng.Int(4));
	}
	typed = 0;
	elapsed = 0;
	roundTime = fixedTime > 0 ? fixedTime : baseTime + perLen * lengths[round];
}

void DancePuzzle::FinishRound(int points, bool perfect, bool failed) {
	if (failed) {
		failedRounds++;
		flashText = "Miss!";
		flashR = 255; flashG = 90; flashB = 90;
		streak = 0;
	}
	else if (perfect) {
		perfectRounds++;
		streak = lastPerfect ? streak + 1 : 1;
		if (lastPerfect) points += 1; // 连续完美额外+1
		maxStreak = max(maxStreak, streak);
		flashText = lastPerfect ? "Perfect! Combo +1" : "Perfect!";
		flashR = 255; flashG = 170; flashB = 40;
	}
	else {
		normalRounds++;
		streak = 0;
		flashText = "Good";
		flashR = 90; flashG = 180; flashB = 255;
	}
	lastPerfect = perfect && !failed;
	score += points;
	phase = PHASE_FLASH;
	flashTime = 0;
}

void DancePuzzle::PressDir(int dir) {
	if (phase != PHASE_PLAY) return;
	if (typed < static_cast<int>(sequence.size()) && sequence[typed] == dir) typed++;
	else typed = 0; // 按错：清空本回合输入，从头再来
}

void DancePuzzle::PressDone() {
	if (phase != PHASE_PLAY) return;
	if (typed < static_cast<int>(sequence.size())) { shake = 0.25; return; } // 还没按完
	double position = elapsed / roundTime;
	bool perfect = position >= perfectStart && position <= perfectEnd;
	FinishRound(perfect ? 2 : 1, perfect, false);
}

JsonValue* DancePuzzle::Finish(bool completed) {
	result = JsonValue(DATA_OBJECT);
	result["score"] = score;
	result["perfectRounds"] = perfectRounds;
	result["normalRounds"] = normalRounds;
	result["failedRounds"] = failedRounds;
	result["maxStreak"] = maxStreak;
	result["completed"] = completed;
	return &result;
}

void DancePuzzle::Layout(int w, int h) {
	unit = max(12, min(w, h) / 16);
	int len = phase == PHASE_END || sequence.empty() ? 8 : static_cast<int>(sequence.size());
	arrowSize = max(20, min(static_cast<int>(w * 0.9 / max(len, 3)), h / 6));
	int barH = static_cast<int>(arrowSize * 1.5);
	barArea = MakeRect(w / 40, h * 28 / 100, w - w / 20, barH);
	int timerH = max(16, h / 16);
	timerArea = MakeRect(w / 12, h * 58 / 100, w - w / 6, timerH);

	int gap = max(6, w / 60);
	int btn = min((w - gap * 6) / 5, h * 18 / 100);
	int rowW = btn * 5 + gap * 4;
	int x0 = (w - rowW) / 2, y0 = h * 72 / 100;
	for (int i = 0; i < 4; i++) dirButtons[i] = MakeRect(x0 + i * (btn + gap), y0, btn, btn);
	doneButton = MakeRect(x0 + 4 * (btn + gap), y0, btn, btn);
}

void DancePuzzle::DrawArrow(Canvas* canvas, int cx, int cy, int size, int dir, int r, int g, int b) const {
	// 箭头在"朝右"的局部坐标里定义，再按方向旋转(屏幕y向下：右0°，下90°，左180°，上270°)
	static const double kShaft[4][2] = { { -0.5, -0.13 }, { 0.1, -0.13 }, { 0.1, 0.13 }, { -0.5, 0.13 } };
	static const double kHead[3][2] = { { 0.05, -0.42 }, { 0.5, 0 }, { 0.05, 0.42 } };
	double angle = (dir == DIR_RIGHT ? 0 : dir == DIR_DOWN ? 90 : dir == DIR_LEFT ? 180 : 270) * 3.14159265358979 / 180.0;
	double c = cos(angle), s = sin(angle);
	auto map = [&](double x, double y, int& ox, int& oy) {
		ox = cx + static_cast<int>((x * c - y * s) * size);
		oy = cy + static_cast<int>((x * s + y * c) * size);
	};
	int p[4][2], q[3][2];
	for (int i = 0; i < 4; i++) map(kShaft[i][0], kShaft[i][1], p[i][0], p[i][1]);
	for (int i = 0; i < 3; i++) map(kHead[i][0], kHead[i][1], q[i][0], q[i][1]);
	canvas->SetColor(r, g, b);
	canvas->SetAlpha(1.f);
	canvas->PutTriangle(p[0][0], p[0][1], p[1][0], p[1][1], p[2][0], p[2][1], true);
	canvas->PutTriangle(p[0][0], p[0][1], p[2][0], p[2][1], p[3][0], p[3][1], true);
	canvas->PutTriangle(q[0][0], q[0][1], q[1][0], q[1][1], q[2][0], q[2][1], true);
}

JsonValue* DancePuzzle::Loop(Canvas* canvas, int ms, PostHandle* post) {
	PuzzleInput input;
	input.Poll(canvas);
	if (input.Key(KEY_ESCAPE)) return Finish(false);

	int w = canvas->GetWidth(), h = canvas->GetHeight();
	double dt = ms / 1000.0;
	Layout(w, h);

	if (phase == PHASE_END) {
		endTime += dt;
		Render(canvas);
		if (endTime > 0.5 && input.anyPress) return Finish(true);
		return nullptr;
	}

	if (phase == PHASE_PLAY) {
		for (int key : input.keys) {
			if (key == KEY_LEFT || key == 'a') PressDir(DIR_LEFT);
			else if (key == KEY_UP || key == 'w') PressDir(DIR_UP);
			else if (key == KEY_RIGHT || key == 'd') PressDir(DIR_RIGHT);
			else if (key == KEY_DOWN || key == 's') PressDir(DIR_DOWN);
			else if (key == ' ' || key == KEY_ENTER) PressDone();
		}
		for (const MouseEvent& e : input.events) {
			if (!e.down) continue;
			for (int i = 0; i < 4; i++) if (dirButtons[i].Hit(e.x, e.y)) PressDir(i);
			if (doneButton.Hit(e.x, e.y)) PressDone();
		}
		if (phase == PHASE_PLAY) {
			elapsed += dt;
			shake = max(0.0, shake - dt);
			if (elapsed >= roundTime) FinishRound(0, false, true); // 滑块走到头还没完成
		}
	}
	else if (phase == PHASE_FLASH) {
		flashTime += dt;
		if (flashTime >= 0.9) {
			StartRound();
			Layout(w, h);
		}
	}

	Render(canvas);
	return nullptr;
}

void DancePuzzle::Render(Canvas* canvas) {
	int w = canvas->GetWidth(), h = canvas->GetHeight();
	DrawBackdrop(canvas, MakeRect(0, 0, w - 1, h - 1), hasImage ? &image : nullptr, 0.5f);

	// HUD
	int totalRounds = static_cast<int>(lengths.size());
	DrawText(canvas, "Round " + to_string(min(round + 1, totalRounds)) + "/" + to_string(totalRounds), unit / 2, unit / 2, unit, 30, 30, 50);
	string scoreText = "Score " + to_string(score);
	DrawText(canvas, scoreText, w - TextWidth(canvas, scoreText, unit) - unit / 2, unit / 2, unit, 30, 30, 50);

	// 箭头序列扁条：黑色0.5透明，里面黄色箭头，已按对的加红色描边
	FillRect(canvas, barArea, 0, 0, 0, 0.5f);
	int n = static_cast<int>(sequence.size());
	if (n > 0) {
		int slot = barArea.Width() / max(n, 1);
		int size = min(arrowSize, slot * 9 / 10);
		for (int i = 0; i < n; i++) {
			int cx = barArea.x1 + slot * i + slot / 2;
			int cy = barArea.CenterY();
			if (i < typed) {
				// 等宽描边：把红箭头向四周各平移d像素叠画一圈，再在上面画黄箭头
				int d = max(2, size / 12);
				for (int k = 0; k < 16; k++) {
					double a = k * 3.14159265358979 / 8;
					DrawArrow(canvas, cx + static_cast<int>(lround(cos(a) * d)), cy + static_cast<int>(lround(sin(a) * d)), size, sequence[i], 230, 40, 40);
				}
			}
			DrawArrow(canvas, cx, cy, size, sequence[i], 255, 230, 40);
		}
	}

	// 计时条：蓝色，橙色完美区，紫色滑块
	FillRect(canvas, timerArea, 40, 90, 220);
	PuzzleRect zone = timerArea;
	zone.x1 = timerArea.x1 + static_cast<int>(timerArea.Width() * perfectStart);
	zone.x2 = timerArea.x1 + static_cast<int>(timerArea.Width() * perfectEnd);
	FillRect(canvas, zone, 255, 150, 30);
	StrokeRect(canvas, timerArea, 255, 255, 255, 2);
	double position = phase == PHASE_PLAY ? clamp(elapsed / roundTime, 0.0, 1.0) : (flashText == "Miss!" ? 1.0 : clamp(elapsed / roundTime, 0.0, 1.0));
	int sliderW = max(8, timerArea.Height() / 2);
	int sx = timerArea.x1 + static_cast<int>(timerArea.Width() * position);
	FillRect(canvas, PuzzleRect{ sx - sliderW / 2, timerArea.y1 - 6, sx + sliderW / 2, timerArea.y2 + 6 }, 160, 60, 220);
	StrokeRect(canvas, PuzzleRect{ sx - sliderW / 2, timerArea.y1 - 6, sx + sliderW / 2, timerArea.y2 + 6 }, 255, 255, 255, 2);

	// 按钮
	for (int i = 0; i < 4; i++) {
		DrawButton(canvas, dirButtons[i], "", unit, 70, 80, 120);
		DrawArrow(canvas, dirButtons[i].CenterX(), dirButtons[i].CenterY(), dirButtons[i].Width() * 6 / 10, i, 255, 255, 255);
	}
	bool ready = phase == PHASE_PLAY && typed == n && n > 0;
	DrawButton(canvas, doneButton, "OK", unit, ready ? 40 : 90, ready ? 160 : 90, ready ? 70 : 90);
	if (shake > 0) DrawTextCentered(canvas, "Finish the sequence first", w / 2, timerArea.y2 + unit * 2, unit * 3 / 4, 200, 40, 40);

	if (phase == PHASE_FLASH) {
		DrawTextCentered(canvas, flashText, w / 2, h * 18 / 100, unit * 2, flashR, flashG, flashB);
	}
	if (phase == PHASE_END) {
		string lines = "Score " + to_string(score) + "\nPerfect " + to_string(perfectRounds) + "  Good " + to_string(normalRounds) + "  Miss " + to_string(failedRounds)
			+ "\nBest combo " + to_string(maxStreak);
		DrawResultPanel(canvas, "Finished", lines, "Press any key or tap to exit");
	}
}
