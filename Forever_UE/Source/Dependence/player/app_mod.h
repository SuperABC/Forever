#pragma once

#include "common/mod_state.h"
#include "player/canvas.h"
#include "common/handle.h"

#include <string>


// 设计见app_mod.md。
class AppMod {
public:
	AppMod() = default;
	virtual ~AppMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 第一次打开时调用一次;canvas只用来读尺寸布局初始状态(分辨率动态),不持有所有权。
	virtual void Init(Canvas* canvas, PostHandle* post) = 0;

	// 前台运行时每帧调用,在canvas上画界面;不返回int,退出/切换由Phone决定。
	virtual void Loop(Canvas* canvas, int ms, PostHandle* post) = 0;

	// 收到"返回"操作时调用,多级页面的App在这里弹出导航栈。
	virtual void Back(Canvas* canvas, PostHandle* post) = 0;

	// 从后台切回前台时调用:不重新Init,按已有状态重画。
	virtual void Refresh(Canvas* canvas, PostHandle* post) = 0;

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
