#include "phone.h"

#include "common/registry.h"
#include "common/json.h"

#include <algorithm>


using namespace std;

Phone::Phone() {

}

Phone::~Phone() {
	for (AppEntry& entry : entries) {
		delete entry.app;
	}
}

void Phone::Init(int inWidth, int inHeight) {
	width = inWidth;
	height = inHeight;
	RecomputeLayout(width, height);
	BuildAppList();
}

void Phone::Resize(int inWidth, int inHeight) {
	if (inWidth == width && inHeight == height) return;
	width = inWidth;
	height = inHeight;
	RecomputeLayout(width, height);
}

void Phone::RecomputeLayout(int w, int h) {
	cellWidth = w / gridCols;
	gridStartY = h * 78 / 1000;
	cellHeight = h * 16 / 100;
	iconSize = max(10, min(cellWidth, cellHeight) * 2 / 3);

	taskCellWidth = w / taskCols;
	taskCellHeight = h * 3 / 10;
	thumbWidth = taskCellWidth / 2;
	thumbHeight = taskCellHeight * 8 / 10;

	barHeight = max(20, h * 625 / 10000);
	barY = h - barHeight;
}

void Phone::BuildAppList() {
	appTypes.clear();
	for (const string& id : Registry::Get().GetAppFactory().GetRegisteredIds()) {
		if (id == "empty") continue; // 每个concept都有的占位id，不当真实App显示在桌面上
		appTypes.push_back(id);
	}
}

int Phone::FindEntry(const string& type) const {
	for (size_t i = 0; i < entries.size(); i++) {
		if (entries[i].type == type) return static_cast<int>(i);
	}
	return -1;
}

void Phone::LaunchApp(Canvas* canvas, const string& type, PostHandle* post) {
	int index = FindEntry(type);
	if (index < 0) {
		AppEntry entry;
		entry.type = type;
		entry.app = new App(&Registry::Get().GetAppFactory(), type);
		entries.push_back(entry);
		index = static_cast<int>(entries.size()) - 1;
	}
	OpenApp(canvas, index, post);
}

void Phone::OpenApp(Canvas* canvas, int entryIndex, PostHandle* post) {
	AppEntry& entry = entries[entryIndex];
	if (!entry.initialized) {
		entry.app->Init(canvas, post);
		entry.initialized = true;
	}
	else {
		entry.app->Refresh(canvas, post);
	}
	currentAppType = entry.type;
	state = State::InApp;
}

void Phone::SaveSnapshot(Canvas* canvas, int entryIndex) {
	AppEntry& entry = entries[entryIndex];
	const uint8_t* data = canvas->GetData();
	int size = canvas->GetDataSize();
	entry.snapshot.assign(data, data + size);
	entry.snapshotWidth = canvas->GetWidth();
	entry.snapshotHeight = canvas->GetHeight();
}

void Phone::CloseEntry(int entryIndex) {
	delete entries[entryIndex].app;
	if (entries[entryIndex].type == currentAppType) currentAppType.clear();
	entries.erase(entries.begin() + entryIndex);
	if (taskSelection >= static_cast<int>(entries.size())) {
		taskSelection = max(0, static_cast<int>(entries.size()) - 1);
	}
}

void Phone::GoHome(Canvas* canvas, PostHandle* post) {
	if (state == State::InApp) {
		int index = FindEntry(currentAppType);
		if (index >= 0) SaveSnapshot(canvas, index);
	}
	state = State::Home;
}

void Phone::GoTaskList(Canvas* canvas, PostHandle* post) {
	if (state == State::InApp) {
		int index = FindEntry(currentAppType);
		if (index >= 0) SaveSnapshot(canvas, index);
	}
	state = State::TaskList;
}

int Phone::HitHomeGrid(int x, int y) const {
	if (y < gridStartY || cellWidth <= 0 || cellHeight <= 0) return -1;
	int col = x / cellWidth;
	int row = (y - gridStartY) / cellHeight;
	if (col < 0 || col >= gridCols || row < 0) return -1;
	int index = row * gridCols + col;
	if (index < 0 || index >= static_cast<int>(appTypes.size())) return -1;
	return index;
}

int Phone::HitTaskGrid(int x, int y) const {
	if (y < gridStartY || taskCellWidth <= 0 || taskCellHeight <= 0) return -1;
	int col = x / taskCellWidth;
	int row = (y - gridStartY) / taskCellHeight;
	if (col < 0 || col >= taskCols || row < 0) return -1;
	int index = row * taskCols + col;
	if (index < 0 || index >= static_cast<int>(entries.size())) return -1;
	return index;
}

bool Phone::HitTaskCloseButton(int entryIndex, int x, int y) const {
	int col = entryIndex % taskCols, row = entryIndex / taskCols;
	int cellX = col * taskCellWidth, cellY = gridStartY + row * taskCellHeight;
	int thumbX = cellX + (taskCellWidth - thumbWidth) / 2, thumbY = cellY;
	int closeSize = max(12, height / 25);
	int closeX = thumbX + thumbWidth - closeSize, closeY = thumbY;
	return x >= closeX && x <= closeX + closeSize && y >= closeY && y <= closeY + closeSize;
}

Phone::BottomButton Phone::HitBottomBar(int x, int y) const {
	if (y < barY) return BottomButton::None;
	if (x < width / 3) return BottomButton::Back;
	if (x < width * 2 / 3) return BottomButton::Home;
	return BottomButton::Tasks;
}

void Phone::HandleHomeInput(Canvas* canvas, PostHandle* post) {
	while (canvas->HasPendingKey()) {
		int raw = canvas->PopKey();
		bool released = (raw & KEY_RELEASED_FLAG) != 0;
		int key = raw & ~KEY_RELEASED_FLAG;
		if (released) continue;

		// ESC/Tab不依赖appTypes是否为空(哪怕一个App都没注册，也得能关手机/切任务列表)，
		// 其余几个跟"选中哪个图标"相关的操作才需要appTypes非空。
		if (key == KEY_ESCAPE) { pendingClose = true; return; }
		if (key == KEY_TAB) { state = State::TaskList; return; }
		if (appTypes.empty()) continue;

		int count = static_cast<int>(appTypes.size());
		int col = homeSelection % gridCols, row = homeSelection / gridCols;

		if (key == KEY_LEFT) { if (col > 0) homeSelection--; }
		else if (key == KEY_RIGHT) { if (col < gridCols - 1 && homeSelection + 1 < count) homeSelection++; }
		else if (key == KEY_UP) { if (row > 0) homeSelection -= gridCols; }
		else if (key == KEY_DOWN) { if (homeSelection + gridCols < count) homeSelection += gridCols; }
		else if (key == KEY_ENTER) { LaunchApp(canvas, appTypes[homeSelection], post); return; }
	}

	while (canvas->HasPendingMouseEvent()) {
		MouseEvent event = canvas->PopMouseEvent();
		if (!event.down) continue;

		if (event.y >= barY) {
			if (HitBottomBar(event.x, event.y) == BottomButton::Tasks) { state = State::TaskList; return; }
			continue; // 桌面态下Back/Home按钮没有意义(已经在主屏)，忽略
		}

		int hitIndex = HitHomeGrid(event.x, event.y);
		if (hitIndex >= 0) {
			homeSelection = hitIndex;
			LaunchApp(canvas, appTypes[hitIndex], post);
			return;
		}
	}
}

void Phone::HandleInAppInput(Canvas* canvas, PostHandle* post) {
	int index = FindEntry(currentAppType);

	// 先把这一帧全部按键弹出来,只处理Phone自己关心的几个,其余原样PushKey还回去——不能
	// 一边PopKey一边立刻PushKey再继续同一个while循环,那样会在这个循环里死循环。
	vector<int> pendingKeys;
	while (canvas->HasPendingKey()) pendingKeys.push_back(canvas->PopKey());
	for (int raw : pendingKeys) {
		bool released = (raw & KEY_RELEASED_FLAG) != 0;
		int key = raw & ~KEY_RELEASED_FLAG;

		if (!released && key == KEY_ESCAPE) { GoHome(canvas, post); return; }
		if (!released && key == KEY_TAB) { GoTaskList(canvas, post); return; }
		if (!released && key == ' ') { GoHome(canvas, post); return; }
		if (!released && key == KEY_BACKSPACE) {
			if (index >= 0) entries[index].app->Back(canvas, post);
			continue; // App内部导航,不透传给App::Loop的按键队列
		}
		canvas->PushKey(raw);
	}

	vector<MouseEvent> pendingMouse;
	while (canvas->HasPendingMouseEvent()) pendingMouse.push_back(canvas->PopMouseEvent());
	for (const MouseEvent& event : pendingMouse) {
		if (event.down && event.y >= barY) {
			BottomButton hit = HitBottomBar(event.x, event.y);
			if (hit == BottomButton::Back) { if (index >= 0) entries[index].app->Back(canvas, post); }
			else if (hit == BottomButton::Home) { GoHome(canvas, post); return; }
			else if (hit == BottomButton::Tasks) { GoTaskList(canvas, post); return; }
			continue; // 落在工具栏范围但没命中按钮：丢弃，不透传给App
		}
		canvas->PushMouseButton(event.button, event.down);
	}
}

void Phone::HandleTaskListInput(Canvas* canvas, PostHandle* post) {
	while (canvas->HasPendingKey()) {
		int raw = canvas->PopKey();
		bool released = (raw & KEY_RELEASED_FLAG) != 0;
		int key = raw & ~KEY_RELEASED_FLAG;
		if (released) continue;

		if (entries.empty()) {
			if (key == ' ' || key == KEY_ESCAPE || key == KEY_TAB) state = State::Home;
			continue;
		}

		int count = static_cast<int>(entries.size());
		int col = taskSelection % taskCols, row = taskSelection / taskCols;

		if (key == KEY_LEFT) { if (col > 0) taskSelection--; }
		else if (key == KEY_RIGHT) { if (col < taskCols - 1 && taskSelection + 1 < count) taskSelection++; }
		else if (key == KEY_UP) { if (row > 0) taskSelection -= taskCols; }
		else if (key == KEY_DOWN) { if (taskSelection + taskCols < count) taskSelection += taskCols; }
		else if (key == KEY_ENTER) { OpenApp(canvas, taskSelection, post); return; }
		else if (key == KEY_BACKSPACE) { CloseEntry(taskSelection); return; }
		else if (key == ' ' || key == KEY_ESCAPE || key == KEY_TAB) { state = State::Home; return; }
	}

	while (canvas->HasPendingMouseEvent()) {
		MouseEvent event = canvas->PopMouseEvent();
		if (!event.down) continue;

		if (event.y >= barY) {
			BottomButton hit = HitBottomBar(event.x, event.y);
			if (hit == BottomButton::Home || hit == BottomButton::Tasks) { state = State::Home; return; }
			continue;
		}

		int hitIndex = HitTaskGrid(event.x, event.y);
		if (hitIndex < 0) continue;

		if (HitTaskCloseButton(hitIndex, event.x, event.y)) { CloseEntry(hitIndex); return; }

		taskSelection = hitIndex;
		OpenApp(canvas, hitIndex, post);
		return;
	}
}

void Phone::RenderHome(Canvas* canvas, PostHandle* post) {
	canvas->SetColor(25, 25, 35);
	canvas->ClearScreen();

	canvas->SetFontSize(max(26, height / 17));
	canvas->SetColor(255, 255, 255);
	string title = "HOME";
	canvas->PutString(title, width / 2 - canvas->StringWidth(title) / 2, height / 60);

	if (post) {
		JsonValue request(DATA_OBJECT);
		request["post"] = "game time";
		post->Post(request);
		const JsonValue& result = post->GetResult();
		if (result["result"].AsString() == "success") {
			canvas->PutString(result["date"].AsString(), width / 40, height / 60);
			string time = result["time"].AsString();
			canvas->PutString(time, width - canvas->StringWidth(time) - width / 40, height / 60);
		}
	}

	for (size_t i = 0; i < appTypes.size(); i++) {
		int index = static_cast<int>(i);
		int col = index % gridCols, row = index / gridCols;
		int cellX = col * cellWidth, cellY = gridStartY + row * cellHeight;
		int iconX = cellX + (cellWidth - iconSize) / 2, iconY = cellY + height / 100;

		bool selected = (index == homeSelection);
		canvas->SetColor(selected ? 90 : 60, selected ? 140 : 90, selected ? 220 : 140);
		canvas->PutRect(iconX, iconY, iconX + iconSize, iconY + iconSize, true);

		canvas->SetFontSize(max(24, height / 21));
		canvas->SetColor(255, 255, 255);
		const string& label = appTypes[i];
		canvas->PutString(label, cellX + (cellWidth - canvas->StringWidth(label)) / 2, iconY + iconSize + height / 200);
	}
}

void Phone::RenderTaskList(Canvas* canvas) {
	canvas->SetColor(15, 15, 20);
	canvas->ClearScreen();

	canvas->SetFontSize(max(26, height / 17));
	canvas->SetColor(255, 255, 255);
	string title = "TASKS";
	canvas->PutString(title, width / 2 - canvas->StringWidth(title) / 2, height / 60);

	for (size_t i = 0; i < entries.size(); i++) {
		int index = static_cast<int>(i);
		int col = index % taskCols, row = index / taskCols;
		int cellX = col * taskCellWidth, cellY = gridStartY + row * taskCellHeight;
		int thumbX = cellX + (taskCellWidth - thumbWidth) / 2, thumbY = cellY;

		bool selected = (index == taskSelection);
		canvas->SetColor(selected ? 200 : 90, selected ? 200 : 90, selected ? 60 : 90);
		canvas->PutRect(thumbX - 2, thumbY - 2, thumbX + thumbWidth + 2, thumbY + thumbHeight + 2, false);

		const AppEntry& entry = entries[i];
		if (!entry.snapshot.empty() && entry.snapshotWidth > 0 && entry.snapshotHeight > 0) {
			canvas->PutRawImage(entry.snapshot.data(), entry.snapshotWidth, entry.snapshotHeight,
				thumbX, thumbY, thumbWidth, thumbHeight);
		}
		else {
			canvas->SetColor(50, 50, 50);
			canvas->PutRect(thumbX, thumbY, thumbX + thumbWidth, thumbY + thumbHeight, true);
		}

		canvas->SetFontSize(max(24, height / 21));
		canvas->SetColor(255, 255, 255);
		canvas->PutString(entry.type, thumbX, thumbY + thumbHeight + height / 30);

		canvas->SetColor(220, 60, 60);
		int closeSize = max(12, height / 25);
		canvas->PutString("X", thumbX + thumbWidth - closeSize, thumbY);
	}
}

void Phone::RenderBottomBar(Canvas* canvas) {
	canvas->SetColor(10, 10, 15);
	canvas->PutRect(0, barY, width - 1, height - 1, true);

	// 三个按钮用矢量图形画(PutLine/PutCircle/PutRect)，不再依赖字体——一开始用文字符号
	// "<"/"O"/"[]"是因为当时Canvas还是内置点阵字体，覆盖不了"<"/"["/"]"这几个符号；现在换成
	// GDI字体理论上能画这些符号了，但直接画图形比找字体符号更可靠、也更像真的手机图标。
	canvas->SetColor(230, 230, 230);
	int iconSize = max(12, barHeight * 2 / 5);
	int centerY = barY + barHeight / 2;

	// Back：一个左箭头，两条线拼成"<"
	int backX = width / 6;
	canvas->PutLine(backX + iconSize / 2, centerY - iconSize / 2, backX - iconSize / 2, centerY);
	canvas->PutLine(backX - iconSize / 2, centerY, backX + iconSize / 2, centerY + iconSize / 2);

	// Home：一个空心圆
	int homeX = width / 2;
	canvas->PutCircle(homeX, centerY, iconSize / 2, false);

	// Tasks：两个错开的空心方框(安卓"最近任务"图标的经典画法)
	int tasksX = width * 5 / 6;
	int square = iconSize * 3 / 4;
	int offset = iconSize / 4;
	canvas->PutRect(tasksX - square / 2 - offset, centerY - square / 2 - offset,
		tasksX + square / 2 - offset, centerY + square / 2 - offset, false);
	canvas->PutRect(tasksX - square / 2 + offset, centerY - square / 2 + offset,
		tasksX + square / 2 + offset, centerY + square / 2 + offset, false);
}

int Phone::Loop(Canvas* canvas, int ms, PostHandle* post) {
	pendingClose = false;

	switch (state) {
	case State::Home: HandleHomeInput(canvas, post); break;
	case State::InApp: HandleInAppInput(canvas, post); break;
	case State::TaskList: HandleTaskListInput(canvas, post); break;
	}

	switch (state) {
	case State::Home: RenderHome(canvas, post); break;
	case State::InApp: {
		int index = FindEntry(currentAppType);
		if (index >= 0) entries[index].app->Loop(canvas, ms, post);
		else state = State::Home;
		break;
	}
	case State::TaskList: RenderTaskList(canvas); break;
	}
	RenderBottomBar(canvas);

	return pendingClose ? 1 : 0;
}
