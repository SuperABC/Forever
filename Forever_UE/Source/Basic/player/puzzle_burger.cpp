#include "puzzle_burger.h"


using namespace std;

int BurgerPuzzle::count = 0;

void BurgerPuzzle::IngColor(int ing, int& r, int& g, int& b) {
	static const int c[ING_COUNT][3] = {
		{ 225, 185, 110 }, { 140, 70, 50 }, { 235, 205, 150 }, { 110, 190, 80 }, { 220, 70, 60 },
		{ 190, 130, 200 }, { 190, 30, 30 }, { 250, 245, 215 }, { 240, 230, 120 }, { 215, 170, 20 } };
	r = c[ing][0]; g = c[ing][1]; b = c[ing][2];
}

const char* BurgerPuzzle::IngName(int ing) {
	static const char* n[ING_COUNT] = { "Bun", "Beef", "Chick", "Lettuce", "Tomato", "Onion", "Ketchup", "Mayo", "Salad", "Mustard" };
	return n[ing];
}

void BurgerPuzzle::SetParam(const string& param) {
	params.Parse(param);
}

void BurgerPuzzle::Init(Canvas* canvas, PostHandle* post) {
	rng.Seed(params);
	timeLimit = max(5.0, params.GetDouble("time", 60));
	cookTime = max(0.5, params.GetDouble("cook", 6));
	burnTime = max(0.5, params.GetDouble("burn", 5));
	orderMin = max(0.5, params.GetDouble("order_min", 6));
	orderMax = max(orderMin, params.GetDouble("order_max", 12));
	maxOrders = clamp(params.GetInt("max_orders", 4), 1, 6);

	scripted.clear();
	scriptedUsed = 0;
	string spec = params.GetString("orders", "");
	istringstream all(spec);
	string order;
	while (getline(all, order, ';')) {
		int mask = 0;
		istringstream parts(order);
		string name;
		while (getline(parts, name, '+')) {
			static const char* names[ING_COUNT] = { "bun", "beef", "chick", "lett", "tom", "onion", "ketchup", "mayo", "salad", "mustard" };
			for (int i = 0; i < ING_COUNT; i++) if (name == names[i]) mask |= 1 << i;
		}
		if (mask) scripted.push_back(mask);
	}

	static const char* iconNames[ING_COUNT] = { "bun", "beef", "chicken", "lettuce", "tomato", "onion", "ketchup", "mayo", "salad", "mustard" };
	for (int i = 0; i < ING_COUNT; i++) hasIcon[i] = LoadModImage(post, string("puzzle_burger_") + iconNames[i] + ".png", icons[i]);

	for (Cell& cell : cells) cell = Cell();
	for (int& p : plates) p = 0;
	orders.clear();
	drag = Drag();
	timeLeft = timeLimit;
	score = served = expired = 0;
	ended = false;
	endTime = 0;
	message.clear();
	nextOrder = 0;
}

JsonValue* BurgerPuzzle::Finish(bool completed) {
	result = JsonValue(DATA_OBJECT);
	result["score"] = score;
	result["served"] = served;
	result["expired"] = expired;
	result["completed"] = completed;
	return &result;
}

void BurgerPuzzle::SpawnOrder() {
	int mask = 0;
	if (scriptedUsed < scripted.size()) mask = scripted[scriptedUsed++];
	else {
		mask = 1 << BUN;
		mask |= 1 << (rng.Chance(0.5) ? BEEF : CHICKEN);
		for (int i = LETTUCE; i < ING_COUNT; i++) if (rng.Chance(0.5)) mask |= 1 << i;
	}
	int extras = 0;
	for (int i = 0; i < ING_COUNT; i++) if ((mask >> i) & 1) extras++;
	extras -= 2; // 面包+肉不算额外
	Order o;
	o.mask = mask;
	o.value = 10 + 3 * max(0, extras);
	o.total = o.remaining = 40 + 5 * max(0, extras);
	orders.push_back(o);
}

int BurgerPuzzle::SideState(double t) const {
	return t < cookTime ? 0 : (t < cookTime + burnTime ? 1 : 2);
}

bool BurgerPuzzle::Ready(const Cell& cell) const {
	if (cell.food < 0) return false;
	int a = SideState(cell.side[0]), b = SideState(cell.side[1]);
	if (cell.food == BUN) return a == 1;   // 面包只煎一面，不能翻面
	if (a == 2 || b == 2) return false;
	return a == 1 && b == 1;
}

void BurgerPuzzle::Layout(int width, int height) {
	w = width; h = height;
	unit = max(10, min(w, h) / 34);
	margin = max(6, w * 3 / 100);
	int innerW = w - 2 * margin;
	orderArea = MakeRect(margin, static_cast<int>(h * 0.06), innerW, static_cast<int>(h * 0.125));
	cellW = innerW / 4.0;
	cellH = h * 0.125;
	grillArea = MakeRect(margin, static_cast<int>(h * 0.2), innerW, static_cast<int>(cellH * 2 + h * 0.01));
	plateW = innerW / 4.0;
	plateArea = MakeRect(margin, static_cast<int>(h * 0.49), innerW, static_cast<int>(h * 0.135));
	serveRect = MakeRect(margin, static_cast<int>(h * 0.65), static_cast<int>(w * 0.6) - margin, static_cast<int>(h * 0.1));
	trashRect = MakeRect(static_cast<int>(w * 0.65), static_cast<int>(h * 0.65), w - margin - static_cast<int>(w * 0.65), static_cast<int>(h * 0.1));
	pantryRow1 = MakeRect(margin, static_cast<int>(h * 0.77), innerW, static_cast<int>(h * 0.1));
	pantryRow2 = MakeRect(margin, static_cast<int>(h * 0.88), innerW, static_cast<int>(h * 0.1));
}

PuzzleRect BurgerPuzzle::PantryRect(int ing) const {
	int pad = max(2, unit / 4);
	const PuzzleRect& row = ing < KETCHUP ? pantryRow1 : pantryRow2;
	int slots = ing < KETCHUP ? 6 : 4;
	int index = ing < KETCHUP ? ing : ing - KETCHUP;
	double slotW = static_cast<double>(row.Width()) / slots;
	int x = row.x1 + static_cast<int>(slotW * index);
	return PuzzleRect{ x + pad, row.y1 + pad, x + static_cast<int>(slotW) - pad, row.y2 - pad };
}

PuzzleRect BurgerPuzzle::CellRect(int index) const {
	int pad = max(2, unit / 4);
	int col = index % 4, row = index / 4;
	int x = grillArea.x1 + static_cast<int>(cellW * col);
	int y = grillArea.y1 + static_cast<int>((cellH + h * 0.01) * row);
	return PuzzleRect{ x + pad, y + pad, x + static_cast<int>(cellW) - pad, y + static_cast<int>(cellH) - pad };
}

PuzzleRect BurgerPuzzle::PlateRect(int index) const {
	int pad = max(2, unit / 4);
	int x = plateArea.x1 + static_cast<int>(plateW * index);
	return PuzzleRect{ x + pad, plateArea.y1 + pad, x + static_cast<int>(plateW) - pad, plateArea.y2 - pad };
}

void BurgerPuzzle::Say(const string& text) {
	message = text;
	messageTime = 1.4;
}

void BurgerPuzzle::Serve(int plate) {
	int mask = plates[plate];
	for (size_t i = 0; i < orders.size(); i++) {
		if (orders[i].mask != mask) continue;
		score += orders[i].value;
		served++;
		Say("+" + to_string(orders[i].value));
		orders.erase(orders.begin() + i);
		plates[plate] = 0;
		return;
	}
	plates[plate] = 0;
	Say("Wrong burger");
}

void BurgerPuzzle::Press(int x, int y) {
	drag = Drag();
	for (int i = 0; i < ING_COUNT; i++) {
		if (PantryRect(i).Hit(x, y)) { drag.src = Drag::PANTRY; drag.kind = i; drag.startX = x; drag.startY = y; return; }
	}
	for (int i = 0; i < 8; i++) {
		if (cells[i].food >= 0 && CellRect(i).Hit(x, y)) { drag.src = Drag::GRILL; drag.index = i; drag.kind = cells[i].food; drag.startX = x; drag.startY = y; return; }
	}
	for (int i = 0; i < 4; i++) {
		if (plates[i] && PlateRect(i).Hit(x, y)) { drag.src = Drag::PLATE; drag.index = i; drag.startX = x; drag.startY = y; return; }
	}
}

void BurgerPuzzle::Release(int x, int y) {
	if (drag.src == Drag::NONE) return;
	Drag d = drag;
	drag = Drag();
	int dx = x - d.startX, dy = y - d.startY;
	bool moved = dx * dx + dy * dy > unit * unit;

	int plate = -1, cell = -1;
	for (int i = 0; i < 4; i++) if (PlateRect(i).Hit(x, y)) plate = i;
	for (int i = 0; i < 8; i++) if (CellRect(i).Hit(x, y)) cell = i;
	bool onTrash = trashRect.Hit(x, y), onServe = serveRect.Hit(x, y);

	if (d.src == Drag::PANTRY) {
		if (!moved || onTrash) return;
		if (d.kind <= CHICKEN) {
			if (cell >= 0) {
				if (cells[cell].food >= 0) Say("Grill spot busy");
				else { cells[cell] = Cell(); cells[cell].food = d.kind; }
			}
			else if (plate >= 0) Say("Cook it first");
		}
		else if (plate >= 0) {
			if (plates[plate] & (1 << d.kind)) Say("Already on plate");
			else plates[plate] |= 1 << d.kind;
		}
	}
	else if (d.src == Drag::GRILL) {
		if (!moved) { if (cells[d.index].food != BUN) cells[d.index].down ^= 1; return; }
		if (onTrash) cells[d.index] = Cell();
		else if (plate >= 0) {
			if (!Ready(cells[d.index])) Say("Not cooked right");
			else if (plates[plate] & (1 << d.kind)) Say("Already on plate");
			else { plates[plate] |= 1 << d.kind; cells[d.index] = Cell(); }
		}
	}
	else if (d.src == Drag::PLATE) {
		if (onServe) Serve(d.index);
		else if (onTrash) plates[d.index] = 0;
	}
}

JsonValue* BurgerPuzzle::Loop(Canvas* canvas, int ms, PostHandle* post) {
	PuzzleInput input;
	input.Poll(canvas);
	if (input.Key(KEY_ESCAPE)) return Finish(ended);

	Layout(canvas->GetWidth(), canvas->GetHeight());
	double dt = ms / 1000.0;

	if (!ended) {
		for (const MouseEvent& e : input.events) {
			pointerX = e.x; pointerY = e.y;
			if (e.down) Press(e.x, e.y);
			else Release(e.x, e.y);
		}
		pointerX = input.mouse.x; pointerY = input.mouse.y;
		if (drag.src != Drag::NONE) {
			int dx = pointerX - drag.startX, dy = pointerY - drag.startY;
			if (dx * dx + dy * dy > unit * unit) drag.moved = true;
		}

		timeLeft -= dt;
		messageTime = max(0.0, messageTime - dt);
		for (Cell& cell : cells) if (cell.food >= 0) cell.side[cell.down] += dt;

		nextOrder -= dt;
		if (nextOrder <= 0 && static_cast<int>(orders.size()) < maxOrders) {
			SpawnOrder();
			nextOrder = rng.Real(orderMin, orderMax);
		}
		else if (nextOrder <= 0) nextOrder = 1.0; // 订单满了，稍后再看
		for (Order& o : orders) o.remaining -= dt;
		for (size_t i = 0; i < orders.size();) {
			if (orders[i].remaining <= 0) { orders.erase(orders.begin() + i); expired++; Say("Order expired"); }
			else i++;
		}
		if (timeLeft <= 0) { timeLeft = 0; ended = true; endTime = 0; drag = Drag(); }
	}
	else {
		endTime += dt;
		if (endTime > 0.5 && input.anyPress) return Finish(true);
	}

	Render(canvas);
	return nullptr;
}

void BurgerPuzzle::DrawTag(Canvas* canvas, int x, int y, int size, int ing) const {
	int r, g, b;
	IngColor(ing, r, g, b);
	PuzzleRect box = MakeRect(x, y, size, size);
	FillRect(canvas, box, r, g, b);
	StrokeRect(canvas, box, 40, 40, 40, 1);
	if (hasIcon[ing]) { canvas->SetAlpha(1.f); FillRect(canvas, box, 255, 250, 235); canvas->PutBitmap(icons[ing], x, y, size, size); }
}

void BurgerPuzzle::DrawIngredient(Canvas* canvas, const PuzzleRect& rect, int ing, bool small) const {
	int r, g, b;
	IngColor(ing, r, g, b);
	FillRect(canvas, rect, min(255, r + 25), min(255, g + 25), min(255, b + 25));
	StrokeRect(canvas, rect, 50, 40, 30, 2);
	int sz = min(rect.Width(), rect.Height()) - 6;
	if (hasIcon[ing]) { canvas->SetAlpha(1.f); canvas->PutBitmap(icons[ing], rect.CenterX() - sz / 2, rect.CenterY() - sz / 2, sz, sz); }
}

void BurgerPuzzle::Render(Canvas* canvas) {
	FillRect(canvas, MakeRect(0, 0, w, h), 245, 235, 215);
	FillRect(canvas, MakeRect(0, 0, w, static_cast<int>(h * 0.055)), 90, 60, 40);
	DrawText(canvas, "Score " + to_string(score), margin, unit / 3, unit * 3 / 2, 255, 240, 200);
	string timeText = to_string(static_cast<int>(ceil(timeLeft))) + "s";
	DrawText(canvas, timeText, w - margin - TextWidth(canvas, timeText, unit * 3 / 2), unit / 3, unit * 3 / 2, 255, 240, 200);

	// 订单
	int slotW = orderArea.Width() / maxOrders;
	for (int i = 0; i < maxOrders; i++) {
		PuzzleRect card = MakeRect(orderArea.x1 + slotW * i + 2, orderArea.y1, slotW - 4, orderArea.Height());
		FillRect(canvas, card, 255, 252, 240);
		StrokeRect(canvas, card, 120, 100, 80, 1);
		if (i >= static_cast<int>(orders.size())) continue;
		const Order& o = orders[i];
		int tag = max(10, min(card.Width() / 4 - 2, unit * 3 / 2));
		int n = 0;
		for (int ing = 0; ing < ING_COUNT; ing++) {
			if (!((o.mask >> ing) & 1)) continue;
			DrawTag(canvas, card.x1 + 3 + (n % 4) * (tag + 1), card.y1 + 3 + (n / 4) * (tag + 1), tag, ing);
			n++;
		}
		DrawText(canvas, "$" + to_string(o.value), card.x1 + 4, card.y1 + 3 + 3 * (tag + 1) - tag / 2, max(9, unit), 120, 60, 20);
		double frac = clamp(o.remaining / o.total, 0.0, 1.0);
		PuzzleRect bar = MakeRect(card.x1 + 3, card.y2 - unit / 2 - 3, card.Width() - 6, unit / 2);
		FillRect(canvas, bar, 215, 210, 200);
		PuzzleRect fill = bar;
		fill.x2 = bar.x1 + static_cast<int>(bar.Width() * frac);
		FillRect(canvas, fill, frac > 0.4 ? 90 : 220, frac > 0.4 ? 190 : 70, 80);
	}

	// 平板锅
	FillRect(canvas, MakeRect(grillArea.x1, grillArea.y1, grillArea.Width(), grillArea.Height()), 70, 70, 80);
	for (int i = 0; i < 8; i++) {
		PuzzleRect r = CellRect(i);
		FillRect(canvas, r, 100, 100, 110);
		const Cell& cell = cells[i];
		if (cell.food < 0 || (drag.src == Drag::GRILL && drag.moved && drag.index == i)) { StrokeRect(canvas, r, 60, 60, 70, 1); continue; }
		int cr, cg, cb;
		IngColor(cell.food, cr, cg, cb);
		bool isBun = cell.food == BUN;
		bool burnt = SideState(cell.side[0]) == 2 || (!isBun && SideState(cell.side[1]) == 2);
		int barH = max(5, unit * 6 / 10);
		PuzzleRect body{ r.x1 + 2, r.y1 + 2, r.x2 - 2, r.y2 - barH * 2 - 6 };
		FillRect(canvas, body, min(255, cr + 25), min(255, cg + 25), min(255, cb + 25));
		int isz = min(body.Width(), body.Height()) - 2;
		if (hasIcon[cell.food]) canvas->PutBitmap(icons[cell.food], body.x1 + 2, body.CenterY() - isz / 2, isz, isz);
		if (burnt) FillRect(canvas, body, 20, 10, 5, 0.6f);
		string status = burnt ? "BURNT" : (Ready(cell) ? "OK" : "RAW");
		DrawTextCentered(canvas, status, body.x1 + 2 + isz + (body.Width() - isz) / 2, body.CenterY(), max(9, unit * 8 / 10), burnt ? 255 : 30, burnt ? 120 : 30, burnt ? 80 : 30);
		for (int side = 0; side < (isBun ? 1 : 2); side++) {
			PuzzleRect bar{ r.x1 + 3, r.y2 - barH * (2 - side) - 3 - (1 - side) * 0, r.x2 - 3, r.y2 - barH * (1 - side) - 3 };
			double t = cell.side[side];
			int st = SideState(t);
			FillRect(canvas, bar, 50, 50, 55);
			PuzzleRect fill = bar;
			fill.x2 = bar.x1 + static_cast<int>(bar.Width() * clamp(t / (cookTime + burnTime), 0.0, 1.0));
			if (st == 0) FillRect(canvas, fill, 90, 150, 230);
			else if (st == 1) FillRect(canvas, fill, 80, 210, 90);
			else FillRect(canvas, fill, 150, 60, 40);
			int mark = bar.x1 + static_cast<int>(bar.Width() * cookTime / (cookTime + burnTime));
			FillRect(canvas, MakeRect(mark, bar.y1, 2, bar.Height()), 255, 255, 255);
			if (cell.down == side) StrokeRect(canvas, bar, 255, 255, 120, 2);
		}
	}

	// 盘位
	for (int i = 0; i < 4; i++) {
		PuzzleRect r = PlateRect(i);
		FillRect(canvas, r, 250, 250, 250);
		StrokeRect(canvas, r, 150, 150, 160, 2);
		if (drag.src == Drag::PLATE && drag.moved && drag.index == i) continue;
		int tag = max(10, min(r.Width() / 4 - 2, unit * 3 / 2));
		int n = 0;
		for (int ing = 0; ing < ING_COUNT; ing++) {
			if (!((plates[i] >> ing) & 1)) continue;
			DrawTag(canvas, r.x1 + 3 + (n % 4) * (tag + 1), r.y1 + 3 + (n / 4) * (tag + 1), tag, ing);
			n++;
		}
		if (!plates[i]) DrawTextCentered(canvas, "Plate " + to_string(i + 1), r.CenterX(), r.CenterY(), max(9, unit * 9 / 10), 170, 170, 180);
	}

	// 出餐区、垃圾桶
	FillRect(canvas, serveRect, 120, 190, 120);
	StrokeRect(canvas, serveRect, 50, 110, 50, 2);
	DrawTextCentered(canvas, "SERVE", serveRect.CenterX(), serveRect.CenterY(), unit * 3 / 2, 255, 255, 255);
	FillRect(canvas, trashRect, 130, 130, 140);
	StrokeRect(canvas, trashRect, 70, 70, 80, 2);
	DrawTextCentered(canvas, "TRASH", trashRect.CenterX(), trashRect.CenterY(), unit * 3 / 2, 255, 255, 255);

	// 食材区
	FillRect(canvas, MakeRect(0, pantryRow1.y1 - 2, w, h - pantryRow1.y1 + 2), 215, 195, 160);
	for (int i = 0; i < ING_COUNT; i++) DrawIngredient(canvas, PantryRect(i), i);

	// 拖拽中的东西跟着指针
	if (drag.src != Drag::NONE && drag.moved) {
		int sz = unit * 3;
		if (drag.src == Drag::PLATE) {
			PuzzleRect r = MakeRect(pointerX - sz, pointerY - sz / 2, sz * 2, sz);
			FillRect(canvas, r, 250, 250, 250, 0.9f);
			StrokeRect(canvas, r, 150, 150, 160, 2);
			int n = 0;
			for (int ing = 0; ing < ING_COUNT; ing++) {
				if (!((plates[drag.index] >> ing) & 1)) continue;
				DrawTag(canvas, r.x1 + 3 + (n % 5) * (unit + 3), r.y1 + 3 + (n / 5) * (unit + 3), unit + 2, ing);
				n++;
			}
		}
		else DrawIngredient(canvas, MakeRect(pointerX - sz, pointerY - sz / 2, sz * 2, sz), drag.kind);
	}

	if (messageTime > 0 && !message.empty()) {
		int mw = TextWidth(canvas, message, unit * 3 / 2) + unit * 2;
		PuzzleRect box = MakeRect(w / 2 - mw / 2, static_cast<int>(h * 0.465), mw, unit * 2);
		FillRect(canvas, box, 40, 40, 50, 0.85f);
		DrawTextCentered(canvas, message, box.CenterX(), box.CenterY(), unit * 3 / 2, 255, 240, 160);
	}

	if (ended) DrawResultPanel(canvas, "Shift over", "Score " + to_string(score) + "\nServed " + to_string(served) + "\nExpired " + to_string(expired), "Press any key to exit");
}
