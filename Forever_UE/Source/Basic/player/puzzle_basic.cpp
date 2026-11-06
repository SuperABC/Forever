#include "puzzle_basic.h"

#include "common/utility.h"

#include <algorithm>
#include <cstring>


using namespace std;

const TetrisPuzzle::PieceShape TetrisPuzzle::kPieces[kPieceTypeCount] = {
	{ 4, { {0,1},{1,1},{2,1},{3,1} }, 0, 255, 255 },   // I 青
	{ 2, { {0,0},{1,0},{0,1},{1,1} }, 255, 255, 0 },   // O 黄
	{ 3, { {1,0},{0,1},{1,1},{2,1} }, 160, 0, 240 },   // T 紫
	{ 3, { {1,0},{2,0},{0,1},{1,1} }, 0, 255, 0 },     // S 绿
	{ 3, { {0,0},{1,0},{1,1},{2,1} }, 255, 0, 0 },     // Z 红
	{ 3, { {0,0},{0,1},{1,1},{2,1} }, 0, 0, 255 },     // J 蓝
	{ 3, { {2,0},{0,1},{1,1},{2,1} }, 255, 165, 0 },   // L 橙
};

int TetrisPuzzle::count = 0;

TetrisPuzzle::TetrisPuzzle() : id(count++) {

}

const char* TetrisPuzzle::GetName() {
	name = "Tetris" + to_string(id);
	return name.data();
}

void TetrisPuzzle::Init(Canvas* canvas, PostHandle* post) {
	memset(board, 0, sizeof(board));
	score = 0;
	level = 1;
	totalLines = 0;
	gravityTimerMs = 0;
	paused = false;
	gameOver = false;
	heldAction = -1;
	holdTimerMs = 0;
	holdRepeating = false;

	RecomputeLayout(canvas->GetWidth(), canvas->GetHeight());
	canvas->SetResizeFunc(&TetrisPuzzle::OnCanvasResize, this);

	nextType = GetRandom(kPieceTypeCount);
	SpawnPiece();
}

void TetrisPuzzle::RecomputeLayout(int width, int height) {
	// 棋盘按画布高度留出边距后算格子边长；侧栏放在棋盘右边，显示分数/等级/下一个方块预览，
	// 宽度按cellSize估一个够用的近似值(侧栏内容都是短文字+一个方块预览，不需要精确量出
	// 实际渲染宽度)。"棋盘+侧栏"作为一个整体在画布内居中，而不是钉死在左上角——这样窗口
	// 拉伸/切分辨率之后主游戏区域始终保持在屏幕正中间。
	//
	// 屏幕按键(鼠标/触摸操作)的位置：竖屏(手机)放在棋盘下方一整条；横屏放在侧栏右边一个
	// 两列的按键面板。
	int gap = 20;
	bool portrait = width < height * 12 / 10;

	if (portrait) {
		gap = max(8, width / 40);
		int controlsHeight = max(60, height * 22 / 100);
		int areaHeight = height - controlsHeight;
		cellSize = max(10, min(areaHeight / (kBoardHeight + 1), (width - gap - 16) / (kBoardWidth + 3)));
		int boardWidth = kBoardWidth * cellSize;
		int boardHeight = kBoardHeight * cellSize;
		int sidebarWidth = cellSize * 3;

		boardOriginX = max(8, (width - (boardWidth + gap + sidebarWidth)) / 2);
		boardOriginY = max(4, (areaHeight - boardHeight) / 2);
		sidebarX = boardOriginX + boardWidth + gap;

		int margin = max(6, width / 60);
		int rowHeight = (controlsHeight - margin) / 2;
		static const int kRow1[4] = { ACTION_LEFT, ACTION_ROTATE, ACTION_RIGHT, ACTION_DOWN };
		static const int kRow2[3] = { ACTION_DROP, ACTION_PAUSE, ACTION_EXIT };
		PlaceButtonRow(kRow1, 4, margin, areaHeight, width - margin * 2, rowHeight - margin / 2);
		PlaceButtonRow(kRow2, 3, margin, areaHeight + rowHeight, width - margin * 2, rowHeight - margin / 2);
	}
	else {
		cellSize = max(10, height / (kBoardHeight + 2));
		int boardWidth = kBoardWidth * cellSize;
		int boardHeight = kBoardHeight * cellSize;
		int sidebarWidth = cellSize * 3;
		int panelWidth = cellSize * 7;

		int totalWidth = boardWidth + gap + sidebarWidth + gap + panelWidth;
		boardOriginX = max(10, (width - totalWidth) / 2);
		boardOriginY = max(10, (height - boardHeight) / 2);
		sidebarX = boardOriginX + boardWidth + gap;

		int panelX = sidebarX + sidebarWidth + gap;
		int rowHeight = boardHeight / 4;
		int btnHeight = rowHeight - 8;
		static const int kRows[3][2] = { { ACTION_ROTATE, ACTION_DROP }, { ACTION_LEFT, ACTION_RIGHT }, { ACTION_DOWN, ACTION_PAUSE } };
		for (int row = 0; row < 3; row++) {
			PlaceButtonRow(kRows[row], 2, panelX, boardOriginY + row * rowHeight, panelWidth, btnHeight);
		}
		static const int kRow4[1] = { ACTION_EXIT };
		PlaceButtonRow(kRow4, 1, panelX, boardOriginY + 3 * rowHeight, panelWidth, btnHeight);
	}

	uiFontSize = max(14, cellSize * 5 / 8);
}

void TetrisPuzzle::PlaceButtonRow(const int* actions, int count, int x, int y, int w, int h) {
	int gapX = max(4, w / 60);
	int btnWidth = (w - gapX * (count - 1)) / count;
	for (int i = 0; i < count; i++) {
		Button& button = buttons[actions[i]];
		button.x1 = x + i * (btnWidth + gapX);
		button.y1 = y;
		button.x2 = button.x1 + btnWidth;
		button.y2 = y + h;
	}
}

int TetrisPuzzle::HitButton(int x, int y) const {
	for (int i = 0; i < ACTION_COUNT; i++) {
		const Button& button = buttons[i];
		if (x >= button.x1 && x <= button.x2 && y >= button.y1 && y <= button.y2) return i;
	}
	return -1;
}

bool TetrisPuzzle::IsRepeatAction(int action) const {
	return action == ACTION_LEFT || action == ACTION_RIGHT || action == ACTION_DOWN;
}

bool TetrisPuzzle::DoAction(int action) {
	if (action == ACTION_EXIT) return true; // 不管是否已经游戏结束，退出都直接结束这一局
	if (gameOver) return false;

	if (action == ACTION_PAUSE) { paused = !paused; return false; }
	if (paused) return false;

	switch (action) {
	case ACTION_LEFT: TryMove(-1, 0); break;
	case ACTION_RIGHT: TryMove(1, 0); break;
	case ACTION_DOWN: TryMove(0, 1); break;
	case ACTION_ROTATE: TryRotate(); break;
	case ACTION_DROP: HardDrop(); break;
	}
	return false;
}

void TetrisPuzzle::OnCanvasResize(void* userData, int width, int height) {
	static_cast<TetrisPuzzle*>(userData)->RecomputeLayout(width, height);
}

void TetrisPuzzle::SpawnPiece() {
	currentType = nextType;
	nextType = GetRandom(kPieceTypeCount);

	const PieceShape& shape = kPieces[currentType];
	memcpy(cellsRel, shape.cells, sizeof(cellsRel));
	pieceSize = shape.size;
	pieceX = (kBoardWidth - pieceSize) / 2;
	pieceY = 0;

	if (!CanPlace(pieceX, pieceY, cellsRel)) {
		gameOver = true;
	}
}

bool TetrisPuzzle::CanPlace(int x, int y, const int cells[4][2]) const {
	for (int i = 0; i < 4; i++) {
		int bx = x + cells[i][0];
		int by = y + cells[i][1];
		if (bx < 0 || bx >= kBoardWidth || by >= kBoardHeight) return false;
		if (by >= 0 && board[by][bx] != 0) return false;
	}
	return true;
}

bool TetrisPuzzle::TryMove(int dx, int dy) {
	if (!CanPlace(pieceX + dx, pieceY + dy, cellsRel)) return false;
	pieceX += dx;
	pieceY += dy;
	return true;
}

void TetrisPuzzle::TryRotate() {
	if (pieceSize <= 2) return; // O方块转了还是原样，不用做

	int rotated[4][2];
	for (int i = 0; i < 4; i++) {
		int x = cellsRel[i][0], y = cellsRel[i][1];
		rotated[i][0] = pieceSize - 1 - y;
		rotated[i][1] = x;
	}

	// 简单踢墙：先试原位，再试左右各挪一格，都不行就放弃这次旋转。
	static const int kKickOffsets[3] = { 0, -1, 1 };
	for (int offset : kKickOffsets) {
		if (CanPlace(pieceX + offset, pieceY, rotated)) {
			memcpy(cellsRel, rotated, sizeof(cellsRel));
			pieceX += offset;
			return;
		}
	}
}

void TetrisPuzzle::HardDrop() {
	while (TryMove(0, 1)) {}
	LockPiece();
}

void TetrisPuzzle::LockPiece() {
	for (int i = 0; i < 4; i++) {
		int bx = pieceX + cellsRel[i][0];
		int by = pieceY + cellsRel[i][1];
		if (by >= 0 && by < kBoardHeight && bx >= 0 && bx < kBoardWidth) {
			board[by][bx] = currentType + 1;
		}
	}

	ClearLines();
	gravityTimerMs = 0;
	SpawnPiece();
}

void TetrisPuzzle::ClearLines() {
	int cleared = 0;
	for (int y = kBoardHeight - 1; y >= 0; y--) {
		bool full = true;
		for (int x = 0; x < kBoardWidth; x++) {
			if (board[y][x] == 0) { full = false; break; }
		}
		if (!full) continue;

		cleared++;
		for (int moveY = y; moveY > 0; moveY--) {
			memcpy(board[moveY], board[moveY - 1], sizeof(board[moveY]));
		}
		memset(board[0], 0, sizeof(board[0]));
		y++; // 这一行现在是搬下来的上一行内容，重新检查一次
	}

	if (cleared <= 0) return;
	totalLines += cleared;
	score += cleared * cleared * 100 * level; // 一次清多行给更高分，简单实现不追求跟标准计分表一致
	level = 1 + totalLines / 10;
}

JsonValue* TetrisPuzzle::Finish() {
	result = JsonValue(DATA_OBJECT);
	result["score"] = score;
	return &result;
}

JsonValue* TetrisPuzzle::Loop(Canvas* canvas, int ms, PostHandle* post) {
	while (canvas->HasPendingKey()) {
		int raw = canvas->PopKey();
		bool released = (raw & KEY_RELEASED_FLAG) != 0;
		int key = raw & ~KEY_RELEASED_FLAG;
		if (released) continue; // 只处理按下

		if (key == KEY_ESCAPE) {
			return Finish(); // 不管是否已经游戏结束，ESC都直接退出这一局
		}

		if (gameOver) continue; // 结束画面只响应ESC，见上面

		if (key == 'p') {
			paused = !paused;
			continue;
		}
		if (paused) continue;

		if (key == KEY_LEFT || key == 'a') TryMove(-1, 0);
		else if (key == KEY_RIGHT || key == 'd') TryMove(1, 0);
		else if (key == KEY_DOWN || key == 's') TryMove(0, 1);
		else if (key == KEY_UP || key == 'w') TryRotate();
		else if (key == ' ') HardDrop();
	}
	canvas->ClearKeyBuffer();

	while (canvas->HasPendingMouseEvent()) {
		MouseEvent event = canvas->PopMouseEvent();
		if (event.button != MOUSE_LEFT) continue;

		if (!event.down) {
			heldAction = -1;
			continue;
		}

		int action = HitButton(event.x, event.y);
		if (action < 0) {
			// 暂停中点击按键以外的任意位置也算继续，方便触摸操作。
			if (paused && !gameOver) paused = false;
			continue;
		}
		if (DoAction(action)) {
			canvas->ClearMouseBuffer();
			return Finish();
		}
		if (IsRepeatAction(action)) {
			heldAction = action;
			holdTimerMs = 0;
			holdRepeating = false;
		}
	}

	// 按住←→↓连发：首次延迟250ms，之后每90ms一次。
	if (heldAction >= 0 && !gameOver && !paused) {
		holdTimerMs += ms;
		int threshold = holdRepeating ? 90 : 250;
		while (holdTimerMs >= threshold) {
			holdTimerMs -= threshold;
			holdRepeating = true;
			threshold = 90;
			DoAction(heldAction);
		}
	}
	else {
		holdTimerMs = 0;
		holdRepeating = false;
	}

	if (!gameOver && !paused) {
		gravityTimerMs += ms;
		int interval = max(100, 800 - level * 60);
		if (gravityTimerMs >= interval) {
			gravityTimerMs = 0;
			if (!TryMove(0, 1)) LockPiece();
		}
	}

	Render(canvas);
	return nullptr;
}

void TetrisPuzzle::Render(Canvas* canvas) const {
	canvas->SetColor(20, 20, 20);
	canvas->ClearScreen();

	// 棋盘边框
	canvas->SetColor(200, 200, 200);
	canvas->PutRect(boardOriginX - 2, boardOriginY - 2,
		boardOriginX + kBoardWidth * cellSize + 1, boardOriginY + kBoardHeight * cellSize + 1, false);

	// 已落定的方块
	for (int y = 0; y < kBoardHeight; y++) {
		for (int x = 0; x < kBoardWidth; x++) {
			if (board[y][x] == 0) continue;
			const PieceShape& shape = kPieces[board[y][x] - 1];
			canvas->SetColor(shape.r, shape.g, shape.b);
			int px = boardOriginX + x * cellSize, py = boardOriginY + y * cellSize;
			canvas->PutRect(px, py, px + cellSize - 2, py + cellSize - 2, true);
		}
	}

	// 当前下落方块
	if (!gameOver) {
		const PieceShape& shape = kPieces[currentType];
		canvas->SetColor(shape.r, shape.g, shape.b);
		for (int i = 0; i < 4; i++) {
			int x = pieceX + cellsRel[i][0], y = pieceY + cellsRel[i][1];
			if (y < 0) continue;
			int px = boardOriginX + x * cellSize, py = boardOriginY + y * cellSize;
			canvas->PutRect(px, py, px + cellSize - 2, py + cellSize - 2, true);
		}
	}

	// 侧栏：分数/等级/下一个方块预览
	canvas->SetFontSize(uiFontSize);
	canvas->SetColor(255, 255, 255);
	int line = canvas->GetFontHeight();
	canvas->PutString("SCORE", sidebarX, boardOriginY);
	canvas->PutString(to_string(score), sidebarX, boardOriginY + line);
	canvas->PutString("LEVEL", sidebarX, boardOriginY + line * 5 / 2);
	canvas->PutString(to_string(level), sidebarX, boardOriginY + line * 7 / 2);
	canvas->PutString("NEXT", sidebarX, boardOriginY + line * 5);

	const PieceShape& next = kPieces[nextType];
	canvas->SetColor(next.r, next.g, next.b);
	int previewCell = max(4, cellSize * 2 / 3);
	for (int i = 0; i < 4; i++) {
		int px = sidebarX + next.cells[i][0] * previewCell;
		int py = boardOriginY + line * 6 + next.cells[i][1] * previewCell;
		canvas->PutRect(px, py, px + previewCell - 2, py + previewCell - 2, true);
	}

	RenderButtons(canvas);

	if (paused) {
		canvas->SetColor(255, 255, 0);
		canvas->SetFontSize(uiFontSize * 3 / 2);
		canvas->PutString("PAUSE", boardOriginX + 10, boardOriginY + kBoardHeight * cellSize / 2);
	}
	if (gameOver) {
		canvas->SetColor(255, 60, 60);
		canvas->SetFontSize(uiFontSize * 3 / 2);
		int textY = boardOriginY + kBoardHeight * cellSize / 2;
		canvas->PutString("GAME OVER", boardOriginX + 10, textY);
		canvas->SetFontSize(uiFontSize);
		canvas->PutString("PRESS EXIT TO LEAVE", boardOriginX + 10, textY + uiFontSize * 2);
	}
}

void TetrisPuzzle::RenderButtons(Canvas* canvas) const {
	static const char* kLabels[ACTION_COUNT] = { "LEFT", "ROTATE", "RIGHT", "DOWN", "DROP", "PAUSE", "EXIT" };
	for (int i = 0; i < ACTION_COUNT; i++) {
		const Button& button = buttons[i];
		bool held = (i == heldAction);
		canvas->SetColor(held ? 110 : 55, held ? 110 : 55, held ? 150 : 80);
		canvas->PutRect(button.x1, button.y1, button.x2, button.y2, true);
		canvas->SetColor(160, 160, 190);
		canvas->PutRect(button.x1, button.y1, button.x2, button.y2, false);

		string label = (i == ACTION_PAUSE && paused) ? "RESUME" : kLabels[i];
		int btnHeight = button.y2 - button.y1;
		canvas->SetFontSize(max(12, min(uiFontSize, btnHeight * 2 / 5)));
		canvas->SetColor(255, 255, 255);
		int textX = button.x1 + ((button.x2 - button.x1) - canvas->StringWidth(label)) / 2;
		int textY = button.y1 + (btnHeight - canvas->GetFontHeight()) / 2;
		canvas->PutString(label, textX, textY);
	}
}


int AnyKeyPuzzle::count = 0;

AnyKeyPuzzle::AnyKeyPuzzle() : id(count++) {

}

const char* AnyKeyPuzzle::GetName() {
	name = "AnyKey" + to_string(id);
	return name.data();
}

void AnyKeyPuzzle::SetParam(const std::string& param) {
	message = param;
}

void AnyKeyPuzzle::Init(Canvas* canvas, PostHandle* post) {
	canvas->ClearKeyBuffer(); // 打开瞬间还按着的键(比如点选项的那一下)不能算退出
	canvas->ClearMouseBuffer();

	// 配图：从Resource/Mod/Basic/下按文件名加载(文件名带mod名前缀保证全局唯一)，加载不到就只显示文字。
	hasImage = LoadModImage(post, "puzzle_anykey_icon.png", image);
}

JsonValue* AnyKeyPuzzle::Loop(Canvas* canvas, int ms, PostHandle* post) {
	bool pressed = false;
	while (canvas->HasPendingKey()) {
		int raw = canvas->PopKey();
		if ((raw & KEY_RELEASED_FLAG) == 0) pressed = true;
	}
	while (canvas->HasPendingMouseEvent()) {
		if (canvas->PopMouseEvent().down) pressed = true; // 点击/触摸任意处也算按键
	}
	if (pressed) {
		result = JsonValue(DATA_OBJECT);
		return &result;
	}

	canvas->SetColor(0, 0, 0);
	canvas->ClearScreen();
	canvas->SetColor(255, 255, 255);
	if (hasImage) {
		int side = min(canvas->GetWidth(), canvas->GetHeight()) / 3;
		int imgW = side, imgH = side * image.height / max(1, image.width);
		canvas->SetAlpha(1.f);
		canvas->PutBitmap(image, (canvas->GetWidth() - imgW) / 2, canvas->GetHeight() / 2 - 12 - imgH - 20, imgW, imgH);
	}
	canvas->SetFontSize(24);
	string text = message.empty() ? string("Press any key") : message;
	canvas->PutString(text, (canvas->GetWidth() - canvas->StringWidth(text)) / 2, canvas->GetHeight() / 2 - 12);
	canvas->SetFontSize(14);
	string hint = "Press any key or tap";
	canvas->PutString(hint, (canvas->GetWidth() - canvas->StringWidth(hint)) / 2, canvas->GetHeight() / 2 + 30);
	return nullptr;
}
