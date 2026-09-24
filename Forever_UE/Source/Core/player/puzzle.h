#pragma once

#include "player/puzzle_mod.h"
#include "player/puzzle_factory.h"

#include <string>


// Puzzle：小游戏实体，薄包装类，持有Mod*+Factory*，方法直接转发——照抄
// Core/populace/scheduler.h那种"Citizen独占持有一个Scheduler"的写法（这里是
// UPuzzleWidget独占持有一个Puzzle，见Source/Forever/UI/PuzzleWidget.h）。
class Puzzle {
public:
	/*
	* 禁止默认构造
	*/
	Puzzle() = delete;

	/*
	* 构造小游戏实体
	* @factory: 小游戏工厂
	* @id: 小游戏类型标识
	*/
	Puzzle(PuzzleFactory* factory, const std::string& id);

	/*
	* 析构小游戏实体：factory->DestroyPuzzle(mod)
	*/
	~Puzzle();

	/*
	* 小游戏动态类型标识
	*/
	std::string GetType() const;

	/*
	* 小游戏实例唯一名称
	*/
	std::string GetName() const;

	/*
	* 初始化小游戏
	* @canvas: 画布，不持有所有权
	* @post: 向Core发起查询的句柄
	*/
	void Init(Canvas* canvas, PostHandle* post);

	/*
	* 每帧更新逻辑
	* @canvas: 画布，不持有所有权
	* @ms: 距上一帧的毫秒数
	* @post: 向Core发起查询的句柄
	* @return: 帧状态码，0继续运行，非0退出
	*/
	int Loop(Canvas* canvas, int ms, PostHandle* post);

private:
	// 工厂
	PuzzleFactory* factory;

	// 模组对象
	PuzzleMod* mod;
};
