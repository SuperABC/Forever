// ModHost：不依赖Core/UE的puzzle/app本地验证程序，用法见README.md。

#include "host_core.h"
#include "png_writer.h"

#define NOMINMAX
#include <windows.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>


using namespace std;

namespace {
	struct Options {
		vector<string> dlls;
		string puzzleId;
		string param;
		bool phone = false;
		bool window = false;
		bool list = false;
		string script;
		string outDir = ".";
		string resourceDir;
		int width = 480, height = 640;
	};

	int KeyFromName(const string& name) {
		if (name == "left") return KEY_LEFT;
		if (name == "right") return KEY_RIGHT;
		if (name == "up") return KEY_UP;
		if (name == "down") return KEY_DOWN;
		if (name == "esc" || name == "escape") return KEY_ESCAPE;
		if (name == "enter") return KEY_ENTER;
		if (name == "tab") return KEY_TAB;
		if (name == "backspace") return KEY_BACKSPACE;
		if (name == "space") return ' ';
		if (name.size() == 1) return name[0];
		return 0;
	}

	int KeyFromVirtualKey(WPARAM vk) {
		switch (vk) {
		case VK_LEFT: return KEY_LEFT;
		case VK_RIGHT: return KEY_RIGHT;
		case VK_UP: return KEY_UP;
		case VK_DOWN: return KEY_DOWN;
		case VK_ESCAPE: return KEY_ESCAPE;
		case VK_RETURN: return KEY_ENTER;
		case VK_TAB: return KEY_TAB;
		case VK_BACK: return KEY_BACKSPACE;
		case VK_SPACE: return ' ';
		}
		if (vk >= 'A' && vk <= 'Z') return static_cast<int>(vk - 'A' + 'a');
		if (vk >= '0' && vk <= '9') return static_cast<int>(vk);
		return 0;
	}

	// ---------------------------------------------------------------- 窗口模式

	ModHost* g_host = nullptr;
	int g_pendingWidth = 0, g_pendingHeight = 0;
	bool g_quit = false;

	LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		switch (msg) {
		case WM_CLOSE: g_quit = true; return 0;
		case WM_DESTROY: PostQuitMessage(0); return 0;
		case WM_SIZE:
			g_pendingWidth = LOWORD(lparam);
			g_pendingHeight = HIWORD(lparam);
			return 0;
		case WM_KEYDOWN: case WM_SYSKEYDOWN:
			if (!(lparam & (1 << 30))) { // 忽略系统自动重复
				int key = KeyFromVirtualKey(wparam);
				if (key) g_host->KeyDown(key);
			}
			return 0;
		case WM_KEYUP: case WM_SYSKEYUP: {
			int key = KeyFromVirtualKey(wparam);
			if (key) g_host->KeyUp(key);
			return 0;
		}
		case WM_MOUSEMOVE:
			g_host->MouseMove(static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam)));
			return 0;
		case WM_LBUTTONDOWN: case WM_LBUTTONUP:
			g_host->MouseMove(static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam)));
			g_host->MouseButton(MOUSE_LEFT, msg == WM_LBUTTONDOWN);
			if (msg == WM_LBUTTONDOWN) SetCapture(hwnd); else ReleaseCapture();
			return 0;
		case WM_RBUTTONDOWN: case WM_RBUTTONUP:
			g_host->MouseMove(static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam)));
			g_host->MouseButton(MOUSE_RIGHT, msg == WM_RBUTTONDOWN);
			return 0;
		case WM_MOUSEWHEEL:
			g_host->MouseWheel(GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA);
			return 0;
		case WM_ERASEBKGND: return 1;
		}
		return DefWindowProcW(hwnd, msg, wparam, lparam);
	}

	void Present(HWND hwnd, ModHost& host) {
		Canvas& canvas = host.GetCanvas();
		HDC dc = GetDC(hwnd);
		BITMAPINFO bi = {};
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = canvas.GetWidth();
		bi.bmiHeader.biHeight = -canvas.GetHeight(); // 负数=自上而下
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;
		SetDIBitsToDevice(dc, 0, 0, canvas.GetWidth(), canvas.GetHeight(), 0, 0, 0, canvas.GetHeight(),
			canvas.GetData(), &bi, DIB_RGB_COLORS);
		ReleaseDC(hwnd, dc);
	}

	int RunWindow(ModHost& host, const Options& options) {
		g_host = &host;
		WNDCLASSW wc = {};
		wc.lpfnWndProc = WindowProc;
		wc.hInstance = GetModuleHandleW(nullptr);
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.lpszClassName = L"ModHostWindow";
		RegisterClassW(&wc);

		RECT rect = { 0, 0, options.width, options.height };
		AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
		HWND hwnd = CreateWindowW(wc.lpszClassName, L"ModHost", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
			nullptr, nullptr, wc.hInstance, nullptr);
		if (!hwnd) return 1;

		auto last = chrono::steady_clock::now();
		while (!g_quit) {
			MSG msg;
			while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
				if (msg.message == WM_QUIT) g_quit = true;
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}

			if (g_pendingWidth > 0 && g_pendingHeight > 0) {
				host.Resize(g_pendingWidth, g_pendingHeight);
				g_pendingWidth = g_pendingHeight = 0;
			}

			auto now = chrono::steady_clock::now();
			int ms = static_cast<int>(chrono::duration_cast<chrono::milliseconds>(now - last).count());
			if (ms < 1) { Sleep(1); continue; }
			last = now;

			if (host.Frame(min(ms, 100))) g_quit = true; // puzzle自己结束
			Present(hwnd, host);
			Sleep(4);
		}
		return 0;
	}

	// ---------------------------------------------------------------- 脚本模式

	class ScriptRunner {
	public:
		ScriptRunner(ModHost& host, const string& outDir) : host(host), outDir(outDir) {}

		bool RunFile(const string& path) {
			ifstream file(path);
			if (!file) { fprintf(stderr, "cannot open script %s\n", path.c_str()); return false; }
			string line;
			int lineNo = 0;
			while (getline(file, line)) {
				lineNo++;
				size_t hash = line.find('#');
				if (hash != string::npos) line = line.substr(0, hash);
				istringstream in(line);
				string cmd;
				if (!(in >> cmd)) continue;
				if (!Exec(cmd, in)) {
					fprintf(stderr, "script error at line %d: %s\n", lineNo, line.c_str());
					return false;
				}
			}
			return true;
		}

	private:
		static constexpr int kStep = 16;

		void Step(int count = 1) { for (int i = 0; i < count; i++) host.Frame(kStep); }
		void Wait(int ms) { for (int t = 0; t < ms; t += kStep) host.Frame(kStep); }

		void Click(int x, int y) {
			host.MouseMove(x, y);
			host.MouseButton(MOUSE_LEFT, true);
			Step(2);
			host.MouseButton(MOUSE_LEFT, false);
			Step(2);
		}

		bool Exec(const string& cmd, istringstream& in) {
			int w = host.GetCanvas().GetWidth(), h = host.GetCanvas().GetHeight();
			if (cmd == "wait") { int ms = 0; in >> ms; Wait(ms); return true; }
			if (cmd == "frame") { int n = 1; in >> n; Step(n); return true; }
			if (cmd == "key" || cmd == "keydown" || cmd == "keyup") {
				string name; in >> name;
				int key = KeyFromName(name);
				if (!key) return false;
				if (cmd != "keyup") host.KeyDown(key);
				if (cmd == "key") { Step(2); }
				if (cmd != "keydown") host.KeyUp(key);
				Step(2);
				return true;
			}
			if (cmd == "click") { int x, y; if (!(in >> x >> y)) return false; Click(x, y); return true; }
			if (cmd == "down") { int x, y; if (!(in >> x >> y)) return false; host.MouseMove(x, y); host.MouseButton(MOUSE_LEFT, true); Step(2); return true; }
			if (cmd == "up") { host.MouseButton(MOUSE_LEFT, false); Step(2); return true; }
			if (cmd == "move") { int x, y; if (!(in >> x >> y)) return false; host.MouseMove(x, y); Step(1); return true; }
			if (cmd == "drag") {
				int x1, y1, x2, y2;
				if (!(in >> x1 >> y1 >> x2 >> y2)) return false;
				host.MouseMove(x1, y1);
				host.MouseButton(MOUSE_LEFT, true);
				Step(2);
				for (int i = 1; i <= 8; i++) { host.MouseMove(x1 + (x2 - x1) * i / 8, y1 + (y2 - y1) * i / 8); Step(1); }
				host.MouseButton(MOUSE_LEFT, false);
				Step(2);
				return true;
			}
			if (cmd == "bar") { // 手机底部三按钮：bar back|home|tasks
				string name; in >> name;
				int index = name == "back" ? 0 : name == "home" ? 1 : name == "tasks" ? 2 : -1;
				if (index < 0) return false;
				int x, y; PhoneLayout::BarButton(w, h, index, x, y); Click(x, y); return true;
			}
			if (cmd == "open_app") { int n; if (!(in >> n)) return false; int x, y; PhoneLayout::HomeIcon(w, h, n, x, y); Click(x, y); return true; }
			if (cmd == "tap_task") { int n; if (!(in >> n)) return false; int x, y; PhoneLayout::TaskThumb(w, h, n, x, y); Click(x, y); return true; }
			if (cmd == "close_task") { int n; if (!(in >> n)) return false; int x, y; PhoneLayout::TaskClose(w, h, n, x, y); Click(x, y); return true; }
			if (cmd == "image") { // image <文件名> x y [w h]：经"resource path"加载并用PutBitmap画到当前画布(测试用)
				string name; int x = 0, y = 0, iw = 0, ih = 0;
				if (!(in >> name >> x >> y)) return false;
				in >> iw >> ih;
				Bitmap bitmap;
				StubPost post;
				if (!LoadModImage(&post, name, bitmap)) { fprintf(stderr, "LoadModImage failed: %s\n", name.c_str()); return false; }
				printf("image %s %dx%d\n", name.c_str(), bitmap.width, bitmap.height);
				host.GetCanvas().SetAlpha(1.f);
				host.GetCanvas().PutBitmap(bitmap, x, y, iw, ih);
				return true;
			}
			if (cmd == "resize") { int nw, nh; if (!(in >> nw >> nh)) return false; host.Resize(nw, nh); Step(2); return true; }
			if (cmd == "shot") {
				string name; in >> name;
				Canvas& canvas = host.GetCanvas();
				string path = outDir + "/" + name + ".png";
				if (!png::Write(path, canvas.GetData(), canvas.GetWidth(), canvas.GetHeight())) {
					fprintf(stderr, "cannot write %s\n", path.c_str());
					return false;
				}
				printf("shot %s (%dx%d)\n", path.c_str(), canvas.GetWidth(), canvas.GetHeight());
				return true;
			}
			if (cmd == "expect_finished" || cmd == "expect_running") {
				bool want = cmd == "expect_finished";
				if (host.PuzzleFinished() != want) { fprintf(stderr, "%s failed\n", cmd.c_str()); return false; }
				printf("%s ok\n", cmd.c_str());
				return true;
			}
			fprintf(stderr, "unknown command: %s\n", cmd.c_str());
			return false;
		}

		ModHost& host;
		string outDir;
	};

	void PrintUsage() {
		printf("ModHost --dll <mod.dll> [--dll ...] (--puzzle <id> [--param <text>] | --phone | --list)\n"
			"        [--size WxH] (--window | --script <steps.txt> [--out <dir>])\n");
	}
}

int main(int argc, char** argv) {
	Options options;
	for (int i = 1; i < argc; i++) {
		string arg = argv[i];
		auto next = [&]() -> string { return i + 1 < argc ? argv[++i] : string(); };
		if (arg == "--dll") options.dlls.push_back(next());
		else if (arg == "--puzzle") options.puzzleId = next();
		else if (arg == "--param") options.param = next();
		else if (arg == "--phone") options.phone = true;
		else if (arg == "--window") options.window = true;
		else if (arg == "--list") options.list = true;
		else if (arg == "--script") options.script = next();
		else if (arg == "--out") options.outDir = next();
		else if (arg == "--resource") options.resourceDir = next();
		else if (arg == "--size") {
			string size = next();
			if (sscanf_s(size.c_str(), "%dx%d", &options.width, &options.height) != 2) { PrintUsage(); return 2; }
		}
		else { PrintUsage(); return 2; }
	}

	if (options.dlls.empty()) {
		// 默认和exe同目录的Basic.dll
		char exe[MAX_PATH];
		GetModuleFileNameA(nullptr, exe, MAX_PATH);
		string dir = exe;
		dir = dir.substr(0, dir.find_last_of("\\/") + 1);
		options.dlls.push_back(dir + "Basic.dll");
	}

	if (options.resourceDir.empty()) {
		// 默认：exe在Forever_UE/x64/Release，Resource在Forever_UE/Resource
		char exe[MAX_PATH];
		GetModuleFileNameA(nullptr, exe, MAX_PATH);
		string dir = exe;
		dir = dir.substr(0, dir.find_last_of("\\/") + 1);
		options.resourceDir = dir + "../../Resource";
	}
	StubPost::ResourceRoot() = options.resourceDir;

	ModHost host;
	for (const string& dll : options.dlls) {
		string error;
		int count = host.LoadDll(dll, error);
		if (count == 0 && !error.empty()) { fprintf(stderr, "%s\n", error.c_str()); return 1; }
		printf("loaded %s (%d mods)\n", dll.c_str(), count);
	}

	if (options.list) {
		for (const string& id : host.PuzzleIds()) printf("puzzle %s\n", id.c_str());
		for (const string& id : host.AppIds()) printf("app %s\n", id.c_str());
		return 0;
	}

	try {
		if (options.phone) host.StartPhone(options.width, options.height);
		else if (!options.puzzleId.empty()) host.StartPuzzle(options.puzzleId, options.param, options.width, options.height);
		else { PrintUsage(); return 2; }
	}
	catch (const exception& e) {
		fprintf(stderr, "start failed: %s\n", e.what());
		return 1;
	}

	if (options.window) return RunWindow(host, options);

	if (options.script.empty()) { PrintUsage(); return 2; }
	CreateDirectoryA(options.outDir.c_str(), nullptr);
	host.Frame(16);
	ScriptRunner runner(host, options.outDir);
	return runner.RunFile(options.script) ? 0 : 1;
}
