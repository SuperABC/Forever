#include "puzzle_chase.h"


using namespace std;

int ChasePuzzle::count = 0;

void ChasePuzzle::SetParam(const string& param) {
	params.Parse(param);
}

// 文章规整：换行和连续空白合成一个空格，只保留可直接敲出的ASCII字符。
static string Normalize(const string& raw) {
	string out;
	bool space = true;
	for (unsigned char c : raw) {
		if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
			if (!space) out += ' ';
			space = true;
		}
		else if (c >= 33 && c < 127) {
			out += static_cast<char>(c);
			space = false;
		}
	}
	while (!out.empty() && out.back() == ' ') out.pop_back();
	return out;
}

void ChasePuzzle::Init(Canvas* canvas, PostHandle* post) {
	string name = params.GetString("article", "puzzle_chase_article.txt");
	string text;
	loadFailed = !LoadModTextFile(post, name, text);
	article = Normalize(text);
	if (article.empty()) loadFailed = true;
	double wpm = max(1.0, params.GetDouble("wpm", 40));
	thiefSpeed = wpm * 5.0 / 60.0;
	lead = max(1.0, params.GetDouble("lead", 15));
	penaltyMs = max(0.0, params.GetDouble("penalty", 300));
	typed = wrong = 0;
	thiefPos = lead;
	elapsed = stunned = flash = 0;
	state = RUNNING;
	wrapWidth = -1;
}

JsonValue* ChasePuzzle::Finish(bool completed) {
	result = JsonValue(DATA_OBJECT);
	int total = static_cast<int>(article.size());
	result["success"] = state == WON;
	result["progress"] = total > 0 ? static_cast<double>(typed) / total : 0.0;
	result["time"] = elapsed;
	result["accuracy"] = (typed + wrong) > 0 ? static_cast<double>(typed) / (typed + wrong) : 1.0;
	result["completed"] = completed;
	return &result;
}

// 按宽度把文章折行(在空格处断；单词太长就硬断)
void ChasePuzzle::Wrap(Canvas* canvas, int width, int fontSize) {
	lines.clear();
	canvas->SetFontSize(fontSize);
	int n = static_cast<int>(article.size());
	int begin = 0;
	while (begin < n) {
		int end = begin, lastSpace = -1;
		while (end < n) {
			if (article[end] == ' ') lastSpace = end;
			if (canvas->StringWidth(article.substr(begin, end - begin + 1)) > width && end > begin) break;
			end++;
		}
		if (end >= n) { lines.push_back({ begin, n }); break; }
		int cut = lastSpace > begin ? lastSpace + 1 : end; // 包含行尾空格，保证每个字符都属于某一行
		lines.push_back({ begin, cut });
		begin = cut;
	}
	wrapWidth = width;
	wrapFont = fontSize;
}

JsonValue* ChasePuzzle::Loop(Canvas* canvas, int ms, PostHandle* post) {
	PuzzleInput input;
	input.Poll(canvas);
	if (input.Key(KEY_ESCAPE)) return Finish(state != RUNNING);

	int w = canvas->GetWidth(), h = canvas->GetHeight();
	unit = max(12, min(w, h) / 22);
	double dt = ms / 1000.0;

	if (loadFailed) {
		FillRect(canvas, MakeRect(0, 0, w - 1, h - 1), 20, 20, 30);
		DrawTextCentered(canvas, "Article not found", w / 2, h / 2, unit, 255, 120, 120);
		if (input.anyPress) return Finish(false);
		return nullptr;
	}

	int total = static_cast<int>(article.size());
	if (state == RUNNING) {
		elapsed += dt;
		stunned = max(0.0, stunned - dt);
		flash = max(0.0, flash - dt);
		thiefPos += thiefSpeed * dt;
		for (int key : input.keys) {
			if (state != RUNNING || stunned > 0) continue;
			if (key < 32 || key > 126) continue; // 功能键不算
			char expect = article[typed];
			bool ok = key == expect || (isalpha(static_cast<unsigned char>(expect)) && tolower(key) == tolower(expect));
			if (ok) typed++;
			else { wrong++; stunned = penaltyMs / 1000.0; flash = 0.25; }
			if (typed >= thiefPos || typed >= total) { state = WON; endTime = 0; }
		}
		if (state == RUNNING && thiefPos >= total) { state = LOST; endTime = 0; }
	}
	else {
		endTime += dt;
		if (endTime > 0.5 && input.anyPress) return Finish(true);
	}

	Render(canvas);
	return nullptr;
}

void ChasePuzzle::DrawRunner(Canvas* canvas, int cx, int groundY, int size, bool thief, double phase) const {
	int bob = static_cast<int>(sin(phase * 14.0) * size / 10);
	int bodyW = size, bodyH = size;
	PuzzleRect body{ cx - bodyW / 2, groundY - bodyH + bob, cx + bodyW / 2, groundY + bob };
	if (thief) {
		FillRect(canvas, body, 235, 235, 235);
		for (int i = 1; i < 4; i++) FillRect(canvas, PuzzleRect{ body.x1, body.y1 + i * bodyH / 4 - 2, body.x2, body.y1 + i * bodyH / 4 + 2 }, 30, 30, 30);
		FillRect(canvas, PuzzleRect{ body.x1, body.y1 + bodyH / 5, body.x2, body.y1 + bodyH * 2 / 5 }, 20, 20, 20); // 眼罩
		FillRect(canvas, PuzzleRect{ body.x2 - bodyW / 6, body.y1 + bodyH / 4, body.x2 - bodyW / 12, body.y1 + bodyH * 3 / 8 }, 255, 255, 255);
		FillRect(canvas, PuzzleRect{ body.x1 + bodyW / 6, body.y1 - bodyH / 4, body.x2 - bodyW / 6, body.y1 }, 200, 50, 50); // 帽子
		FillCircle(canvas, body.x2 + size / 3, body.y1 + bodyH / 2, size / 3, 150, 110, 60); // 赃物袋
	}
	else {
		FillRect(canvas, body, 50, 90, 210);
		FillRect(canvas, PuzzleRect{ body.x1 - 2, body.y1 - bodyH / 4, body.x2 + 2, body.y1 }, 30, 50, 140); // 警帽
		FillCircle(canvas, body.CenterX(), body.y1 - bodyH / 8, bodyH / 12, 250, 210, 60);
		FillRect(canvas, PuzzleRect{ body.x1 + bodyW / 5, body.y1 + bodyH / 4, body.x1 + bodyW / 3, body.y1 + bodyH * 2 / 5 }, 255, 255, 255);
		FillRect(canvas, PuzzleRect{ body.x2 - bodyW / 3, body.y1 + bodyH / 4, body.x2 - bodyW / 5, body.y1 + bodyH * 2 / 5 }, 255, 255, 255);
		FillRect(canvas, PuzzleRect{ body.x1 + bodyW / 5, body.y1 + bodyH * 3 / 5, body.x2 - bodyW / 5, body.y1 + bodyH * 7 / 10 }, 250, 210, 60); // 警徽带
	}
	StrokeRect(canvas, body, 20, 20, 30, 2);
}

void ChasePuzzle::Render(Canvas* canvas) {
	int w = canvas->GetWidth(), h = canvas->GetHeight();
	FillRect(canvas, MakeRect(0, 0, w - 1, h - 1), 235, 242, 250);

	// 跑道
	int margin = unit * 2;
	int trackTop = h * 8 / 100, trackBottom = h * 38 / 100;
	FillRect(canvas, MakeRect(0, trackTop, w - 1, trackBottom - trackTop), 150, 190, 235);
	int groundY = trackBottom - unit;
	FillRect(canvas, MakeRect(0, groundY, w - 1, trackBottom - groundY), 110, 110, 125);

	int total = max(1, static_cast<int>(article.size()));
	// 镜头跟着警察：警察固定在左侧，小偷按领先的字符数拉开距离(跑出画面就贴在右边缘)
	double pxPerChar = unit * 0.9;
	int size = unit * 2;
	int policeX = margin;
	int thiefX = min(w - margin, policeX + static_cast<int>((thiefPos - typed) * pxPerChar));
	for (int x = -static_cast<int>(fmod(typed * pxPerChar, unit * 3)); x < w; x += unit * 3)
		FillRect(canvas, MakeRect(x, groundY + unit / 2 - 1, unit, 3), 245, 245, 245);
	DrawRunner(canvas, policeX, groundY, size, false, typed * 0.5);
	DrawRunner(canvas, thiefX, groundY, size, true, elapsed + 0.7);

	// 进度/间距
	double gap = thiefPos - typed;
	DrawText(canvas, "Gap " + to_string(static_cast<int>(max(0.0, gap))) + " chars", unit / 2, unit / 3, unit, 20, 30, 60);
	string timeText = FormatSeconds(elapsed) + "s";
	DrawText(canvas, timeText, w - TextWidth(canvas, timeText, unit) - unit / 2, unit / 3, unit, 20, 30, 60);

	// 文章区域
	PuzzleRect textArea = MakeRect(unit, h * 42 / 100, w - unit * 2, h * 54 / 100);
	FillRect(canvas, textArea, 255, 255, 255);
	StrokeRect(canvas, textArea, flash > 0 ? 230 : 90, flash > 0 ? 60 : 100, flash > 0 ? 60 : 130, flash > 0 ? 4 : 2);
	int font = max(14, unit * 6 / 5);
	if (wrapWidth != textArea.Width() - unit || wrapFont != font) Wrap(canvas, textArea.Width() - unit, font);
	canvas->SetFontSize(font);
	int lineH = canvas->GetFontHeight() + unit / 3;
	int visible = max(1, (textArea.Height() - unit) / lineH);
	int curLine = 0;
	for (int i = 0; i < static_cast<int>(lines.size()); i++) if (typed >= lines[i].begin && typed < lines[i].end) curLine = i;
	if (typed >= static_cast<int>(article.size())) curLine = static_cast<int>(lines.size()) - 1;
	int firstLine = clamp(curLine - 1, 0, max(0, static_cast<int>(lines.size()) - visible));
	for (int row = 0; row < visible && firstLine + row < static_cast<int>(lines.size()); row++) {
		const Line& line = lines[firstLine + row];
		int x = textArea.x1 + unit / 2, y = textArea.y1 + unit / 2 + row * lineH;
		string full = article.substr(line.begin, line.end - line.begin);
		int doneChars = clamp(typed - line.begin, 0, line.end - line.begin);
		if (typed >= line.begin && typed < line.end) {
			// 当前字符的光标块
			int px = x + (doneChars > 0 ? TextWidth(canvas, full.substr(0, doneChars), font) : 0);
			string ch = full.substr(doneChars, 1);
			int cw = max(TextWidth(canvas, ch, font), font / 2);
			FillRect(canvas, MakeRect(px, y, cw, lineH - unit / 4), stunned > 0 ? 255 : 255, stunned > 0 ? 170 : 235, stunned > 0 ? 170 : 120);
		}
		DrawText(canvas, full, x, y, font, 40, 40, 60);
		if (doneChars > 0) DrawText(canvas, full.substr(0, doneChars), x, y, font, 40, 170, 90); // 已敲对的盖成绿色
	}
	// 进度条
	PuzzleRect bar = MakeRect(unit, h - unit * 2, w - unit * 2, unit / 2);
	FillRect(canvas, bar, 200, 205, 220);
	PuzzleRect fill = bar;
	fill.x2 = bar.x1 + static_cast<int>(bar.Width() * static_cast<double>(typed) / total);
	FillRect(canvas, fill, 60, 160, 90);

	if (state != RUNNING) {
		string lines2 = "Typed " + to_string(typed) + "/" + to_string(total) + "\nTime " + FormatSeconds(elapsed) + " s\nAccuracy " + to_string(static_cast<int>(100.0 * typed / max(1, typed + wrong))) + "%";
		DrawResultPanel(canvas, state == WON ? "Caught the thief!" : "The thief got away", lines2, "Press any key to exit");
	}
}
