#pragma once

#include "player/puzzle_mod.h"

#include <string>


// 俄罗斯方块——作为新Canvas图形/输入API的落地测试重新实现(不照抄老工程puzzle_tetris)。
// 标准10x20棋盘，7种方块用"一份基础形状+按外接正方形边长通用旋转公式((x,y)->(size-1-y,x))"
// 的方式实现，不需要为每种方块单独存4个旋转态的查表数据。
//
// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id+GetName里现拼，和
// Source/Basic/map/terrain_basic.h/.cpp的OceanTerrain同一个模式。
class TetrisPuzzle : public PuzzleMod {
public:
	TetrisPuzzle();

	static const char* GetId() { return "tetris"; }
	virtual const char* GetType() const override { return "tetris"; }
	virtual const char* GetName() override;

	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual int Loop(Canvas* canvas, int ms, PostHandle* post) override;

private:
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

	// 按画布尺寸重算格子边长/棋盘偏移/侧栏位置——Init()首次调用一次，之后画布被resize时通过
	// Canvas::ResizeFunc回调再调一次，让窗口拉伸后棋盘还能贴合新的分辨率，不会保留旧尺寸下
	// 算出来的布局。
	void RecomputeLayout(int width, int height);

	// 注册给Canvas::SetResizeFunc的静态转发函数——ResizeFunc是裸函数指针，不能直接指向成员
	// 函数，这里用userData带上this再转发给RecomputeLayout。
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

	static int count;
	int id;
	std::string name;
};
