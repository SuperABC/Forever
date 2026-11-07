#pragma once

#include "common/mod_state.h"
#include "player/canvas.h"
#include "common/handle.h"
#include "common/json.h"

#include <string>


// 设计见puzzle_mod.md。Canvas由UE侧宿主创建并定好尺寸,Init只接收已就绪的Canvas*。
class PuzzleMod {
public:
	PuzzleMod() = default;
	virtual ~PuzzleMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 剧情脚本start_puzzle的param动态启动参数,Init之前调一次,默认忽略;不同于config.json的静态args,不进存档,见puzzle_mod.md。
	virtual void SetParam(const std::string& param) {}

	// 小游戏开始时调用一次;canvas已定好尺寸,按GetWidth()/GetHeight()布局初始状态,不持有所有权。
	virtual void Init(Canvas* canvas, PostHandle* post) = 0;

	// 每帧调用:读取canvas里宿主注入的键鼠事件、推进逻辑、画到canvas上;ms为距上一帧毫秒数。
	// 返回nullptr继续;非空=这局结束,指针是结果(root必须是object),由实例自己持有,Core只读不改不释放。
	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) = 0;

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
