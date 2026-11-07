#pragma once

#include "puzzle_common.h"


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
