#pragma once

#include "player/canvas.h"
#include "common/handle.h"

#include <string>


// PuzzleMod：小游戏Mod基类。GetType()/GetName()是阶段3就有的骨架接口；Init/Loop这次
// 补上，对应"小游戏进入时初始化一次"/"每帧更新一次"这两件事——形状参考老工程
// Core/player/puzzle.h的Puzzle::Init/Loop，但这次画布(Canvas)的创建和定尺寸交给UE侧
// 宿主(UPuzzleWidget)来做，Init只接收一个已经Init()好尺寸的Canvas*，不用再单独传
// width/height（canvas->GetWidth()/GetHeight()已经够用），见Core/player/puzzle.md。
class PuzzleMod {
public:
	PuzzleMod() = default;
	virtual ~PuzzleMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	/*
	* 小游戏开始时调用一次；canvas已经由宿主Init()好尺寸，这里只需要按
	* canvas->GetWidth()/GetHeight()布局初始状态（比如俄罗斯方块按画布尺寸算好每个格子
	* 多大、棋盘摆在画布什么位置）。
	* @canvas: 画布，不持有所有权
	* @post: 向Core发起查询的句柄
	*/
	virtual void Init(Canvas* canvas, PostHandle* post) = 0;

	/*
	* 每帧调用一次：读取canvas里宿主注入的键鼠事件、推进小游戏自身逻辑、把这一帧的画面画到
	* canvas上。
	* @canvas: 画布，不持有所有权
	* @ms: 距上一帧的毫秒数，用于推进重力计时器等
	* @post: 向Core发起查询的句柄
	* @return: 0表示继续，非0表示这一局小游戏结束
	*/
	virtual int Loop(Canvas* canvas, int ms, PostHandle* post) = 0;
};
