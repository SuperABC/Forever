#pragma once

#include "puzzle_common.h"


class TowerPuzzle : public PuzzleMod {
public:
	virtual void SetParam(const std::string& param) override;
	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) override;

	PUZZLE_IDENTITY(TowerPuzzle, "puzzle_tower")

private:
	JsonValue* Finish(bool completed);
	void Spawn();
	double Sway(int floor) const;          // 第floor层(0=底座)当前的横向摇摆位移(像素)
	double SwayAmplitude() const;
	double FloorX(int floor) const;        // 第floor层中心的屏幕x
	double HookX() const;
	void Drop();
	void Render(Canvas* canvas);
	void DrawHouse(Canvas* canvas, double cx, double bottomY, int style, float alpha = 1.f) const;

	PuzzleParams params;
	JsonValue result;
	PuzzleRng rng;

	int lives = 3, livesLeft = 3;
	double swing = 1, overlapNeed = 0.25;
	double startPhase = -1;

	// 单位：像素，世界y向上(0=地面)。布局在Loop里按画布更新。
	int w = 480, h = 640;
	double bw = 120, bh = 60;
	std::vector<double> offsets;  // 每层相对底座中心的静态偏移(底座=0)
	std::vector<int> styles;
	double time = 0;              // 全局时钟，驱动摇摆
	double phase = 0, hookTime = 0;
	double camY = 0;

	enum State { HANGING, FALLING, MISSED, DONE } state = HANGING;
	double blockX = 0, blockBottom = 0, blockVy = 0; // 下落中的屏幕x/世界底y/速度
	int blockStyle = 0;
	double stateTime = 0;
	int floors = 0;
	std::string toast;
	double toastTime = 0;
	double endTime = 0;
	double groundScreenY = 0;
};
