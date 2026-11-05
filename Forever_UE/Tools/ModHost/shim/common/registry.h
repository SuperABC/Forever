#pragma once

// ModHost / ModWeb 专用垫片：Core/player/phone.cpp、puzzle.cpp只用到
// Registry::Get().GetAppFactory()/GetPuzzleFactory()。这里提供只含这两个工厂的最小
// 版本，让这些Core源文件原样编译进独立验证程序，而不需要链接整个Core。
// 注意：include路径里必须让本目录排在Source/Core之前。

#include "player/app_factory.h"
#include "player/puzzle_factory.h"


class Registry {
public:
	static Registry& Get() {
		static Registry instance;
		return instance;
	}

	AppFactory& GetAppFactory() { return appFactory; }
	PuzzleFactory& GetPuzzleFactory() { return puzzleFactory; }

private:
	Registry() = default;

	AppFactory appFactory;
	PuzzleFactory puzzleFactory;
};
