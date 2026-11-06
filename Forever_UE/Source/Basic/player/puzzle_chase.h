#pragma once

#include "puzzle_common.h"


// 生死时速(2D打字追逐)：上方跑道里小偷以固定速度向前跑，玩家(警察)每正确敲对文章里的一个字符就前进一格。
// 警察位置追上小偷位置算成功；小偷跑到文章末尾(文章打完前没追上)算失败。敲错字符不前进，并有短暂硬直。
// 文章是文本文件(通过post "resource path"按名字找，默认puzzle_chase_article.txt)。需要键盘，没有触摸方案。
// 参数(key=value)：wpm(小偷速度，每分钟字数，默认40，1词=5字符)、lead(警察落后小偷的字符数，默认15)、
// penalty(敲错后硬直毫秒，默认300)、article(文章文件名)。
// 返回：{"success","progress"(0~1打完的比例),"time"(秒),"accuracy"(0~1),"completed"(是否分出胜负)}。
class ChasePuzzle : public PuzzleMod {
public:
	virtual void SetParam(const std::string& param) override;
	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) override;

	PUZZLE_IDENTITY(ChasePuzzle, "puzzle_chase")

private:
	JsonValue* Finish(bool completed);
	void Wrap(Canvas* canvas, int width, int fontSize);
	void Render(Canvas* canvas);
	void DrawRunner(Canvas* canvas, int cx, int groundY, int size, bool thief, double phase) const;

	PuzzleParams params;
	JsonValue result;

	std::string article;
	double thiefSpeed = 3.33; // 字符/秒
	double lead = 15, penaltyMs = 300;

	int typed = 0, wrong = 0;
	double thiefPos = 15;
	double elapsed = 0, stunned = 0, flash = 0;
	enum State { RUNNING, WON, LOST } state = RUNNING;
	double endTime = 0;
	bool loadFailed = false;

	// 排版缓存：每行的起止字符下标
	struct Line { int begin, end; };
	std::vector<Line> lines;
	int wrapWidth = -1, wrapFont = -1;
	int unit = 16;
};
