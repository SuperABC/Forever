#pragma once

#include "player/app_mod.h"
#include "player/app_factory.h"

#include <string>


// App：手机里的一个App实例，薄包装类，持有Mod*+Factory*，方法直接转发——照抄
// Core/player/puzzle.h的写法(这里是Phone::AppEntry独占持有一个App，见Core/player/phone.h)。
class App {
public:
	/*
	* 禁止默认构造
	*/
	App() = delete;

	/*
	* 构造App实例
	* @factory: App工厂
	* @id: App类型标识
	*/
	App(AppFactory* factory, const std::string& id);

	/*
	* 析构App实例：factory->DestroyApp(mod)
	*/
	~App();

	/*
	* App动态类型标识
	*/
	std::string GetType() const;

	/*
	* App实例唯一名称
	*/
	std::string GetName() const;

	/*
	* 第一次打开这个App时调用一次
	* @canvas: 画布，不持有所有权
	* @post: 向Core发起查询的句柄
	*/
	void Init(Canvas* canvas, PostHandle* post);

	/*
	* 前台运行时每帧调用一次
	* @canvas: 画布，不持有所有权
	* @ms: 距上一帧的毫秒数
	* @post: 向Core发起查询的句柄
	*/
	void Loop(Canvas* canvas, int ms, PostHandle* post);

	/*
	* 收到"返回"操作时调用
	* @canvas: 画布，不持有所有权
	* @post: 向Core发起查询的句柄
	*/
	void Back(Canvas* canvas, PostHandle* post);

	/*
	* 从后台切回前台时调用：不重新Init，只按已有状态重画一次
	* @canvas: 画布，不持有所有权
	* @post: 向Core发起查询的句柄
	*/
	void Refresh(Canvas* canvas, PostHandle* post);

private:
	// 工厂
	AppFactory* factory;

	// 模组对象
	AppMod* mod;
};
