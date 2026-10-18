#pragma once

#include "common/mod_state.h"
#include "player/canvas.h"
#include "common/handle.h"

#include <string>


// AppMod："手机里的一个App"的Mod扩展接口，Init/Loop/Back/Refresh四个生命周期方法都是
// 真正的虚函数实例方法——Factory创建一个真实Mod*实例，虚函数调用(和PuzzleMod同一个模式)，
// Phone::AppEntry直接持有一个真正的App(Core包装类)实例，多个App互不干扰。
class AppMod {
public:
	AppMod() = default;
	virtual ~AppMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 第一次打开这个App时调用一次。跟PuzzleMod::Init不同的地方是这里的画布尺寸不是"新建"
	// 出来的——所有App共用Phone持有的那一块画布，Init拿到的Canvas*只是用来读
	// GetWidth()/GetHeight()布局初始状态(这个项目的画布分辨率是动态的，见Canvas::Resize，
	// App不能像老工程TestApp那样硬编码480x640去猜初始位置)，不持有所有权。
	virtual void Init(Canvas* canvas, PostHandle* post) = 0;

	// 这个App在前台运行时每帧调用一次，在canvas上画自己的界面。不像PuzzleMod::Loop，
	// 这里不返回int——没有App需要单方面结束整个Phone，退出/切换完全由Phone自己的
	// Home/Back/任务列表逻辑决定。
	virtual void Loop(Canvas* canvas, int ms, PostHandle* post) = 0;

	// 收到"返回"操作(手机底部工具栏的返回按钮/Backspace)时调用——多级页面的App在这里
	// 弹出自己的导航栈，单页App可以留空实现。
	virtual void Back(Canvas* canvas, PostHandle* post) = 0;

	// 从后台切回前台时调用：不重新Init，只需要按已有状态重画一次(比如恢复被切走前的
	// 页面/滚动位置)。
	virtual void Refresh(Canvas* canvas, PostHandle* post) = 0;

	// ---- 存档(见Source/Dependence/common/mod_state.md) ----
	// mod私有数据的存档/读档钩子，默认不存任何东西。基类里的公共声明字段由Core直接快照，
	// 这里只放mod自己的私有数据。
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	// SaveState写出数据的格式版本：改了自己SaveState的格式就加一，读档时从
	// StateReader::GetSavedVersion()拿到当时的版本自己迁移。默认0。
	virtual int GetStateVersion() const { return 0; }
	// 这个mod类用来生成唯一实例名的静态计数器——存档保存、读档后恢复，避免读档后新建的实例和
	// 恢复出来的名字撞车。没有计数器的类保持默认(返回-1、Set不做事)。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
