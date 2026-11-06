#pragma once

#include "puzzle_common.h"


// 躲避方块：正方形场地里一个红色小方块(玩家拖拽)和四个蓝色长方形(上下左右各一个，开始后按各自朝外
// 的随机方向飞，每隔一段时间一起加速)。红块碰到蓝块或四周障碍就结束，得分是存活秒数。
// 场地内部全部用归一化坐标([0,1]x[0,1])，画面按画布大小等比映射，窗口缩放不影响状态。
// 参数(SetParam, key=value空格分隔)：speed初速度(场地边长/秒，默认0.12)、accel每次加速倍数(1.15)、
// interval加速间隔秒(10)、ratio长宽比上限(3，即1:3~3:1)、seed。
// 返回：{"score":整数秒,"time":秒,"completed":是否被碰撞结束}。
class DodgePuzzle : public PuzzleMod {
public:
	virtual void SetParam(const std::string& param) override;
	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) override;

	PUZZLE_IDENTITY(DodgePuzzle, "puzzle_dodge")

private:
	struct Block {
		double cx, cy, w, h; // 中心和尺寸
		double vx, vy;       // 方向(单位向量)
	};

	JsonValue* Finish(bool completed);
	void Step(double dt);
	bool Hits() const;
	void Render(Canvas* canvas);

	PuzzleParams params;
	PuzzleRng rng;
	Bitmap image;
	bool hasImage = false;
	JsonValue result;

	static constexpr double kObstacle = 0.06;
	static constexpr double kRedSize = 0.07;

	double speed0 = 0.12, accel = 1.15, interval = 10.0, ratioMax = 3.0;
	double speed = 0.12;
	double redX = 0.5, redY = 0.5;
	Block blues[4];
	bool started = false, dead = false, grabbing = false;
	double grabDx = 0, grabDy = 0;
	double elapsed = 0, nextAccel = 10.0;
	double deadTime = 0;
	// 画面映射：场地左上角像素和边长
	int fieldX = 0, fieldY = 0, fieldSize = 1;
};
