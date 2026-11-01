#pragma once

#include "CoreMinimal.h"

// 载具/人物Controller共用的"指令列表+当前执行到哪条"：Controller自己决定每条指令怎么执行，
// 这里只负责排队。指令全部执行完（或Clear）后内部数组复位，Current()返回nullptr。
// 注意：Advance()/Clear()之后之前拿到的Current()指针失效。
template <typename TCommand>
class TCommandQueue
{
public:
	void Push(const TCommand& command) { commands.Add(command); }

	void Clear()
	{
		commands.Reset();
		currentIndex = 0;
	}

	bool IsEmpty() const { return currentIndex >= commands.Num(); }

	TCommand* Current() { return IsEmpty() ? nullptr : &commands[currentIndex]; }
	const TCommand* Current() const { return IsEmpty() ? nullptr : &commands[currentIndex]; }

	// 当前指令做完，进入下一条；没有下一条时复位。
	void Advance()
	{
		if (!IsEmpty()) currentIndex++;
		if (IsEmpty()) Clear();
	}

	// 还没执行完的指令条数（含当前这条）。
	int32 Num() const { return commands.Num() - currentIndex; }

private:
	TArray<TCommand> commands;
	int32 currentIndex = 0;
};
