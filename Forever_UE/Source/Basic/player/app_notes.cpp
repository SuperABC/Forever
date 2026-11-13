#include "app_notes.h"

#include "player/canvas.h"
#include "common/utility.h"

#include <algorithm>
#include <cmath>


using namespace std;

int NotesApp::count = 0;

NotesApp::NotesApp() : id(count++) {

}

const char* NotesApp::GetName() {
	name = "Notes" + to_string(id);
	return name.data();
}

void NotesApp::Init(Canvas* canvas, PostHandle* post) {
	notes = {
		{ "Welcome", "This is the Notes test app.\nTap a row to open it,\nthen use Back to return." },
		{ "Todo", "1. Finish phone shell\n2. Test task switching\n3. Ship it" },
		{ "Shortcut", "Backspace = Back\nSpace = Home\nTab = Tasks" },
	};
	screen = Screen::List;
	selection = 0;
	rowHeight = max(20, canvas->GetHeight() / 12);
}

int NotesApp::HitRow(int x, int y) const {
	if (y < rowHeight) return -1; // 顶部留一行给标题
	int row = y / rowHeight - 1;
	if (row < 0 || row >= static_cast<int>(notes.size())) return -1;
	return row;
}

void NotesApp::Loop(Canvas* canvas, int ms, PostHandle* post) {
	while (canvas->HasPendingKey()) {
		int raw = canvas->PopKey();
		bool released = (raw & KEY_RELEASED_FLAG) != 0;
		int key = raw & ~KEY_RELEASED_FLAG;
		if (released) continue;

		if (screen == Screen::List) {
			if (key == KEY_UP) { if (selection > 0) selection--; }
			else if (key == KEY_DOWN) { if (selection + 1 < static_cast<int>(notes.size())) selection++; }
			else if (key == KEY_ENTER) { screen = Screen::Detail; }
		}
	}

	while (canvas->HasPendingMouseEvent()) {
		MouseEvent event = canvas->PopMouseEvent();
		if (!event.down) continue;

		if (screen == Screen::List) {
			int row = HitRow(event.x, event.y);
			if (row >= 0) { selection = row; screen = Screen::Detail; }
		}
	}

	if (screen == Screen::List) RenderList(canvas);
	else RenderDetail(canvas);
}

void NotesApp::Back(Canvas* canvas, PostHandle* post) {
	if (screen == Screen::Detail) screen = Screen::List;
	// 已经在列表：没有更上一级可退，什么都不做(交给Phone决定是否要离开这个App)
}

void NotesApp::Refresh(Canvas* canvas, PostHandle* post) {
	// 停在离开前的screen/selection，不重置——从后台切回来时应该看到跟切走前一样的页面。
	if (screen == Screen::List) RenderList(canvas);
	else RenderDetail(canvas);
}

void NotesApp::RenderList(Canvas* canvas) const {
	canvas->SetColor(30, 30, 45);
	canvas->ClearScreen();

	// 标题字号(App名字这一档)统一用height/17，比App内其它文字都大；Y留一点顶部边距。
	canvas->SetFontSize(max(26, canvas->GetHeight() / 17));
	canvas->SetColor(255, 255, 255);
	canvas->PutString("NOTES", canvas->GetWidth() / 40, canvas->GetHeight() / 60);

	for (size_t i = 0; i < notes.size(); i++) {
		int y = rowHeight * (static_cast<int>(i) + 1);
		bool selected = (static_cast<int>(i) == selection);

		canvas->SetColor(selected ? 70 : 40, selected ? 70 : 40, selected ? 100 : 55);
		canvas->PutRect(0, y, canvas->GetWidth() - 1, y + rowHeight - 2, true);

		// 内容字号比顶部"NOTES"这一档小一档(height/23 < height/17)。
		canvas->SetFontSize(max(21, canvas->GetHeight() / 23));
		canvas->SetColor(255, 255, 255);
		// 垂直居中：PutString的y是文字顶部，用GetFontHeight算偏移，字号变了也不会偏。
		int textTop = y + (rowHeight - canvas->GetFontHeight()) / 2;
		canvas->PutString(notes[i].title, canvas->GetWidth() / 30, textTop);
	}
}

void NotesApp::RenderDetail(Canvas* canvas) const {
	canvas->SetColor(20, 20, 30);
	canvas->ClearScreen();

	if (selection < 0 || selection >= static_cast<int>(notes.size())) return;
	const Note& note = notes[selection];

	// 详情页标题/正文属于App内容，字号用内容这一档(比顶部"NOTES"小)。
	canvas->SetFontSize(max(21, canvas->GetHeight() / 23));
	canvas->SetColor(255, 255, 255);
	canvas->PutString(note.title, canvas->GetWidth() / 40, canvas->GetHeight() / 60);

	canvas->PutString(note.body, canvas->GetWidth() / 40, canvas->GetHeight() / 10);

	canvas->SetColor(180, 180, 180);
	canvas->PutString("BACK BUTTON / BACKSPACE TO RETURN", canvas->GetWidth() / 40, canvas->GetHeight() - canvas->GetHeight() / 15);
}
