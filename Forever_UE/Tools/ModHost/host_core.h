#pragma once

#include "player/canvas.h"
#include "player/puzzle_mod.h"
#include "player/app_mod.h"
#include "common/handle.h"
#include "common/json.h"

#include <string>
#include <vector>


class Puzzle;
class Phone;

// 不依赖Core/UE的puzzle/app验证宿主：加载mod DLL(原生)或直接使用静态注册(wasm)，
// 创建一个puzzle或一个Phone(和游戏里UPuzzleWidget/UPhoneWidget同样的驱动方式)，
// 按帧驱动并接收输入。同一份代码被ModHost.exe(原生)和ModWeb(wasm)共用。
class StubPost : public PostHandle {
public:
	StubPost();
	virtual void Post(const JsonValue& request) override;
	virtual const JsonValue& GetResult() const override { return result; }

	// "resource path"查询的根目录：在这个目录下递归按文件名找(和Core的Config::GetResourceFilePath
	// 同样的语义)。原生默认Forever_UE/Resource，网页版是内嵌虚拟文件系统里的/res。
	static std::string& ResourceRoot();

private:
	JsonValue result;
};

class ModHost {
public:
	ModHost();
	~ModHost();

	// 原生：LoadLibrary一个mod DLL并把它的puzzle/app注册进Registry的工厂。返回注册的id数。
	int LoadDll(const std::string& path, std::string& error);

	// 把id登记成"启用"(Factory要求id在modArgs里才算启用)，必须在注册前调用。
	static void EnableIds(const std::vector<std::string>& puzzles, const std::vector<std::string>& apps);

	std::vector<std::string> PuzzleIds() const;
	std::vector<std::string> AppIds() const;

	void StartPuzzle(const std::string& id, const std::string& param, int width, int height);
	void StartPhone(int width, int height);
	void Stop();

	void Resize(int width, int height);
	// 推进一帧；返回puzzle是否已结束(phone模式恒为false)。
	bool Frame(int ms);

	void KeyDown(int code);
	void KeyUp(int code);
	void MouseMove(int x, int y);
	void MouseButton(int button, bool down);
	void MouseWheel(int delta);

	Canvas& GetCanvas() { return canvas; }
	bool IsPhone() const { return phone != nullptr; }
	bool IsRunning() const { return puzzle != nullptr || phone != nullptr; }
	bool PuzzleFinished() const { return finished; }

private:
	Canvas canvas;
	StubPost post;
	Puzzle* puzzle = nullptr;
	Phone* phone = nullptr;
	bool finished = false;
	std::vector<std::string> enabledPuzzles, enabledApps;
};

// 手机界面布局(和Core/player/phone.cpp的RecomputeLayout同样的公式)，脚本用来换算点击坐标。
struct PhoneLayout {
	static void BarButton(int width, int height, int index, int& x, int& y); // 0=Back 1=Home 2=Tasks
	static void HomeIcon(int width, int height, int index, int& x, int& y);
	static void TaskThumb(int width, int height, int index, int& x, int& y);
	static void TaskClose(int width, int height, int index, int& x, int& y);
};
