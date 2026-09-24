#include "puzzle_tetris.h"

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
	cellSize = max(10, height / (kBoardHeight + 2));
	int boardWidth = kBoardWidth * cellSize;
	int boardHeight = kBoardHeight * cellSize;
	int sidebarWidth = cellSize * 3;
	int gap = 20;

	boardOriginX = max(10, (width - (boardWidth + gap + sidebarWidth)) / 2);
	boardOriginY = max(10, (height - boardHeight) / 2);
	sidebarX = boardOriginX + boardWidth + gap;
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

int TetrisPuzzle::Loop(Canvas* canvas, int ms, PostHandle* post) {
	while (canvas->HasPendingKey()) {
		int raw = canvas->PopKey();
		bool released = (raw & KEY_RELEASED_FLAG) != 0;
		int key = raw & ~KEY_RELEASED_FLAG;
		if (released) continue; // 只处理按下

		if (key == KEY_ESCAPE) {
			return 1; // 不管是否已经游戏结束，ESC都直接退出这一局
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

	if (!gameOver && !paused) {
		gravityTimerMs += ms;
		int interval = max(100, 800 - level * 60);
		if (gravityTimerMs >= interval) {
			gravityTimerMs = 0;
			if (!TryMove(0, 1)) LockPiece();
		}
	}

	Render(canvas);
	return 0;
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
	canvas->SetFontSize(14);
	canvas->SetColor(255, 255, 255);
	canvas->PutString("SCORE", sidebarX, boardOriginY);
	canvas->PutString(to_string(score), sidebarX, boardOriginY + 20);
	canvas->PutString("LEVEL", sidebarX, boardOriginY + 60);
	canvas->PutString(to_string(level), sidebarX, boardOriginY + 80);
	canvas->PutString("NEXT", sidebarX, boardOriginY + 120);

	const PieceShape& next = kPieces[nextType];
	canvas->SetColor(next.r, next.g, next.b);
	int previewCell = cellSize * 2 / 3;
	for (int i = 0; i < 4; i++) {
		int px = sidebarX + next.cells[i][0] * previewCell;
		int py = boardOriginY + 150 + next.cells[i][1] * previewCell;
		canvas->PutRect(px, py, px + previewCell - 2, py + previewCell - 2, true);
	}

	if (paused) {
		canvas->SetColor(255, 255, 0);
		canvas->SetFontSize(21);
		canvas->PutString("PAUSE", boardOriginX + 10, boardOriginY + kBoardHeight * cellSize / 2);
	}
	if (gameOver) {
		canvas->SetColor(255, 60, 60);
		canvas->SetFontSize(21);
		int textY = boardOriginY + kBoardHeight * cellSize / 2;
		canvas->PutString("GAME OVER", boardOriginX + 10, textY);
		canvas->SetFontSize(14);
		canvas->PutString("ESC TO EXIT", boardOriginX + 10, textY + 30);
	}
}
