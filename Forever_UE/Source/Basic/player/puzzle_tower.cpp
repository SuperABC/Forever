#include "puzzle_tower.h"


using namespace std;

int TowerPuzzle::count = 0;

static const double kPi = 3.14159265358979323846;

void TowerPuzzle::SetParam(const string& param) {
	params.Parse(param);
}

void TowerPuzzle::Init(Canvas* canvas, PostHandle* post) {
	rng.Seed(params);
	lives = livesLeft = max(1, params.GetInt("lives", 3));
	swing = max(0.01, params.GetDouble("swing", 1.0));
	overlapNeed = clamp(params.GetDouble("overlap", 0.25), 0.01, 0.95);
	startPhase = params.GetDouble("startphase", -1);
	offsets.assign(1, 0.0);
	styles.assign(1, 0);
	floors = 0;
	time = camY = 0;
	toastTime = endTime = 0;
	Spawn();
}

void TowerPuzzle::Spawn() {
	state = HANGING;
	stateTime = 0;
	hookTime = 0;
	phase = startPhase >= 0 ? startPhase : rng.Real(0, 2 * kPi);
	blockStyle = rng.Int(4);
}

JsonValue* TowerPuzzle::Finish(bool completed) {
	result = JsonValue(DATA_OBJECT);
	result["floors"] = floors;
	result["completed"] = completed;
	return &result;
}

// 摆幅(像素，顶层处的最大位移)：偏移越大越摇；楼越高基础摇摆越大
double TowerPuzzle::SwayAmplitude() const {
	double sum = 0;
	for (size_t i = 1; i < offsets.size(); i++) sum += fabs(offsets[i] - offsets[i - 1]) / bw;
	return bw * min(0.55, 0.01 * floors + 0.12 * sum);
}

double TowerPuzzle::Sway(int floor) const {
	int n = static_cast<int>(offsets.size());
	if (n <= 1) return 0;
	double k = static_cast<double>(floor) / (n - 1);
	return SwayAmplitude() * pow(k, 1.5) * sin(time * 2 * kPi / 3.0);
}

double TowerPuzzle::FloorX(int floor) const {
	return w / 2.0 + offsets[floor] + Sway(floor);
}

double TowerPuzzle::HookX() const {
	double omega = 1.6 * swing * (1 + 0.04 * floors);
	return w / 2.0 + w * 0.34 * sin(phase + omega * hookTime);
}

void TowerPuzzle::Drop() {
	state = FALLING;
	stateTime = 0;
	blockX = HookX();
	blockVy = 0;
	// 吊着的房子底边在屏幕上的固定位置，换算成世界y
	double screenBottom = h * 0.13 + bh;
	blockBottom = camY + (groundScreenY - screenBottom);
}

JsonValue* TowerPuzzle::Loop(Canvas* canvas, int ms, PostHandle* post) {
	PuzzleInput input;
	input.Poll(canvas);
	if (input.Key(KEY_ESCAPE)) return Finish(state == DONE);

	w = canvas->GetWidth(); h = canvas->GetHeight();
	bw = min(w * 0.28, h * 0.2);
	bh = bw * 0.5;
	groundScreenY = h * 0.88;
	double dt = ms / 1000.0;
	time += dt;
	stateTime += dt;
	toastTime = max(0.0, toastTime - dt);

	int n = static_cast<int>(offsets.size());
	double topWorld = n * bh;                      // 顶层上沿的世界y
	double camTarget = max(0.0, topWorld - (groundScreenY - h * 0.55));
	camY += (camTarget - camY) * min(1.0, dt * 4);

	if (state == HANGING) {
		hookTime += dt;
		if (input.anyPress) Drop();
	}
	else if (state == FALLING) {
		double prev = blockBottom;
		blockVy += bh * 14 * dt;
		blockBottom -= blockVy * dt;
		if (prev >= topWorld && blockBottom <= topWorld) {
			double topX = FloorX(n - 1);
			double overlap = bw - fabs(blockX - topX);
			if (overlap >= bw * overlapNeed) {
				// 搭上：把当前位置记成静态偏移(扣掉新层的摇摆，使画面不跳)
				offsets.push_back(0);
				styles.push_back(blockStyle);
				offsets.back() = blockX - w / 2.0 - Sway(n);
				floors++;
				toast = overlap >= bw * 0.92 ? "Perfect!" : "";
				toastTime = 0.8;
				Spawn();
			}
			else {
				livesLeft--;
				state = MISSED;
				stateTime = 0;
				toast = "Missed!";
				toastTime = 0.8;
			}
		}
	}
	else if (state == MISSED) {
		blockVy += bh * 14 * dt;
		blockBottom -= blockVy * dt;
		if (stateTime > 0.9) {
			if (livesLeft <= 0) { state = DONE; endTime = 0; }
			else Spawn();
		}
	}
	else {
		endTime += dt;
		if (endTime > 0.5 && input.anyPress) return Finish(true);
	}

	Render(canvas);
	return nullptr;
}

void TowerPuzzle::DrawHouse(Canvas* canvas, double cx, double bottomY, int style, float alpha) const {
	static const int colors[4][3] = { { 220, 120, 90 }, { 230, 190, 90 }, { 110, 170, 220 }, { 150, 200, 130 } };
	const int* c = colors[style & 3];
	int x1 = static_cast<int>(cx - bw / 2), x2 = static_cast<int>(cx + bw / 2);
	int y2 = static_cast<int>(bottomY), y1 = static_cast<int>(bottomY - bh);
	PuzzleRect body{ x1, y1, x2, y2 };
	FillRect(canvas, body, c[0], c[1], c[2], alpha);
	int ww = static_cast<int>(bw / 6), wh = static_cast<int>(bh / 3);
	for (int i = 0; i < 3; i++) {
		int wx = x1 + static_cast<int>(bw * (0.14 + i * 0.3));
		FillRect(canvas, MakeRect(wx, y1 + static_cast<int>(bh * 0.28), ww, wh), 250, 250, 235, alpha);
	}
	StrokeRect(canvas, body, 60, 50, 50, 2, alpha);
}

void TowerPuzzle::Render(Canvas* canvas) {
	FillRect(canvas, MakeRect(0, 0, w, h), 190, 225, 250);
	int unit = max(12, min(w, h) / 22);
	// 地面(随镜头下移)
	double groundY = groundScreenY + camY;
	if (groundY < h) FillRect(canvas, MakeRect(0, static_cast<int>(groundY), w, h - static_cast<int>(groundY)), 110, 140, 90);

	int n = static_cast<int>(offsets.size());
	for (int i = 0; i < n; i++) {
		double bottom = groundScreenY - (i * bh - camY);
		if (bottom < -bh || bottom - bh > h) continue;
		DrawHouse(canvas, FloorX(i), bottom, i == 0 ? 3 : styles[i]);
	}

	// 吊车：横梁、绳、房子
	FillRect(canvas, MakeRect(0, unit / 2, w, unit / 2), 80, 80, 90);
	double hx = HookX();
	if (state == HANGING) {
		double by = h * 0.13 + bh;
		FillRect(canvas, MakeRect(static_cast<int>(hx) - 1, unit, 3, static_cast<int>(by - bh - unit)), 40, 40, 40);
		DrawHouse(canvas, hx, by, blockStyle);
	}
	else if (state == FALLING || state == MISSED) {
		double by = groundScreenY - (blockBottom - camY);
		DrawHouse(canvas, blockX, by, blockStyle, state == MISSED ? 0.7f : 1.f);
	}

	DrawText(canvas, "Floors " + to_string(floors), unit / 2, unit, unit * 3 / 2, 20, 30, 60);
	string heart;
	for (int i = 0; i < lives; i++) heart += i < livesLeft ? "O " : "x ";
	DrawText(canvas, heart, w - TextWidth(canvas, heart, unit * 3 / 2) - unit / 2, unit, unit * 3 / 2, 200, 40, 40);
	if (toastTime > 0 && !toast.empty()) DrawTextCentered(canvas, toast, w / 2, h / 3, unit * 2, 255, 255, 255);
	if (state == HANGING && floors == 0) DrawTextCentered(canvas, "Click / any key to drop", w / 2, h - unit, unit, 40, 40, 60);

	if (state == DONE) DrawResultPanel(canvas, "Tower finished", "Floors " + to_string(floors), "Press any key to exit");
}
