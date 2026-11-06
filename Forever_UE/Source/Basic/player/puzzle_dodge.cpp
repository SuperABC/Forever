#include "puzzle_dodge.h"


using namespace std;

int DodgePuzzle::count = 0;

void DodgePuzzle::SetParam(const string& param) {
	params.Parse(param);
}

void DodgePuzzle::Init(Canvas* canvas, PostHandle* post) {
	rng.Seed(params);
	speed0 = params.GetDouble("speed", 0.24);
	accel = params.GetDouble("accel", 1.15);
	interval = max(1.0, params.GetDouble("interval", 10.0));
	ratioMax = max(1.0, params.GetDouble("ratio", 3.0));
	hasImage = LoadModImage(post, "puzzle_dodge_bg.png", image);

	speed = speed0;
	redX = redY = 0.5;
	started = dead = grabbing = false;
	elapsed = 0;
	nextAccel = interval;

	// 面积相同(比红块大)，长宽比在1/ratio~ratio之间对数均匀采样。
	const double area = 0.012;
	const double dist = 0.27; // 初始离中心的距离
	const double dx[4] = { 0, 0, -dist, dist }; // 上 下 左 右
	const double dy[4] = { -dist, dist, 0, 0 };
	for (int i = 0; i < 4; i++) {
		double aspect = pow(ratioMax, rng.Real(-1.0, 1.0));
		double w = sqrt(area * aspect), h = area / w;
		blues[i] = { 0.5 + dx[i], 0.5 + dy[i], w, h, 0, 0 };
	}
}

// 蓝块出发方向：朝外的半圆里随机(避开几乎贴着场地边缘的方向)。
static const double kPi = 3.14159265358979323846;
static void PickOutward(PuzzleRng& rng, int side, double& vx, double& vy) {
	double a = rng.Real(0.2, kPi - 0.2); // 以"朝外"为中心的半圆里的角度
	double along = cos(a), out = sin(a);
	switch (side) {
	case 0: vx = along; vy = -out; break; // 上
	case 1: vx = along; vy = out; break;  // 下
	case 2: vx = -out; vy = along; break; // 左
	default: vx = out; vy = along; break; // 右
	}
}

bool DodgePuzzle::Hits() const {
	double half = kRedSize / 2;
	if (redX - half < kObstacle || redX + half > 1 - kObstacle || redY - half < kObstacle || redY + half > 1 - kObstacle) return true;
	for (const Block& b : blues) {
		if (fabs(redX - b.cx) < half + b.w / 2 && fabs(redY - b.cy) < half + b.h / 2) return true;
	}
	return false;
}

void DodgePuzzle::Step(double dt) {
	elapsed += dt;
	while (elapsed >= nextAccel) {
		speed *= accel; // 方向不变，四个一起加速
		nextAccel += interval;
	}
	for (Block& b : blues) {
		b.cx += b.vx * speed * dt;
		b.cy += b.vy * speed * dt;
		// 无视障碍，只在场地边缘反弹
		if (b.cx - b.w / 2 < 0) { b.cx = b.w / 2; b.vx = fabs(b.vx); }
		if (b.cx + b.w / 2 > 1) { b.cx = 1 - b.w / 2; b.vx = -fabs(b.vx); }
		if (b.cy - b.h / 2 < 0) { b.cy = b.h / 2; b.vy = fabs(b.vy); }
		if (b.cy + b.h / 2 > 1) { b.cy = 1 - b.h / 2; b.vy = -fabs(b.vy); }
	}
	if (Hits()) {
		dead = true;
		grabbing = false;
		deadTime = 0;
	}
}

JsonValue* DodgePuzzle::Finish(bool completed) {
	result = JsonValue(DATA_OBJECT);
	result["score"] = static_cast<int>(elapsed);
	result["time"] = elapsed;
	result["completed"] = completed;
	return &result;
}

JsonValue* DodgePuzzle::Loop(Canvas* canvas, int ms, PostHandle* post) {
	PuzzleInput input;
	input.Poll(canvas);
	if (input.Key(KEY_ESCAPE)) return Finish(dead);

	int w = canvas->GetWidth(), h = canvas->GetHeight();
	fieldSize = max(50, min(w, h));
	fieldX = (w - fieldSize) / 2;
	fieldY = (h - fieldSize) / 2;
	auto toX = [&](int px) { return (px - fieldX) / static_cast<double>(fieldSize); };
	auto toY = [&](int py) { return (py - fieldY) / static_cast<double>(fieldSize); };

	double dt = ms / 1000.0;
	if (dead) {
		deadTime += dt;
		if (deadTime > 0.5 && input.anyPress) return Finish(true);
		Render(canvas);
		return nullptr;
	}

	// 抓取红块：按下时指针在红块附近(给触摸留余量)就开始拖；松手后红块停在原地，蓝块继续飞。
	for (const MouseEvent& e : input.events) {
		if (e.down) {
			double mx = toX(e.x), my = toY(e.y);
			double reach = kRedSize * 1.6;
			if (fabs(mx - redX) <= reach && fabs(my - redY) <= reach) {
				grabbing = true;
				grabDx = redX - mx;
				grabDy = redY - my;
			}
		}
		else {
			grabbing = false;
		}
	}
	if (!input.mouse.buttons[MOUSE_LEFT]) grabbing = false;

	// 拖动：沿路径细分，防止红块一帧瞬移穿过蓝块/障碍而漏判。
	double targetX = redX, targetY = redY;
	if (grabbing) {
		targetX = clamp(toX(input.mouse.x) + grabDx, kRedSize / 2, 1 - kRedSize / 2);
		targetY = clamp(toY(input.mouse.y) + grabDy, kRedSize / 2, 1 - kRedSize / 2);
		if (!started && (fabs(targetX - redX) > 0.002 || fabs(targetY - redY) > 0.002)) {
			started = true;
			for (int i = 0; i < 4; i++) PickOutward(rng, i, blues[i].vx, blues[i].vy);
		}
	}

	int substeps = max(1, static_cast<int>(max(fabs(targetX - redX), fabs(targetY - redY)) / (kRedSize * 0.25)) + 1);
	substeps = max(substeps, static_cast<int>(dt / 0.008) + 1);
	for (int i = 1; i <= substeps && !dead; i++) {
		double fx = static_cast<double>(i) / substeps;
		double px = redX, py = redY;
		redX = px + (targetX - px) * fx;
		redY = py + (targetY - py) * fx;
		if (started) {
			// 位置按比例前进一小段，再推进时间
			Step(dt / substeps);
		}
		else if (Hits()) {
			dead = true;
		}
		if (dead) break;
		redX = px; redY = py; // 还原，下一小步重新按fx插值
	}
	if (!dead) { redX = targetX; redY = targetY; }
	else { redX = clamp(redX, kRedSize / 2, 1 - kRedSize / 2); }

	Render(canvas);
	return nullptr;
}

void DodgePuzzle::Render(Canvas* canvas) {
	int w = canvas->GetWidth(), h = canvas->GetHeight();
	FillRect(canvas, MakeRect(0, 0, w - 1, h - 1), 28, 30, 40);

	PuzzleRect field = MakeRect(fieldX, fieldY, fieldSize - 1, fieldSize - 1);
	DrawBackdrop(canvas, field, hasImage ? &image : nullptr, 0.5f);

	// 四周障碍：0.5透明度的0.5灰
	int t = static_cast<int>(kObstacle * fieldSize);
	FillRect(canvas, PuzzleRect{ field.x1, field.y1, field.x2, field.y1 + t }, 128, 128, 128, 0.5f);
	FillRect(canvas, PuzzleRect{ field.x1, field.y2 - t, field.x2, field.y2 }, 128, 128, 128, 0.5f);
	FillRect(canvas, PuzzleRect{ field.x1, field.y1 + t, field.x1 + t, field.y2 - t }, 128, 128, 128, 0.5f);
	FillRect(canvas, PuzzleRect{ field.x2 - t, field.y1 + t, field.x2, field.y2 - t }, 128, 128, 128, 0.5f);

	auto px = [&](double v) { return fieldX + static_cast<int>(v * fieldSize); };
	auto py = [&](double v) { return fieldY + static_cast<int>(v * fieldSize); };
	for (const Block& b : blues) {
		FillRect(canvas, PuzzleRect{ px(b.cx - b.w / 2), py(b.cy - b.h / 2), px(b.cx + b.w / 2), py(b.cy + b.h / 2) }, 0, 0, 255);
	}
	FillRect(canvas, PuzzleRect{ px(redX - kRedSize / 2), py(redY - kRedSize / 2), px(redX + kRedSize / 2), py(redY + kRedSize / 2) }, 255, 0, 0);

	int unit = max(14, fieldSize / 24);
	DrawText(canvas, "Time " + FormatSeconds(elapsed) + "s", field.x1 + t + 6, field.y1 + t + 6, unit, 20, 20, 20);
	if (!started && !dead) {
		DrawTextCentered(canvas, "Drag the red square to start", field.CenterX(), field.y2 - t - unit * 2, unit, 20, 20, 20);
	}
	if (dead) {
		DrawResultPanel(canvas, "Caught!", "Survived " + to_string(static_cast<int>(elapsed)) + " s", "Press any key or tap to exit");
	}
}
