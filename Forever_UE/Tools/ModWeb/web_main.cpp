// ModWeb：把真实的C++ puzzle/app(Basic的puzzle_basic.cpp/app_basic.cpp)+游戏里的Phone外壳
// 编译成WebAssembly，网页只负责显示Canvas像素和转发鼠标/触摸/键盘事件。构建见build_web.bat。

#include <emscripten.h>

#include "host_core.h"
#include "common/registry.h"
#include "player/puzzle_basic.h"
#include "player/puzzle_poker.h"
#include "player/puzzle_burger.h"
#include "player/puzzle_miner.h"
#include "player/puzzle_tower.h"
#include "player/puzzle_chase.h"
#include "player/puzzle_rhythm.h"
#include "player/puzzle_tubes.h"
#include "player/puzzle_coins.h"
#include "player/puzzle_dance.h"
#include "player/puzzle_dodge.h"
#include "player/app_basic.h"

#include <string>
#include <vector>


using namespace std;

// Dependence/common/utility.cpp依赖<windows.h>，网页版不编它，这里只提供mod用到的GetRandom。
int GetRandom(int range) {
	if (range <= 0) return 0;
	int value = static_cast<int>(emscripten_random() * range);
	return value >= range ? range - 1 : value;
}

namespace {
	ModHost* host = nullptr;
	vector<uint8_t> rgba;
	string listing;
}

extern "C" {

// 注册内置mod(网页版没有DLL加载，直接静态注册，等价于Basic.dll里RegisterModPuzzles/Apps)。
EMSCRIPTEN_KEEPALIVE void web_setup() {
	if (host) return;
	host = new ModHost();
	StubPost::ResourceRoot() = "/res"; // build_web.bat用--embed-file把Resource/Mod/Basic内嵌到这里

	ModHost::EnableIds({ TetrisPuzzle::GetId(), AnyKeyPuzzle::GetId(), DodgePuzzle::GetId(), DancePuzzle::GetId(), CoinsPuzzle::GetId(), TubesPuzzle::GetId(), RhythmPuzzle::GetId(), ChasePuzzle::GetId(), TowerPuzzle::GetId(), MinerPuzzle::GetId(), BurgerPuzzle::GetId(), PokerPuzzle::GetId() }, { BounceApp::GetId(), NotesApp::GetId() });
	PuzzleFactory& puzzles = Registry::Get().GetPuzzleFactory();
	puzzles.RegisterPuzzle(TetrisPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new TetrisPuzzle(); },
		[](PuzzleMod* m) { delete m; });
	puzzles.RegisterPuzzle(AnyKeyPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new AnyKeyPuzzle(); },
		[](PuzzleMod* m) { delete m; });
	puzzles.RegisterPuzzle(DodgePuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new DodgePuzzle(); },
		[](PuzzleMod* m) { delete m; });
	puzzles.RegisterPuzzle(DancePuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new DancePuzzle(); },
		[](PuzzleMod* m) { delete m; });
	puzzles.RegisterPuzzle(CoinsPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new CoinsPuzzle(); },
		[](PuzzleMod* m) { delete m; });
	puzzles.RegisterPuzzle(TubesPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new TubesPuzzle(); },
		[](PuzzleMod* m) { delete m; });
	puzzles.RegisterPuzzle(RhythmPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new RhythmPuzzle(); },
		[](PuzzleMod* m) { delete m; });
	puzzles.RegisterPuzzle(ChasePuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new ChasePuzzle(); },
		[](PuzzleMod* m) { delete m; });
	puzzles.RegisterPuzzle(TowerPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new TowerPuzzle(); },
		[](PuzzleMod* m) { delete m; });
	puzzles.RegisterPuzzle(MinerPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new MinerPuzzle(); },
		[](PuzzleMod* m) { delete m; });
	puzzles.RegisterPuzzle(BurgerPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new BurgerPuzzle(); },
		[](PuzzleMod* m) { delete m; });
	puzzles.RegisterPuzzle(PokerPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new PokerPuzzle(); },
		[](PuzzleMod* m) { delete m; });
	AppFactory& apps = Registry::Get().GetAppFactory();
	apps.RegisterApp(BounceApp::GetId(),
		[](const std::string&) -> AppMod* { return new BounceApp(); },
		[](AppMod* m) { delete m; });
	apps.RegisterApp(NotesApp::GetId(),
		[](const std::string&) -> AppMod* { return new NotesApp(); },
		[](AppMod* m) { delete m; });
}

// "puzzle:<id>\n"和"app:<id>\n"逐行列出已注册的mod。
EMSCRIPTEN_KEEPALIVE const char* web_list() {
	listing.clear();
	for (const string& id : host->PuzzleIds()) listing += "puzzle:" + id + "\n";
	for (const string& id : host->AppIds()) listing += "app:" + id + "\n";
	return listing.c_str();
}

EMSCRIPTEN_KEEPALIVE int web_start_puzzle(const char* id, int width, int height) {
	try {
		host->StartPuzzle(id, "", width, height);
		return 1;
	}
	catch (...) {
		return 0;
	}
}

EMSCRIPTEN_KEEPALIVE int web_start_phone(int width, int height) {
	try {
		host->StartPhone(width, height);
		return 1;
	}
	catch (...) {
		return 0;
	}
}

EMSCRIPTEN_KEEPALIVE void web_stop() { host->Stop(); }
EMSCRIPTEN_KEEPALIVE void web_resize(int width, int height) { host->Resize(width, height); }

// 推进一帧，返回puzzle是否已结束。
EMSCRIPTEN_KEEPALIVE int web_frame(int ms) { return host->Frame(ms) ? 1 : 0; }

EMSCRIPTEN_KEEPALIVE void web_key(int code, int down) { if (down) host->KeyDown(code); else host->KeyUp(code); }
EMSCRIPTEN_KEEPALIVE void web_mouse_move(int x, int y) { host->MouseMove(x, y); }
EMSCRIPTEN_KEEPALIVE void web_mouse_button(int button, int down) { host->MouseButton(button, down != 0); }
EMSCRIPTEN_KEEPALIVE void web_wheel(int delta) { host->MouseWheel(delta); }

EMSCRIPTEN_KEEPALIVE int web_width() { return host->GetCanvas().GetWidth(); }
EMSCRIPTEN_KEEPALIVE int web_height() { return host->GetCanvas().GetHeight(); }

// Canvas是BGRA，ImageData要RGBA，这里转好再给JS。
EMSCRIPTEN_KEEPALIVE const uint8_t* web_pixels() {
	Canvas& canvas = host->GetCanvas();
	size_t size = static_cast<size_t>(canvas.GetDataSize());
	rgba.resize(size);
	const uint8_t* src = canvas.GetData();
	for (size_t i = 0; i + 3 < size; i += 4) {
		rgba[i] = src[i + 2]; rgba[i + 1] = src[i + 1]; rgba[i + 2] = src[i]; rgba[i + 3] = 255;
	}
	return rgba.data();
}

}
