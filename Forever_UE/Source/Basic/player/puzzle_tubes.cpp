#include "puzzle_tubes.h"

#include <set>


using namespace std;

int TubesPuzzle::count = 0;

namespace {
	const int kPalette[12][3] = {
		{ 230, 60, 60 }, { 60, 130, 235 }, { 80, 200, 90 }, { 245, 205, 50 },
		{ 170, 80, 220 }, { 255, 140, 40 }, { 60, 210, 210 }, { 240, 110, 170 },
		{ 140, 90, 50 }, { 150, 160, 170 }, { 20, 120, 60 }, { 250, 250, 250 },
	};
}

void TubesPuzzle::SetParam(const string& param) {
	params.Parse(param);
}

int TubesPuzzle::Pour(Tubes& tubes, int from, int to, int capacity) {
	if (from == to || from < 0 || to < 0) return 0;
	vector<int>& a = tubes[from];
	vector<int>& b = tubes[to];
	if (a.empty()) return 0;
	int color = a.back();
	int run = 0;
	for (int i = static_cast<int>(a.size()) - 1; i >= 0 && a[i] == color; i--) run++;
	if (!b.empty() && b.back() != color) return 0;
	int room = capacity - static_cast<int>(b.size());
	int amount = min(run, room);
	if (amount <= 0) return 0;
	for (int i = 0; i < amount; i++) { a.pop_back(); b.push_back(color); }
	return amount;
}

// 每个管必须是空的，或者装满且同色(同色但没满说明还有同色层在别的管里)。
bool TubesPuzzle::IsSolved(const Tubes& tubes, int capacity) {
	for (const auto& tube : tubes) {
		if (tube.empty()) continue;
		if (static_cast<int>(tube.size()) != capacity) return false;
		for (size_t i = 1; i < tube.size(); i++) if (tube[i] != tube[0]) return false;
	}
	return true;
}

static string StateKey(const TubesPuzzle::Tubes& tubes) {
	vector<string> parts;
	for (const auto& tube : tubes) {
		string s;
		for (int c : tube) s += static_cast<char>('a' + c);
		parts.push_back(s);
	}
	sort(parts.begin(), parts.end()); // 管的顺序无关
	string key;
	for (const string& p : parts) { key += p; key += '|'; }
	return key;
}

bool TubesPuzzle::IsSolvable(const Tubes& start, int capacity, int nodeLimit) {
	set<string> visited;
	int nodes = 0;
	// 深度优先；用显式栈避免递归太深
	vector<Tubes> stack{ start };
	visited.insert(StateKey(start));
	while (!stack.empty()) {
		Tubes state = move(stack.back());
		stack.pop_back();
		if (IsSolved(state, capacity)) return true;
		if (++nodes > nodeLimit) return false;
		int n = static_cast<int>(state.size());
		for (int from = 0; from < n; from++) {
			if (state[from].empty()) continue;
			// 已经是整管单色的不用再往外倒
			bool pure = true;
			for (int c : state[from]) if (c != state[from][0]) pure = false;
			if (pure && static_cast<int>(state[from].size()) == capacity) continue;
			for (int to = 0; to < n; to++) {
				if (from == to) continue;
				// 倒进空管没意义：整管同色倒到另一个空管只是换位置
				if (state[to].empty() && pure) continue;
				Tubes next = state;
				if (Pour(next, from, to, capacity) == 0) continue;
				string key = StateKey(next);
				if (visited.insert(key).second) stack.push_back(move(next));
			}
		}
	}
	return false;
}

TubesPuzzle::Tubes TubesPuzzle::Generate(int colors, int capacity, int empty, PuzzleRng& rng, int* adjacentOut) {
	Tubes best;
	int bestAdjacent = 1 << 30;
	for (int attempt = 0; attempt < 60; attempt++) {
		vector<int> layers;
		for (int c = 0; c < colors; c++) for (int i = 0; i < capacity; i++) layers.push_back(c);
		rng.Shuffle(layers);
		Tubes tubes(colors + empty);
		for (int c = 0; c < colors; c++) tubes[c].assign(layers.begin() + c * capacity, layers.begin() + (c + 1) * capacity);

		auto adjacency = [&]() {
			int total = 0;
			for (const auto& tube : tubes) for (size_t i = 1; i < tube.size(); i++) if (tube[i] == tube[i - 1]) total++;
			return total;
		};
		// 爬山：随机换两层，只要相邻同色不增加就接受，把初始状态打散
		int current = adjacency();
		for (int iter = 0; iter < 4000 && current > 0; iter++) {
			int ta = rng.Int(colors), tb = rng.Int(colors);
			int ia = rng.Int(capacity), ib = rng.Int(capacity);
			if (ta == tb && ia == ib) continue;
			swap(tubes[ta][ia], tubes[tb][ib]);
			int next = adjacency();
			if (next <= current) current = next;
			else swap(tubes[ta][ia], tubes[tb][ib]);
		}
		if (IsSolved(tubes, capacity)) continue;
		if (!IsSolvable(tubes, capacity)) continue;
		if (current < bestAdjacent) { bestAdjacent = current; best = tubes; }
		if (current == 0) break;
	}
	if (best.empty()) {
		// 兜底：生成一个必可解的状态(已完成状态的逆向随机倒水)，不应走到这里
		best.assign(colors + empty, vector<int>());
		for (int c = 0; c < colors; c++) best[c].assign(capacity, c);
		for (int i = 0; i < 400; i++) {
			int a = rng.Int(colors + empty), b = rng.Int(colors + empty);
			if (a == b || best[a].empty() || static_cast<int>(best[b].size()) >= capacity) continue;
			best[b].push_back(best[a].back());
			best[a].pop_back();
		}
		bestAdjacent = 0;
	}
	if (adjacentOut) *adjacentOut = bestAdjacent;
	return best;
}

void TubesPuzzle::Init(Canvas* canvas, PostHandle* post) {
	rng.Seed(params);
	colors = clamp(params.GetInt("colors", 6), 2, 12);
	capacity = clamp(params.GetInt("capacity", 4), 2, 10);
	emptyTubes = clamp(params.GetInt("empty", 2), 1, 6);

	tubes.clear();
	string preset = params.GetString("preset", "");
	if (!preset.empty()) {
		// 按分号切，末尾的空串也算一个空管
		size_t from = 0;
		while (true) {
			size_t semi = preset.find(';', from);
			string item = preset.substr(from, semi == string::npos ? string::npos : semi - from);
			vector<int> tube;
			for (char c : item) if (c >= '0' && c <= '9') tube.push_back(c - '0');
			tubes.push_back(tube);
			if (semi == string::npos) break;
			from = semi + 1;
		}
		capacity = 1;   // 预设时容量由最长的管决定
		for (const auto& tube : tubes) capacity = max(capacity, static_cast<int>(tube.size()));
	}
	else {
		tubes = Generate(colors, capacity, emptyTubes, rng);
	}
	selected = -1;
	steps = 0;
	solved = IsSolved(tubes, capacity) && !tubes.empty();
	solvedTime = 0;
}

JsonValue* TubesPuzzle::Finish(bool completed) {
	result = JsonValue(DATA_OBJECT);
	result["steps"] = steps;
	result["completed"] = completed;
	return &result;
}

void TubesPuzzle::Layout(int w, int h) {
	unit = max(12, min(w, h) / 22);
	int top = unit * 3;
	int bottom = h - unit * 4;
	int n = static_cast<int>(tubes.size());
	int bestRows = 1, bestLayer = 0;
	for (int rows = 1; rows <= 4; rows++) {
		int perRow = (n + rows - 1) / rows;
		int tubeW = static_cast<int>((w - unit) / (perRow * 1.4));
		int availH = (bottom - top) / rows;
		int layer = min(static_cast<int>(tubeW * 0.9), static_cast<int>(availH * 0.85 / (capacity + 0.7)));
		if (layer > bestLayer) { bestLayer = layer; bestRows = rows; }
	}
	layerH = max(8, bestLayer);
	int perRow = (n + bestRows - 1) / bestRows;
	int tubeW = static_cast<int>(layerH * 1.1);
	int tubeH = static_cast<int>(layerH * (capacity + 0.5));
	int rowH = (bottom - top) / bestRows;
	tubeRects.assign(n, PuzzleRect());
	for (int i = 0; i < n; i++) {
		int row = i / perRow, col = i % perRow;
		int inRow = min(perRow, n - row * perRow);
		int slot = (w - unit) / perRow;
		int x = (w - slot * inRow) / 2 + col * slot + (slot - tubeW) / 2;
		int y = top + row * rowH + (rowH - tubeH) / 2 + layerH / 2;
		tubeRects[i] = MakeRect(x, y, tubeW, tubeH);
	}
	exitButton = MakeRect(w / 2 - unit * 4, h - unit * 3, unit * 8, unit * 2);
}

JsonValue* TubesPuzzle::Loop(Canvas* canvas, int ms, PostHandle* post) {
	PuzzleInput input;
	input.Poll(canvas);
	if (input.Key(KEY_ESCAPE)) return Finish(solved);

	int w = canvas->GetWidth(), h = canvas->GetHeight();
	double dt = ms / 1000.0;
	Layout(w, h);
	messageTime = max(0.0, messageTime - dt);

	if (solved) {
		solvedTime += dt;
		Render(canvas);
		if (solvedTime > 0.5 && input.anyPress) return Finish(true);
		return nullptr;
	}

	for (const MouseEvent& e : input.events) {
		if (!e.down) continue;
		if (exitButton.Hit(e.x, e.y)) return Finish(false);
		int hit = -1;
		for (int i = 0; i < static_cast<int>(tubeRects.size()); i++) {
			PuzzleRect r = tubeRects[i];
			r.x1 -= layerH / 3; r.x2 += layerH / 3; r.y1 -= layerH; r.y2 += layerH / 3; // 点击区域放宽一点(手指)
			if (r.Hit(e.x, e.y)) { hit = i; break; }
		}
		if (hit < 0) { selected = -1; continue; }
		if (selected < 0) {
			if (!tubes[hit].empty()) selected = hit;
		}
		else if (selected == hit) {
			selected = -1;
		}
		else if (Pour(tubes, selected, hit, capacity) > 0) {
			steps++;
			selected = -1;
			if (IsSolved(tubes, capacity)) { solved = true; solvedTime = 0; }
		}
		else {
			selected = tubes[hit].empty() ? -1 : hit; // 倒不过去：改选这一管
			messageTime = 0.8;
		}
	}

	Render(canvas);
	return nullptr;
}

void TubesPuzzle::Render(Canvas* canvas) {
	int w = canvas->GetWidth(), h = canvas->GetHeight();
	FillRect(canvas, MakeRect(0, 0, w - 1, h - 1), 36, 40, 58);
	DrawText(canvas, "Steps " + to_string(steps), unit / 2, unit / 2, unit, 255, 255, 255);
	if (messageTime > 0) DrawTextCentered(canvas, "Can't pour there", w / 2, unit * 2 + unit / 2, unit * 3 / 4, 255, 200, 90);

	for (int i = 0; i < static_cast<int>(tubes.size()); i++) {
		PuzzleRect r = tubeRects[i];
		if (i == selected) { r.y1 -= layerH / 2; r.y2 -= layerH / 2; }
		FillRect(canvas, r, 70, 78, 105, 0.5f);
		int inner = max(2, layerH / 12);
		const vector<int>& tube = tubes[i];
		for (int k = 0; k < static_cast<int>(tube.size()); k++) {
			const int* rgb = kPalette[tube[k] % 12];
			PuzzleRect layer;
			layer.x1 = r.x1 + inner + 1;
			layer.x2 = r.x2 - inner - 1;
			layer.y2 = r.y2 - inner - k * layerH;
			layer.y1 = layer.y2 - layerH + 2;
			FillRect(canvas, layer, rgb[0], rgb[1], rgb[2]);
		}
		StrokeRect(canvas, r, i == selected ? 255 : 220, i == selected ? 230 : 225, i == selected ? 80 : 240, i == selected ? 3 : 2);
	}

	DrawButton(canvas, exitButton, "Exit", unit, 120, 70, 70);

	if (solved) {
		DrawResultPanel(canvas, "Solved!", "Steps: " + to_string(steps), "Press any key or tap to exit");
	}
}
