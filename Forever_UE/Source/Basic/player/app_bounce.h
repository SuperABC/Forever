#pragma once

#include "player/app_mod.h"

#include <string>
#include <vector>


class BounceApp : public AppMod {
public:
	BounceApp();

	static const char* GetId() { return "app_bounce"; }
	virtual const char* GetType() const override { return "app_bounce"; }
	virtual const char* GetName() override;

	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual void Loop(Canvas* canvas, int ms, PostHandle* post) override;
	virtual void Back(Canvas* canvas, PostHandle* post) override {} // 单页App，没有导航栈可退

	// 从后台恢复：不重新Init，只按当前位置重画一次(不推进物理)。
	virtual void Refresh(Canvas* canvas, PostHandle* post) override;

private:
	void Render(Canvas* canvas) const;

	float posX = 0.f, posY = 0.f;
	float velX = 0.f, velY = 0.f;
	int boxSize = 0;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
