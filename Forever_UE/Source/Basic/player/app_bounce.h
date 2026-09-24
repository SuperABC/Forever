#pragma once

#include "player/app_mod.h"

#include <string>


// Bounce——手机系统的第一个测试App：一个在画布里反弹的方块，纯Loop驱动位置积分。
// 用来验证AppMod最基础的Init(随机初始位置/方向)和Refresh(从后台切回来后继续弹，
// 不能被Phone重置)，不涉及Back(单页App，留空实现)。
//
// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id+GetName里现拼，和
// Source/Basic/map/terrain_basic.h/.cpp的OceanTerrain同一个模式。
class BounceApp : public AppMod {
public:
	BounceApp();

	static const char* GetId() { return "bounce"; }
	virtual const char* GetType() const override { return "bounce"; }
	virtual const char* GetName() override;

	virtual void Init(Canvas* canvas, PostHandle* post) override;
	virtual void Loop(Canvas* canvas, int ms, PostHandle* post) override;
	virtual void Back(Canvas* canvas, PostHandle* post) override {} // 单页App，没有导航栈可退

	// 从后台恢复：不重新Init，只按当前位置重画一次(不推进物理)，跟老工程TestApp::Refresh
	// 同一个语义。
	virtual void Refresh(Canvas* canvas, PostHandle* post) override;

private:
	void Render(Canvas* canvas) const;

	float posX = 0.f, posY = 0.f;
	float velX = 0.f, velY = 0.f;
	int boxSize = 0;

	static int count;
	int id;
	std::string name;
};
