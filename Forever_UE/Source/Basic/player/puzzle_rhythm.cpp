#include "puzzle_rhythm.h"


using namespace std;

int RhythmPuzzle::count = 0;

namespace {
	const char kKeyNames[4] = { 'D', 'F', 'J', 'K' };
	const int kKeys[4] = { 'd', 'f', 'j', 'k' };
}

void RhythmPuzzle::SetParam(const string& param) {
	params.Parse(param);
}

int RhythmPuzzle::Note::LaneAt(double t) const {
	int current = lane;
	for (const auto& move : moves) {
		if (t >= move.first) current = move.second;
	}
	return current;
}

bool RhythmPuzzle::ParseChart(const string& text) {
	notes.clear();
	double bpm = 120, offset = 0;
	vector<string> lines;
	{
		stringstream stream(text);
		string line;
		while (getline(stream, line)) lines.push_back(line);
	}
	// 先读bpm/offset(可以写在任意位置)，再读音符
	for (const string& raw : lines) {
		string line = raw.substr(0, raw.find('#'));
		stringstream s(line);
		string word;
		if (!(s >> word)) continue;
		if (word == "bpm") s >> bpm;
		else if (word == "offset") s >> offset;
	}
	if (bpm <= 0) return false;
	double beatMs = 60000.0 / bpm;
	for (const string& raw : lines) {
		string line = raw.substr(0, raw.find('#'));
		stringstream s(line);
		string word;
		if (!(s >> word)) continue;
		Note note;
		if (word == "tap") {
			double beat; int lane;
			if (!(s >> beat >> lane)) continue;
			note.time = note.endTime = offset + beat * beatMs;
			note.lane = clamp(lane, 0, 3);
		}
		else if (word == "hold") {
			double beat, endBeat; int lane;
			if (!(s >> beat >> endBeat >> lane)) continue;
			note.hold = true;
			note.time = offset + beat * beatMs;
			note.endTime = offset + max(beat, endBeat) * beatMs;
			note.lane = clamp(lane, 0, 3);
			string node;
			while (s >> node) {
				size_t colon = node.find(':');
				if (colon == string::npos) continue;
				double nb = atof(node.substr(0, colon).c_str());
				int nl = atoi(node.substr(colon + 1).c_str());
				note.moves.push_back({ offset + nb * beatMs, clamp(nl, 0, 3) });
			}
			sort(note.moves.begin(), note.moves.end());
			note.ticks = static_cast<int>((note.endTime - note.time) / (beatMs / 2) + 1e-6);
		}
		else continue;
		notes.push_back(note);
	}
	if (notes.empty()) return false;
	sort(notes.begin(), notes.end(), [](const Note& a, const Note& b) { return a.time < b.time; });
	units = 0;
	lastTime = 0;
	for (const Note& n : notes) {
		units += 1 + n.ticks;
		lastTime = max(lastTime, n.endTime);
	}
	unitScore = 1000000.0 / max(1, units);
	return true;
}

void RhythmPuzzle::Init(Canvas* canvas, PostHandle* post) {
	chartName = params.GetString("chart", "puzzle_rhythm_demo.chart");
	speedMs = max(300.0, params.GetDouble("speed", 1500));
	leadMs = max(0.0, params.GetDouble("lead", 2000));
	perfectWin = params.GetDouble("perfect", 45);
	normalWin = max(perfectWin, params.GetDouble("normal", 110));

	string text;
	loadFailed = !LoadModTextFile(post, chartName, text) || !ParseChart(text);
	songTime = -leadMs;
	finished = false;
	scoreValue = 0;
	perfect = normal = miss = combo = maxCombo = 0;
	popup.clear();
	for (bool& k : keyDown) k = false;
	for (double& l : lastHeld) l = -1e9;
	pointerDown = false;
}

JsonValue* RhythmPuzzle::Finish(bool completed) {
	result = JsonValue(DATA_OBJECT);
	result["score"] = static_cast<int>(scoreValue + 0.5);
	result["perfect"] = perfect;
	result["normal"] = normal;
	result["miss"] = miss;
	result["maxCombo"] = maxCombo;
	result["completed"] = completed;
	return &result;
}

void RhythmPuzzle::Judge(int grade, double points) {
	if (grade == 0) {
		miss++;
		combo = 0;
		popup = "MISS";
	}
	else {
		combo++;
		maxCombo = max(maxCombo, combo);
		scoreValue += unitScore * points;
		if (grade == 2) { perfect++; popup = "PERFECT"; }
		else { normal++; popup = "GOOD"; }
	}
	popupGrade = grade;
	popupTime = 0.6;
}

bool RhythmPuzzle::TryHit(int lane, double now) {
	int best = -1;
	double bestDiff = normalWin + 1;
	for (size_t i = 0; i < notes.size(); i++) {
		const Note& n = notes[i];
		if (n.headDone || n.lane != lane) continue;
		double diff = fabs(n.time - now);
		if (diff <= normalWin && diff < bestDiff) { bestDiff = diff; best = static_cast<int>(i); }
	}
	if (best < 0) return false;
	Note& n = notes[best];
	n.headDone = true;
	if (n.hold) { n.alive = true; n.hitTime = now; }
	if (bestDiff <= perfectWin) Judge(2, 1.0);
	else Judge(1, 0.6);
	return true;
}

int RhythmPuzzle::LaneOfX(int x) const {
	if (x < fieldX || x >= fieldX + fieldW) return -1;
	return clamp((x - fieldX) * 4 / max(1, fieldW), 0, 3);
}

double RhythmPuzzle::TimeToY(double t, double now) const {
	return judgeY - (t - now) / speedMs * (judgeY - topY);
}

void RhythmPuzzle::Layout(int w, int h) {
	unit = max(12, min(w, h) / 22);
	fieldW = min(w, h * 3 / 4);
	fieldX = (w - fieldW) / 2;
	topY = unit * 2;
	padH = max(unit * 3, h / 8);
	padY = h - padH;
	// 判定线(Perfect区中心)上移，给下方留出"晚了的Normal区"，不被按键区压住
	int tapHalf = max(10, fieldW / 16) / 2;
	int lateN = static_cast<int>(normalWin * (padY - topY) / max(1.0, speedMs));
	judgeY = padY - unit / 2 - lateN - tapHalf;
}

JsonValue* RhythmPuzzle::Loop(Canvas* canvas, int ms, PostHandle* post) {
	PuzzleInput input;
	input.Poll(canvas);
	if (input.Key(KEY_ESCAPE)) return Finish(finished);

	int w = canvas->GetWidth(), h = canvas->GetHeight();
	Layout(w, h);
	double dt = ms;

	if (loadFailed) {
		FillRect(canvas, MakeRect(0, 0, w - 1, h - 1), 20, 20, 30);
		DrawTextCentered(canvas, "Chart not found:\n" + chartName, w / 2, h / 2, unit, 255, 120, 120);
		if (input.anyPress) return Finish(false);
		return nullptr;
	}

	if (finished) {
		finishTime += dt / 1000.0;
		Render(canvas);
		if (finishTime > 0.5 && input.anyPress) return Finish(true);
		return nullptr;
	}

	songTime += dt;
	double now = songTime;

	// 输入：按键边沿+指针边沿触发tap/长条头；"当前被按住的轨道"集合用来判定长条。
	for (int key : input.keys) {
		for (int lane = 0; lane < 4; lane++) {
			if (key == kKeys[lane]) { keyDown[lane] = true; TryHit(lane, now); }
		}
	}
	for (int key : input.keyUps) {
		for (int lane = 0; lane < 4; lane++) if (key == kKeys[lane]) keyDown[lane] = false;
	}
	for (const MouseEvent& e : input.events) {
		if (e.down) {
			int lane = LaneOfX(e.x);
			if (lane >= 0) { pointerDown = true; pointerLane = lane; TryHit(lane, now); }
		}
		else pointerDown = false;
	}
	if (!input.mouse.buttons[MOUSE_LEFT]) pointerDown = false;
	if (pointerDown) {
		int lane = LaneOfX(input.mouse.x);
		if (lane >= 0) pointerLane = lane;
	}
	auto laneHeld = [&](int lane) { return keyDown[lane] || (pointerDown && pointerLane == lane); };

	// 错过的头：超出normal窗口还没被命中
	for (Note& n : notes) {
		if (!n.headDone && now - n.time > normalWin) {
			n.headDone = true;
			Judge(0, 0);
		}
	}
	// 长条判定点：容许前后normalWin毫秒的误差(换轨/左右滑动时来得及换键，和Normal窗口一样宽)，所以延后normalWin再结算，
	// 看这段时间里要求的那条轨道有没有被按住过。
	for (int lane = 0; lane < 4; lane++) if (laneHeld(lane)) lastHeld[lane] = now;
	for (Note& n : notes) {
		if (!n.hold) continue;
		// 时刻t要求的轨道在normalWin内被按住过就算连上；换轨后新轨晚按也行(看t-normalWin时的轨道)
		auto linked = [&](double t) { return lastHeld[n.LaneAt(t)] >= t - normalWin || lastHeld[n.LaneAt(t - normalWin)] >= t - normalWin; };
		// 头命中后要一直按住：断开超过normalWin就断掉，之后的判定点都算Miss
		if (n.alive && now >= n.time && now >= n.hitTime + normalWin && now <= n.endTime && !linked(now)) n.alive = false;
		double beatTick = n.ticks > 0 ? (n.endTime - n.time) / n.ticks : 0;
		while (n.nextTick < n.ticks && now >= n.time + beatTick * (n.nextTick + 1) + normalWin) {
			double tickTime = n.time + beatTick * (n.nextTick + 1);
			n.nextTick++;
			if (n.alive && linked(tickTime)) Judge(2, 1.0);
			else { n.alive = false; Judge(0, 0); }
		}
		n.held = n.alive && now >= n.time - normalWin && now <= n.endTime && laneHeld(n.LaneAt(now));
	}

	popupTime = max(0.0, popupTime - dt / 1000.0);
	if (now > lastTime + 1200) { finished = true; finishTime = 0; }

	Render(canvas);
	return nullptr;
}

void RhythmPuzzle::Render(Canvas* canvas) {
	int w = canvas->GetWidth(), h = canvas->GetHeight();
	FillRect(canvas, MakeRect(0, 0, w - 1, h - 1), 14, 16, 28);
	int laneW = fieldW / 4;
	PuzzleRect field = MakeRect(fieldX, 0, fieldW - 1, h - 1);
	FillRect(canvas, field, 24, 28, 48);
	for (int lane = 0; lane <= 4; lane++) {
		canvas->SetColor(70, 80, 120);
		canvas->PutLine(fieldX + lane * laneW, 0, fieldX + lane * laneW, h - 1);
	}
	int tapH = max(10, laneW / 4);
	// 判定区：只画Perfect窗口(黄)，音符中心压在判定线上最准；前后的Normal窗口不画；每轨再画一个白色判定框
	{
		double pxPerMs = (judgeY - topY) / speedMs;
		int halfP = max(1, static_cast<int>(perfectWin * pxPerMs));
		FillRect(canvas, MakeRect(fieldX + 1, judgeY - halfP, fieldW - 2, halfP * 2), 255, 215, 40, 0.85f);
		for (int lane = 0; lane < 4; lane++) {
			bool pressed = keyDown[lane] || (pointerDown && pointerLane == lane);
			PuzzleRect box{ fieldX + lane * laneW + 4, judgeY - tapH / 2, fieldX + (lane + 1) * laneW - 4, judgeY + tapH / 2 };
			if (pressed) FillRect(canvas, box, 255, 255, 255, 0.35f);
			StrokeRect(canvas, box, 255, 255, 255, 2);
		}
	}
	canvas->SetColor(255, 255, 255);
	canvas->PutLine(fieldX, judgeY, fieldX + fieldW, judgeY, 2);

	double now = songTime;
	// 长条先画(在音符下面)
	for (const Note& n : notes) {
		if (!n.hold) continue;
		// 长条画成一根连续的粗折线：沿途顶点(轨道中心, 时间)，换轨处先竖后横，拐角用圆补成圆角，看不出分段。
		struct Vertex { int x; double y; };
		vector<Vertex> path;
		auto centerX = [&](int lane) { return fieldX + lane * laneW + laneW / 2; };
		int curLane = n.lane;
		path.push_back({ centerX(curLane), TimeToY(n.time, now) });
		for (const auto& m : n.moves) {
			if (m.first <= n.time || m.first >= n.endTime) continue;
			double my = TimeToY(m.first, now);
			path.push_back({ centerX(curLane), my });
			curLane = m.second;
			path.push_back({ centerX(curLane), my });
		}
		path.push_back({ centerX(curLane), TimeToY(n.endTime, now) });
		double yMin = topY, yMax = judgeY + tapH / 2;
		int bw = max(6, laneW / 2);
		bool active = n.headDone && n.held;
		for (int pass = 0; pass < 2; pass++) {
			int width = pass == 0 ? bw + 6 : bw;
			if (pass == 0) canvas->SetColor(255, 255, 255);
			else if (active) canvas->SetColor(255, 235, 130);
			else canvas->SetColor(255, 165, 55);
			canvas->SetAlpha(1.f);
			for (size_t i = 0; i + 1 < path.size(); i++) {
				Vertex a = path[i], b = path[i + 1];
				if (a.x == b.x) {   // 竖段：夹到可见区域
					double ya = clamp(a.y, yMin, yMax), yb = clamp(b.y, yMin, yMax);
					if (ya == yb) continue;
					canvas->PutLine(a.x, static_cast<int>(ya), b.x, static_cast<int>(yb), width);
				}
				else if (a.y >= yMin && a.y <= yMax) {   // 横段(换轨)
					canvas->PutLine(a.x, static_cast<int>(a.y), b.x, static_cast<int>(b.y), width);
				}
			}
			for (const Vertex& v : path) {
				if (v.y < yMin || v.y > yMax) continue;
				canvas->PutCircle(v.x, static_cast<int>(v.y), width / 2, true);
			}
		}
		// 头
		double hy = TimeToY(n.time, now);
		if (!n.headDone && hy >= topY - tapH && hy <= judgeY + tapH) {
			PuzzleRect head{ fieldX + n.lane * laneW + 4, static_cast<int>(hy) - tapH / 2, fieldX + (n.lane + 1) * laneW - 4, static_cast<int>(hy) + tapH / 2 };
			FillRect(canvas, head, 255, 150, 40);
			StrokeRect(canvas, head, 255, 255, 255, 2);
		}
	}
	for (const Note& n : notes) {
		if (n.hold || n.headDone) continue;
		double y = TimeToY(n.time, now);
		if (y < topY - tapH || y > judgeY + tapH * 2) continue;
		PuzzleRect r{ fieldX + n.lane * laneW + 4, static_cast<int>(y) - tapH / 2, fieldX + (n.lane + 1) * laneW - 4, static_cast<int>(y) + tapH / 2 };
		FillRect(canvas, r, 80, 200, 255);
		StrokeRect(canvas, r, 255, 255, 255, 2);
	}

	// 底部按键(也是触摸区)
	for (int lane = 0; lane < 4; lane++) {
		bool pressed = keyDown[lane] || (pointerDown && pointerLane == lane);
		PuzzleRect pad{ fieldX + lane * laneW + 3, padY, fieldX + (lane + 1) * laneW - 3, h - 4 };
		FillRect(canvas, pad, pressed ? 120 : 50, pressed ? 200 : 60, pressed ? 255 : 100);
		StrokeRect(canvas, pad, 200, 210, 240, 2);
		DrawTextCentered(canvas, string(1, kKeyNames[lane]), pad.CenterX(), pad.CenterY(), unit * 3 / 2, 255, 255, 255);
	}

	// HUD
	char scoreText[16];
	snprintf(scoreText, sizeof(scoreText), "%07d", static_cast<int>(scoreValue + 0.5));
	DrawText(canvas, scoreText, fieldX + unit / 2, unit / 3, unit, 255, 255, 255);
	double progress = lastTime > 0 ? clamp((songTime + leadMs) / (lastTime + leadMs + 1200), 0.0, 1.0) : 0;
	canvas->SetColor(255, 255, 255);
	PuzzleRect bar = MakeRect(fieldX + fieldW / 2, unit / 3 + unit / 4, fieldW / 2 - unit / 2, unit / 2);
	FillRect(canvas, bar, 50, 56, 84);
	PuzzleRect fill = bar;
	fill.x2 = bar.x1 + static_cast<int>(bar.Width() * progress);
	FillRect(canvas, fill, 90, 200, 255);

	if (combo >= 2) {
		DrawTextCentered(canvas, to_string(combo), fieldX + fieldW / 2, h * 3 / 10, unit * 3, 255, 255, 255);
		DrawTextCentered(canvas, "COMBO", fieldX + fieldW / 2, h * 3 / 10 + unit * 2, unit * 3 / 4, 200, 210, 255);
	}
	if (popupTime > 0 && !popup.empty()) {
		int r = popupGrade == 2 ? 255 : popupGrade == 1 ? 120 : 255;
		int g = popupGrade == 2 ? 220 : popupGrade == 1 ? 220 : 90;
		int b = popupGrade == 2 ? 60 : popupGrade == 1 ? 255 : 90;
		DrawTextCentered(canvas, popup, fieldX + fieldW / 2, judgeY - unit * 4, unit * 3 / 2, r, g, b);
	}
	if (songTime < 0) {
		DrawTextCentered(canvas, "Get ready\nD F J K  or  touch the lanes", fieldX + fieldW / 2, h / 2, unit, 255, 255, 255);
	}

	if (finished) {
		int total = static_cast<int>(scoreValue + 0.5);
		string rank = total >= 950000 ? "S" : total >= 850000 ? "A" : total >= 700000 ? "B" : "C";
		string lines = "Score " + to_string(total) + "   Rank " + rank + "\nPerfect " + to_string(perfect) + "  Good " + to_string(normal)
			+ "  Miss " + to_string(miss) + "\nMax combo " + to_string(maxCombo);
		DrawResultPanel(canvas, "Finished", lines, "Press any key or tap to exit");
	}
}
