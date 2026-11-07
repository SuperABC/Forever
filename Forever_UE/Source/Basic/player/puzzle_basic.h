#pragma once

#include "player/puzzle_mod.h"

#include <string>


class TetrisPuzzle : public PuzzleMod {
public:
	TetrisPuzzle();

	static const char* GetId() { return "puzzle_tetris"; }
	virtual const char* GetType() const override { return "puzzle_tetris"; }
	virtual const char* GetName() override;

	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) override;

private:
	// 结束时填好结果({"score":n})并返回，指针归自己所有
	JsonValue* Finish();
	JsonValue result;

	static constexpr int kBoardWidth = 10;
	static constexpr int kBoardHeight = 20;
	static constexpr int kPieceTypeCount = 7;

	// 一种方块的基础形状：外接正方形边长(2=O，3=大多数，4=I)+4个格子在这个正方形里的坐标+
	// 渲染颜色。
	struct PieceShape {
		int size;
		int cells[4][2];
		int r, g, b;
	};
	static const PieceShape kPieces[kPieceTypeCount];

	void SpawnPiece();
	bool CanPlace(int x, int y, const int cells[4][2]) const;
	bool TryMove(int dx, int dy);
	void TryRotate();
	void HardDrop();
	void LockPiece();
	void ClearLines();
	void Render(Canvas* canvas) const;

	// 屏幕按键：点击等价于对应键盘按键，见puzzle_basic.md。
	enum Action { ACTION_LEFT, ACTION_ROTATE, ACTION_RIGHT, ACTION_DOWN, ACTION_DROP, ACTION_PAUSE, ACTION_EXIT, ACTION_COUNT };
	struct Button {
		int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	};
	int HitButton(int x, int y) const;
	bool IsRepeatAction(int action) const;
	// 执行一个动作，返回true表示请求退出这一局。
	bool DoAction(int action);
	void PlaceButtonRow(const int* actions, int count, int x, int y, int w, int h);
	void RenderButtons(Canvas* canvas) const;

	// 按画布尺寸重算布局：Init()调一次，画布resize时经Canvas::ResizeFunc回调再调。
	void RecomputeLayout(int width, int height);

	// ResizeFunc是裸函数指针，不能指向成员函数，用userData带this转发给RecomputeLayout。
	static void OnCanvasResize(void* userData, int width, int height);

	int board[kBoardHeight][kBoardWidth] = {}; // 0=空，否则是kPieces下标+1

	int currentType = 0;
	int nextType = 0;
	int cellsRel[4][2] = {};
	int pieceSize = 0;
	int pieceX = 0, pieceY = 0;

	int score = 0;
	int level = 1;
	int totalLines = 0;
	int gravityTimerMs = 0;
	bool paused = false;
	bool gameOver = false;

	int cellSize = 0;
	int boardOriginX = 0, boardOriginY = 0;
	int sidebarX = 0;
	int uiFontSize = 14;

	Button buttons[ACTION_COUNT];
	int heldAction = -1;       // 正被按住的可连发按键(←→↓)，-1=没有
	int holdTimerMs = 0;
	bool holdRepeating = false;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};


class AnyKeyPuzzle : public PuzzleMod {
public:
	AnyKeyPuzzle();

	static const char* GetId() { return "puzzle_anykey"; }
	virtual const char* GetType() const override { return "puzzle_anykey"; }
	virtual const char* GetName() override;

	virtual void SetParam(const std::string& param) override;
	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) override;

private:
	JsonValue result; // 结束时返回{}，没有任何字段
	static int count;
	int id;
	std::string name;
	std::string message;
	Bitmap image;
	bool hasImage = false;
};
