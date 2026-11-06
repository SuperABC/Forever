#pragma once

#include "puzzle_common.h"


// 广场舞：回合制。每回合中心显示一串方向箭头，玩家按顺序按方向键(或点屏幕方向按钮)，按错清空本回合
// 输入从头再来；全部按对后按空格/"OK"按钮完成。底部计时条上紫色滑块匀速从左到右，条上靠后的橙色段是完美区：
// 完成时滑块在完美区得2分，其他位置得1分，滑块走到头还没完成得0分；上一回合和本回合都完美则本回合额外+1。
// 参数(key=value)：lengths每回合序列长度逗号分隔(默认3,3,3,4,4,4,5,5,5,6,6,7,7,8,8)、
// time每回合固定秒数(不给就用base+per*长度)、base(6)、per(0.6)、pstart/pend完美区起止比例(0.7/0.9)、seed。
// 返回：{"score","perfectRounds","normalRounds","failedRounds","maxStreak","completed"}。
class DancePuzzle : public PuzzleMod {
public:
	virtual void SetParam(const std::string& param) override;
	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) override;

	PUZZLE_IDENTITY(DancePuzzle, "puzzle_dance")

private:
	enum Phase { PHASE_PLAY, PHASE_FLASH, PHASE_END };
	enum Dir { DIR_LEFT, DIR_UP, DIR_RIGHT, DIR_DOWN };

	JsonValue* Finish(bool completed);
	void StartRound();
	void FinishRound(int points, bool perfect, bool failed);
	void PressDir(int dir);
	void PressDone();
	void Layout(int w, int h);
	void Render(Canvas* canvas);
	void DrawArrow(Canvas* canvas, int cx, int cy, int size, int dir, int r, int g, int b) const;

	PuzzleParams params;
	PuzzleRng rng;
	Bitmap image;
	bool hasImage = false;
	JsonValue result;

	std::vector<int> lengths;
	double baseTime = 6, perLen = 0.6, fixedTime = 0, perfectStart = 0.7, perfectEnd = 0.9;

	Phase phase = PHASE_PLAY;
	int round = 0;
	std::vector<int> sequence;
	int typed = 0;           // 已按对的个数
	double roundTime = 6, elapsed = 0, flashTime = 0;
	std::string flashText;
	int flashR = 255, flashG = 255, flashB = 255;
	bool lastPerfect = false;
	int streak = 0, maxStreak = 0;
	int score = 0, perfectRounds = 0, normalRounds = 0, failedRounds = 0;
	double endTime = 0;
	double shake = 0;

	PuzzleRect barArea, timerArea, dirButtons[4], doneButton;
	int arrowSize = 40, unit = 16;
};
