#pragma once

#include "common/mod_test.h"
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

	// 要不要在手机桌面上放图标，默认放。不放图标的App(占位模板、以后只在后台跑的App)在自己的构造函数里设成false，
	// Phone::BuildAppList据此跳过它。
	bool showOnDesktop = true;

	// 单元测试：print输出关键信息，返回是否通过。没有深度测试的mod用FOREVER_MOD_GENERIC_UNIT_TEST(见common/mod_test.h)。
	virtual bool UnitTest(TestPrintFn print) = 0;
	// 第一次打开时调用一次;canvas只用来读尺寸布局初始状态(分辨率动态),不持有所有权。
	virtual void Init(Canvas* canvas, PostHandle* post) = 0;

	// 前台运行时每帧调用,在canvas上画界面;不返回int,退出/切换由Phone决定。
	virtual void Loop(Canvas* canvas, int ms, PostHandle* post) = 0;

	// 收到"返回"操作时调用,多级页面的App在这里弹出导航栈。
	virtual void Back(Canvas* canvas, PostHandle* post) = 0;

	// 从后台切回前台时调用:不重新Init,按已有状态重画。
	virtual void Refresh(Canvas* canvas, PostHandle* post) = 0;

	// 在任务列表里被关掉时调用一次(随后实例被销毁)，App在这里收尾，比如Music停掉自己的音轨。
	// 只有关掉才调：退回桌面/切到别的App不算退出；整局结束(手机随世界销毁)也不调。默认什么都不做。
	virtual void Exit(PostHandle* post) {}

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
