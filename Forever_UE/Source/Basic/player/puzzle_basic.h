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

	static const char* GetId() { return "puzzle_tetris"; }
	virtual const char* GetType() const override { return "puzzle_tetris"; }
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

	// 鼠标/触摸操作：画布上画一组屏幕按键，点击等价于对应键盘按键(←→↓移动/旋转/硬降/暂停/
	// 退出)，让没有键盘的设备(手机网页)也能完整操作。键盘路径保持不变，两者并存。
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


// 按任意键退出：只显示脚本通过start_puzzle的param传来的一行提示文字，收到任意按键(按下)就结束。
// 用来占位"制作汉堡"之类还没有真正玩法的小游戏，结束后脚本靠puzzle_result事件继续往下走。
class AnyKeyPuzzle : public PuzzleMod {
public:
	AnyKeyPuzzle();

	static const char* GetId() { return "puzzle_anykey"; }
	virtual const char* GetType() const override { return "puzzle_anykey"; }
	virtual const char* GetName() override;

	virtual void SetParam(const std::string& param) override;
	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual int Loop(Canvas* canvas, int ms, PostHandle* post) override;

private:
	static int count;
	int id;
	std::string name;
	std::string message;
};
