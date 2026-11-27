#include "host_core.h"

#include "player/puzzle.h"
#include "player/phone.h"
#include "common/registry.h"
#include "story/event.h"
#include "common/utf8_path.h"

#include <algorithm>
#include <filesystem>
#include <unordered_map>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif


using namespace std;

StubPost::StubPost() : result(DATA_OBJECT) {
	result["result"] = "fail";
}

map<string, StubPost::FakeTrack>& StubPost::FakeTracks() {
	static map<string, FakeTrack> tracks;
	return tracks;
}

string& StubPost::ResourceRoot() {
	static string root;
	return root;
}

void StubPost::Post(const JsonValue& request) {
	result = JsonValue(DATA_OBJECT);

	// "resource path"：request["name"]是带扩展名的文件名，在ResourceRoot下递归找同名文件，返回路径。
	if (request.IsObject() && request["post"].AsString() == "resource path") {
		string name = request["name"].AsString();
		error_code ec;
		if (!ResourceRoot().empty()) {
			for (const auto& entry : filesystem::recursive_directory_iterator(
					ResourceRoot(), filesystem::directory_options::skip_permission_denied, ec)) {
				if (entry.is_regular_file() && entry.path().filename().string() == name) {
					string path = entry.path().generic_string();
					result["result"] = "success";
					result["path"] = path;
					return;
				}
			}
		}
		result["result"] = "fail";
		result["msg"] = "resource not found: " + name;
		return;
	}

	// "list resource files"：和Core的Config::ListResourceFiles一样，列ResourceRoot下所有叫folder的文件夹里(不递归)
	// 扩展名匹配(不分大小写)的文件名，UTF-8，排序去重。Music app的曲库界面靠它在ModHost里有东西可看。
	if (request.IsObject() && request["post"].AsString() == "list resource files") {
		const string folder = request["folder"].AsString();
		vector<string> extensions;
		for (unsigned int i = 0; request["extensions"].IsArray() && i < request["extensions"].size(); i++) {
			string extension = request["extensions"][i].AsString();
			if (!extension.empty() && extension[0] == '.') extension.erase(0, 1);
			transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
			extensions.push_back(extension);
		}
		vector<string> names;
		error_code ec;
		if (!ResourceRoot().empty() && !folder.empty()) {
			for (const auto& entry : filesystem::recursive_directory_iterator(
					ResourceRoot(), filesystem::directory_options::skip_permission_denied, ec)) {
				if (!entry.is_regular_file() || PathToUtf8(entry.path().parent_path().filename()) != folder) continue;
				string extension = PathToUtf8(entry.path().extension());
				if (!extension.empty() && extension[0] == '.') extension.erase(0, 1);
				transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
				if (!extensions.empty() && find(extensions.begin(), extensions.end(), extension) == extensions.end()) continue;
				names.push_back(PathToUtf8(entry.path().filename()));
			}
		}
		sort(names.begin(), names.end());
		names.erase(unique(names.begin(), names.end()), names.end());
		JsonValue files(DATA_ARRAY);
		for (const string& name : names) files.append(JsonValue(name));
		result["result"] = "success";
		result["files"] = files;
		return;
	}

	if (request.IsObject() && request["post"].AsString() == "music") {
		const string op = request["op"].AsString(), track = request["track"].AsString();
		auto& tracks = FakeTracks();
		auto found = tracks.find(track);
		bool ok = true;
		if (op == "play" && !track.empty() && !request["asset"].AsString().empty()) {
			FakeTrack entry;
			entry.asset = request["asset"].AsString();
			entry.loop = request["loop"].IsBool() && request["loop"].AsBool();
			tracks[track] = entry;
		}
		else if (found == tracks.end()) ok = false;
		else if (op == "pause") found->second.paused = true;
		else if (op == "resume") found->second.paused = false;
		else if (op == "stop") tracks.erase(found);
		else if (op == "seek") found->second.position = max(0.0, min(180.0, request["position"].IsNumeric() ? request["position"].AsDouble() : 0.0));
		else ok = false;
		result["result"] = ok ? "success" : "fail";
		return;
	}
	if (request.IsObject() && request["post"].AsString() == "music status") {
		auto found = FakeTracks().find(request["track"].AsString());
		if (found == FakeTracks().end()) { result["result"] = "fail"; return; }
		result["result"] = "success";
		result["asset"] = found->second.asset;
		result["state"] = found->second.paused ? "paused" : "playing";
		result["position"] = found->second.position;
		result["duration"] = 180.0;
		result["loop"] = found->second.loop;
		return;
	}

	// 其余查询一律失败(主屏日期/时间因此不显示)，足够验证puzzle/app本身的逻辑。
	result["result"] = "fail";
	result["msg"] = "post not supported in ModHost.";
}

ModHost::ModHost() {
	canvas.Init(480, 640);
}

ModHost::~ModHost() {
	Stop();
	// 故意不FreeLibrary：进程马上退出，卸载DLL只会带来析构顺序风险。
}

void ModHost::EnableIds(const vector<string>& puzzles, const vector<string>& apps) {
	unordered_map<string, string> puzzleArgs, appArgs;
	for (const string& id : puzzles) puzzleArgs[id] = "";
	for (const string& id : apps) appArgs[id] = "";
	Registry::Get().GetPuzzleFactory().SetModArgs(puzzleArgs);
	Registry::Get().GetAppFactory().SetModArgs(appArgs);
}

int ModHost::LoadDll(const string& path, string& error) {
#ifdef _WIN32
	HMODULE module = LoadLibraryA(path.c_str());
	if (!module) {
		error = "LoadLibrary failed: " + path + " (code " + to_string(GetLastError()) + ")";
		return 0;
	}

	using GetFunc = void* (*)();
	using RegisterPuzzlesFunc = void(*)(PuzzleFactory*);
	using RegisterAppsFunc = void(*)(AppFactory*);

	vector<string> puzzleIds, appIds;
	if (auto get = reinterpret_cast<GetFunc>(GetProcAddress(module, "GetModPuzzles"))) {
		puzzleIds = *static_cast<vector<string>*>(get());
	}
	if (auto get = reinterpret_cast<GetFunc>(GetProcAddress(module, "GetModApps"))) {
		appIds = *static_cast<vector<string>*>(get());
	}

	// 合并已有的id再整体SetModArgs，这样多次LoadDll不会互相覆盖启用表。
	enabledPuzzles.insert(enabledPuzzles.end(), puzzleIds.begin(), puzzleIds.end());
	enabledApps.insert(enabledApps.end(), appIds.begin(), appIds.end());
	EnableIds(enabledPuzzles, enabledApps);

	int count = 0;
	if (auto reg = reinterpret_cast<RegisterPuzzlesFunc>(GetProcAddress(module, "RegisterModPuzzles"))) {
		reg(&Registry::Get().GetPuzzleFactory());
		count += static_cast<int>(puzzleIds.size());
	}
	if (auto reg = reinterpret_cast<RegisterAppsFunc>(GetProcAddress(module, "RegisterModApps"))) {
		reg(&Registry::Get().GetAppFactory());
		count += static_cast<int>(appIds.size());
	}
	return count;
#else
	error = "LoadDll not supported on this platform.";
	return 0;
#endif
}

vector<string> ModHost::PuzzleIds() const {
	return Registry::Get().GetPuzzleFactory().GetRegisteredIds();
}

vector<string> ModHost::AppIds() const {
	return Registry::Get().GetAppFactory().GetRegisteredIds();
}

void ModHost::Stop() {
	delete puzzle;
	puzzle = nullptr;
	delete phone;
	phone = nullptr;
	finished = false;
	resultText.clear();
	canvas.SetResizeFunc(nullptr, nullptr);
	canvas.ClearKeyBuffer();
	canvas.ClearMouseBuffer();
}

void ModHost::StartPuzzle(const string& id, const string& param, int width, int height) {
	Stop();
	canvas.Resize(width, height);
	puzzle = new Puzzle(&Registry::Get().GetPuzzleFactory(), id);
	puzzle->SetParam(param);
	puzzle->Init(&canvas, &post);
}

void ModHost::StartPhone(int width, int height) {
	Stop();
	canvas.Resize(width, height);
	phone = new Phone();
	phone->Init(width, height);
}

void ModHost::Resize(int width, int height) {
	if (width <= 0 || height <= 0) return;
	canvas.Resize(width, height);
	if (phone) phone->Resize(width, height);
}

bool ModHost::Frame(int ms) {
	if (puzzle && !finished) {
		JsonValue* result = puzzle->Loop(&canvas, ms, &post);
		if (result) {
			finished = true;
			PuzzleResultEvent event;
			if (!event.SetFromJson(*result)) resultText = "<invalid>";
			for (const auto& field : event.GetFields()) {
				// utility.cpp(ToString所在)依赖windows.h，网页版不编，这里自己转字符串
				string text = visit([](const auto& v) -> string {
					if constexpr (is_same_v<decay_t<decltype(v)>, string>) return v;
					else return to_string(v);
				}, field.second);
				resultText += field.first + "=" + text + "\n";
			}
		}
	}
	else if (phone) {
		phone->Loop(&canvas, ms, &post);
	}
	return finished;
}

void ModHost::KeyDown(int code) { canvas.PushKey(code); }
void ModHost::KeyUp(int code) { canvas.PushKey(code | KEY_RELEASED_FLAG); }
void ModHost::MouseMove(int x, int y) { canvas.SetMousePos(x, y); }
void ModHost::MouseButton(int button, bool down) { canvas.PushMouseButton(button, down); }
void ModHost::MouseWheel(int delta) { canvas.PushMouseWheel(delta); }

void PhoneLayout::BarButton(int width, int height, int index, int& x, int& y) {
	int barHeight = max(20, height * 625 / 10000);
	x = width * (index * 2 + 1) / 6;
	y = height - barHeight / 2;
}

void PhoneLayout::HomeIcon(int width, int height, int index, int& x, int& y) {
	int cellWidth = width / 3, cellHeight = height * 16 / 100, startY = height * 78 / 1000;
	int col = index % 3, row = index / 3;
	x = col * cellWidth + cellWidth / 2;
	y = startY + row * cellHeight + cellHeight / 3;
}

void PhoneLayout::TaskThumb(int width, int height, int index, int& x, int& y) {
	int cellWidth = width / 2, cellHeight = height * 3 / 10, startY = height * 78 / 1000;
	int col = index % 2, row = index / 2;
	x = col * cellWidth + cellWidth / 2;
	y = startY + row * cellHeight + cellHeight * 8 / 10 / 2;
}

void PhoneLayout::TaskClose(int width, int height, int index, int& x, int& y) {
	int cellWidth = width / 2, cellHeight = height * 3 / 10, startY = height * 78 / 1000;
	int thumbWidth = cellWidth / 2;
	int col = index % 2, row = index / 2;
	int thumbX = col * cellWidth + (cellWidth - thumbWidth) / 2;
	int closeSize = max(12, height / 25);
	x = thumbX + thumbWidth - closeSize / 2;
	y = startY + row * cellHeight + closeSize / 2;
}
