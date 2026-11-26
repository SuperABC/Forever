#include "suites.h"

#include "test_framework.h"

#include "common/config.h"
#include "common/error.h"
#include "common/json.h"
#include "common/registry.h"
#include "common/utility.h"
#include "industry/industry.h"
#include "industry/product.h"
#include "map/building.h"
#include "map/map.h"
#include "player/canvas.h"
#include "player/phone.h"
#include "player/player.h"
#include "player/puzzle.h"
#include "polit/polit.h"
#include "populace/populace.h"
#include "society/society.h"
#include "story/event.h"
#include "story/expression.h"
#include "story/script.h"
#include "story/story.h"
#include "story/script_mod.h"
#include "traffic/vehicle.h"
#include "traffic/vehicle_factory.h"
#include "traffic/vehicle_mod.h"
#include "story/script_factory.h"
#include "traffic/route.h"
#include "traffic/traffic.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>


using namespace std;

// 套件fault：故障注入。异常体系的规则(Dependence/common/error.md)在这里逐条验证：
//   - 三类异常各自报出正确的类别、消息、抛出位置；不是ExceptionBase的异常按"当时是不是在mod里"归类。
//   - ErrorScope：正常返回不留痕迹；异常穿过时按从里到外的顺序记进报告；过期的轨迹不会混进下一份报告。
//   - GuardData只接数据错误，别的照常往外抛。
//   - 坏的config/script/layout/表达式都是数据错误，报告里带文件；读坏config之后配置是空的而不是半份。
//   - mod dll里抛出的异常(标准库异常、Dependence的异常、不认识的类型)能被宿主接住并指名到那个mod。
//   - 出过这些错之后，世界仍然能正常销毁、mod dll仍然能卸干净。
// 前半在父进程里跑(不需要mod)，后半在子进程里跑(世界已经生成、mod已经加载)。
namespace {
	// 假装是一个mod：只要有GetType()就能用MOD_CALL。
	struct FakeMod {
		const char* GetType() const { return "mod_fake"; }
		int Work(bool fail) const {
			if (fail) throw runtime_error("the fake mod failed while working on something");
			return 7;
		}
	};

	bool Contains(const string& text, const string& part) { return text.find(part) != string::npos; }

	string JoinContext(const ErrorReport& report) {
		string joined;
		for (const string& frame : report.context) joined += (joined.empty() ? "" : " < ") + frame;
		return joined;
	}

	void CheckKinds() {
		TestRun& run = TestRun::Get();
		ErrorReport report;

		bool finished = GuardCall([] { THROW_EXCEPTION(DataException, "the data is wrong in a way that needs a long message\n"); }, report);
		run.Check(!finished && report.kind == ErrorKind::Data, "DataException is reported as a data error", ErrorKindName(report.kind));
		run.Check(report.message == "the data is wrong in a way that needs a long message", "the message is kept, without the trailing newline", report.message);
		run.Check(Contains(report.location, "suite_fault.cpp") && Contains(report.location, ":"), "the throw location (file:line) is reported", report.location);
		run.Check(report.context.empty() && report.mod.empty(), "no context is reported when no ErrorScope was active", JoinContext(report));

		GuardCall([] { THROW_EXCEPTION(ModException, "the mod handed over contradictory parameters"); }, report);
		run.Check(report.kind == ErrorKind::Mod, "ModException is reported as a mod error", ErrorKindName(report.kind));
		GuardCall([] { THROW_EXCEPTION(InternalException, "an invariant of the program was broken"); }, report);
		run.Check(report.kind == ErrorKind::Internal, "InternalException is reported as an internal error", ErrorKindName(report.kind));

		// 不是ExceptionBase的异常：不在mod里=内部错误，在mod里=mod错误。
		GuardCall([] { throw runtime_error("a standard library exception with a long enough message"); }, report);
		run.Check(report.kind == ErrorKind::Internal && report.location.empty(), "a std exception outside any mod is an internal error", ErrorKindName(report.kind));
		run.Check(report.message == "a standard library exception with a long enough message", "the std exception keeps its message", report.message);
		GuardCall([] { throw 42; }, report);
		run.Check(report.kind == ErrorKind::Internal && !report.message.empty(), "an exception of an unknown type is still described", report.message);
		const string modId = "mod_under_test";
		GuardCall([&] {
			ErrorScope scope(kErrorScopeMod, modId);
			throw runtime_error("a standard library exception thrown while a mod was running");
		}, report);
		run.Check(report.kind == ErrorKind::Mod && report.mod == modId, "a std exception thrown inside a mod is a mod error naming that mod",
			string(ErrorKindName(report.kind)) + " mod=" + report.mod);
		GuardCall([&] {
			ErrorScope scope(kErrorScopeMod, modId);
			throw 42;
		}, report);
		run.Check(report.kind == ErrorKind::Mod && report.mod == modId, "an unknown exception thrown inside a mod is a mod error naming that mod");
		// ExceptionBase在mod里抛：类别用它自己的，但报告里仍然指名那个mod。
		GuardCall([&] {
			ErrorScope scope(kErrorScopeMod, modId);
			THROW_EXCEPTION(DataException, "a data error noticed while a mod was running");
		}, report);
		run.Check(report.kind == ErrorKind::Data && report.mod == modId, "a DataException thrown inside a mod stays a data error but names the mod",
			string(ErrorKindName(report.kind)) + " mod=" + report.mod);
	}

	void CheckScopes() {
		TestRun& run = TestRun::Get();
		ErrorReport report;
		const string step = "generate.buildings", modId = "building_x", file = "C:/somewhere/a.layout";

		// 从里到外：文件 < mod < 步骤。
		GuardCall([&] {
			ErrorScope stepScope(kErrorScopeStep, step);
			ErrorScope modScope(kErrorScopeMod, modId);
			ErrorScope fileScope(kErrorScopeFile, file);
			THROW_EXCEPTION(DataException, "something inside the file is wrong");
		}, report);
		run.Check(report.context.size() == 3 && report.context[0] == "file " + file && report.context[1] == "mod " + modId && report.context[2] == "step " + step,
			"context frames are reported from the innermost to the outermost", JoinContext(report));
		run.Check(Contains(report.ToLine(), "[data]") && Contains(report.ToLine(), "mod " + modId), "ToLine carries the kind and the context", report.ToLine());
		run.Check(Contains(report.ToText(), modId) && Contains(report.ToText(), step), "ToText names the mod and the step", report.ToText());

		// 正常返回的scope不留痕迹：前面一堆正常的工作之后再出错，报告里只有出错时还开着的那些。
		GuardCall([&] {
			{ ErrorScope finished(kErrorScopeStep, step); }
			{ ErrorScope finished(kErrorScopeMod, modId); }
			ErrorScope open(kErrorScopeFile, file);
			THROW_EXCEPTION(DataException, "only the file scope is open now");
		}, report);
		run.Check(report.context.size() == 1 && report.context[0] == "file " + file && report.mod.empty(),
			"scopes that returned normally leave nothing in the report", JoinContext(report));

		// 过期的轨迹：异常被一个不取报告的catch接住，轨迹留在线程里；下一段工作开始时必须被清掉。
		try {
			ErrorScope stale(kErrorScopeMod, modId);
			throw runtime_error("handled locally by a plain catch that does not ask for a report");
		}
		catch (const exception&) {}
		GuardCall([&] {
			ErrorScope fresh(kErrorScopeStep, step);
			THROW_EXCEPTION(InternalException, "a later, unrelated failure");
		}, report);
		run.Check(report.context.size() == 1 && report.context[0] == "step " + step && report.mod.empty(),
			"a stale trace from an earlier exception does not leak into the next report", JoinContext(report));

		// GuardData：数据错误接住；别的类别原样往外抛，外层的报告里里外两层scope都在。
		bool innerFinished = true;
		ErrorReport inner;
		const bool outerFinished = GuardCall([&] {
			ErrorScope outerScope(kErrorScopeStep, step);
			innerFinished = GuardData([&] {
				ErrorScope innerScope(kErrorScopeScript, "set_value");
				THROW_EXCEPTION(DataException, "a broken expression in one script change");
			}, inner);
		}, report);
		run.Check(outerFinished && !innerFinished && inner.kind == ErrorKind::Data, "GuardData catches a data error and lets the caller go on");
		run.Check(inner.context.size() == 1 && inner.context[0] == "script set_value", "GuardData reports only the frames it unwound through", JoinContext(inner));
		const bool passedThrough = !GuardCall([&] {
			ErrorScope outerScope(kErrorScopeStep, step);
			GuardData([&] {
				ErrorScope innerScope(kErrorScopeScript, "set_value");
				THROW_EXCEPTION(InternalException, "a broken invariant noticed while applying a script change");
			}, inner);
		}, report);
		run.Check(passedThrough && report.kind == ErrorKind::Internal, "GuardData lets an internal error pass through to the outer guard");
		run.Check(report.context.size() == 2 && report.context[0] == "script set_value" && report.context[1] == "step " + step,
			"the frames inside and outside GuardData are both in the outer report", JoinContext(report));

		// MOD_CALL：返回值原样传出来；出错时指名mod。
		FakeMod fake;
		const FakeMod* mod = &fake;
		int value = 0;
		const bool worked = GuardCall([&] { value = MOD_CALL(mod, mod->Work(false)); }, report);
		run.Check(worked && value == 7, "MOD_CALL passes the return value through");
		GuardCall([&] { value = MOD_CALL(mod, mod->Work(true)); }, report);
		run.Check(report.kind == ErrorKind::Mod && report.mod == "mod_fake", "MOD_CALL attributes an exception to the mod it called",
			string(ErrorKindName(report.kind)) + " mod=" + report.mod);
	}

	// TryParseInt/TryParseDouble必须和std::stoi/std::stod逐个输入一致(它们替换的就是原来的try{stoi}catch{})。
	void CheckParsers() {
		TestRun& run = TestRun::Get();
		const vector<string> inputs = { "", " ", "0", "7", "-7", "+7", "  42", "42 ", "42abc", "abc", "007", "2147483647", "2147483648", "-2147483648",
			"-2147483649", "99999999999999999999", "1.5", "-1.5", ".5", "5.", "1e3", "1e", "1e400", "1e-400", "0x10", "inf", "nan", "-", "+", "--1", "1,5", "１２" };
		int intMismatch = 0, doubleMismatch = 0;
		for (const string& input : inputs) {
			int expectedInt = 0, actualInt = 0;
			size_t expectedPos = 0, actualPos = 0;
			bool expectedOk = true;
			try { expectedInt = stoi(input, &expectedPos); }
			catch (const exception&) { expectedOk = false; }
			const bool actualOk = TryParseInt(input, actualInt, &actualPos);
			if (expectedOk != actualOk || (expectedOk && (expectedInt != actualInt || expectedPos != actualPos))) {
				if (intMismatch++ < 5) run.Note("TryParseInt differs from stoi on \"" + input + "\"");
			}
			double expectedDouble = 0, actualDouble = 0;
			expectedOk = true;
			try { expectedDouble = stod(input, &expectedPos); }
			catch (const exception&) { expectedOk = false; }
			const bool actualDoubleOk = TryParseDouble(input, actualDouble, &actualPos);
			// nan != nan，按位比较。
			const bool sameValue = memcmp(&expectedDouble, &actualDouble, sizeof(double)) == 0;
			if (expectedOk != actualDoubleOk || (expectedOk && (!sameValue || expectedPos != actualPos))) {
				if (doubleMismatch++ < 5) run.Note("TryParseDouble differs from stod on \"" + input + "\"");
			}
		}
		run.Check(intMismatch == 0, "TryParseInt agrees with std::stoi on every sample input", to_string(intMismatch) + " differ");
		run.Check(doubleMismatch == 0, "TryParseDouble agrees with std::stod on every sample input", to_string(doubleMismatch) + " differ");

		Time parsed;
		run.Check(Time::TryParse("2000-08-31", parsed) && parsed.IsValid() && parsed.GetYear() == 2000 && parsed.GetMonth() == 8 && parsed.GetDay() == 31,
			"Time::TryParse accepts a valid date");
		Time untouched(1999, 1, 2);
		run.Check(!Time::TryParse("not a time at all", untouched) && untouched.GetYear() == 1999, "Time::TryParse rejects garbage without throwing and leaves the output alone");
		ErrorReport report;
		GuardCall([&] {
			ErrorScope scope(kErrorScopeStep, "after TryParse");
			THROW_EXCEPTION(InternalException, "TryParse must not leave a trace behind");
		}, report);
		run.Check(report.context.size() == 1, "a failed Time::TryParse leaves no trace for later reports", JoinContext(report));
	}

	void WriteFile(const filesystem::path& path, const string& text) {
		ofstream out(path, ios::binary);
		out << text;
	}

	// 坏config：ReadConfig不抛异常，读完配置是空的(不是读到一半的半份)，原因能查到。最后把原来的config读回来。
	void CheckBadConfig(const TestOptions& options) {
		TestRun& run = TestRun::Get();
		const filesystem::path dir = filesystem::path(options.outDir) / "fault";
		error_code ec;
		filesystem::create_directories(dir, ec);

		// dll_paths合法(用真实config里的第一个目录)、resource_paths里混进一个对象：数字会被转成字符串不算错，对象/数组才转不了。
		// dll_paths先读，所以读到resource_paths出错时已经有东西读进去了——出错后必须整份清空。
		Config::ReadConfig(options.configPath);
		const vector<string> realDllPaths = Config::GetDllPaths();
		run.Check(!realDllPaths.empty(), "the real config has dll paths to borrow for the bad-config test");
		if (realDllPaths.empty()) return;
		const filesystem::path wrongType = dir / "config_wrong_type.json";
		WriteFile(wrongType, "{ \"dll_paths\": [\"" + filesystem::path(realDllPaths[0]).generic_string() + "\"], \"resource_paths\": [{ \"oops\": 1 }], \"main_story\": \"script_empty\" }");
		ErrorReport report;
		const bool finished = GuardCall([&] { Config::ReadConfig(wrongType.string()); }, report);
		run.Check(finished, "ReadConfig does not throw on an element of the wrong type", report.ToLine());
		run.Check(Config::GetDllPaths().empty() && !Config::HasResourcePaths() && Config::GetStoryScripts().empty(),
			"after a config that fails halfway the config is empty, not half read");
		run.Check(Contains(Config::GetLastReadError(), "[data]") && Contains(Config::GetLastReadError(), "config_wrong_type.json"),
			"the reason is kept: a data error naming the config file", Config::GetLastReadError());

		const filesystem::path syntax = dir / "config_syntax.json";
		WriteFile(syntax, "{ \"dll_paths\": [ ");
		const bool syntaxFinished = GuardCall([&] { Config::ReadConfig(syntax.string()); }, report);
		run.Check(syntaxFinished && Config::GetDllPaths().empty(), "ReadConfig does not throw on a json syntax error and leaves the config empty", report.ToLine());

		Config::ReadConfig(options.configPath);
		run.Check(!Config::GetDllPaths().empty() && Config::GetLastReadError().empty(), "the real config reads back fine afterwards", Config::GetLastReadError());
	}

	// seed只接受0到2^64-1的整数：负数、超范围的数都不接受(当作没配置)，不会悄悄变成别的值。
	void CheckSeedRange(const TestOptions& options) {
		TestRun& run = TestRun::Get();
		const filesystem::path dir = filesystem::path(options.outDir) / "fault";
		error_code ec;
		filesystem::create_directories(dir, ec);
		struct Case { const char* json; bool accepted; unsigned long long value; };
		const Case cases[] = {
			{ "42", true, 42ULL },
			{ "\"42\"", true, 42ULL },
			{ "0", true, 0ULL },
			{ "\"18446744073709551615\"", true, 18446744073709551615ULL },
			{ "-1", false, 0 },
			{ "\"-1\"", false, 0 },
			{ "1e30", false, 0 },
			{ "\"18446744073709551616\"", false, 0 },
			{ "\"not a number\"", false, 0 },
		};
		int wrong = 0;
		for (const Case& item : cases) {
			const filesystem::path path = dir / "config_seed.json";
			WriteFile(path, string("{ \"seed\": ") + item.json + " }");
			Config::ReadConfig(path.string());
			unsigned long long seed = 0;
			const bool has = Config::GetGlobalSeed(seed);
			if (has != item.accepted || (has && seed != item.value)) {
				wrong++;
				run.Note(string("seed ") + item.json + " -> " + (has ? "accepted as " + to_string(seed) : string("rejected")));
			}
		}
		run.Check(wrong == 0, "config seed accepts 0..2^64-1 and rejects negative, out-of-range and non-numeric values", to_string(wrong) + " cases wrong");
		Config::ReadConfig(options.configPath);
	}

	// 几个资源目录里有同名文件：配置里排在前面的目录优先；按文件名找资源走索引，新放的文件要RescanFiles之后才找得到。
	void CheckResourceOrder(const TestOptions& options) {
		TestRun& run = TestRun::Get();
		// 配置里的相对路径是相对配置文件所在目录解析的，这里一律给绝对路径。
		const filesystem::path dir = filesystem::absolute(filesystem::path(options.outDir) / "fault");
		const filesystem::path first = dir / "resources_first", second = dir / "resources_second";
		error_code ec;
		filesystem::remove_all(first, ec);
		filesystem::remove_all(second, ec);
		filesystem::create_directories(first / "nested", ec);
		filesystem::create_directories(second, ec);
		WriteFile(first / "shared.script", "{ \"milestones\": [] }");
		WriteFile(second / "shared.script", "{ \"milestones\": [] }");
		WriteFile(first / "nested" / "shared.dat", "first");
		WriteFile(second / "shared.dat", "second");
		WriteFile(second / "only_second.dat", "second");

		auto writeConfig = [&](const filesystem::path& a, const filesystem::path& b) {
			const filesystem::path path = dir / "config_resources.json";
			WriteFile(path, "{ \"resource_paths\": [\"" + a.generic_string() + "\", \"" + b.generic_string() + "\"] }");
			Config::ReadConfig(path.string());
		};
		auto under = [](const string& path, const filesystem::path& root) {
			return !path.empty() && filesystem::path(path).generic_string().find(root.filename().generic_string()) != string::npos;
		};

		writeConfig(first, second);
		const vector<string> roots = Config::GetResourceRootPaths();
		run.Check(roots.size() == 2 && under(roots[0], first) && under(roots[1], second), "resource roots keep the order they have in the config");
		run.Check(under(Config::GetScriptPath("shared"), first), "a script present in two resource paths resolves to the path listed first", Config::GetScriptPath("shared"));
		run.Check(under(Config::GetResourceFilePath("shared.dat"), first), "a resource file present in two paths resolves to the path listed first", Config::GetResourceFilePath("shared.dat"));
		run.Check(under(Config::GetResourceFilePath("only_second.dat"), second), "a resource file that only exists in the second path is still found");
		run.Check(Config::GetResourceFilePath("missing.dat").empty(), "a resource file that does not exist is not found");

		writeConfig(second, first);
		run.Check(under(Config::GetScriptPath("shared"), second) && under(Config::GetResourceFilePath("shared.dat"), second),
			"swapping the two paths in the config swaps which file wins", Config::GetScriptPath("shared") + " | " + Config::GetResourceFilePath("shared.dat"));

		// 索引是扫描时建的：扫描之后新放的文件要等RescanFiles(开始新游戏/读档时调)才找得到。
		WriteFile(first / "late.dat", "late");
		run.Check(Config::GetResourceFilePath("late.dat").empty(), "a file added after the scan is not found before RescanFiles");
		Config::RescanFiles();
		run.Check(under(Config::GetResourceFilePath("late.dat"), first), "a file added after the scan is found after RescanFiles");
		const vector<string> rescanned = Config::GetResourceRootPaths();
		run.Check(rescanned.size() == 2 && under(rescanned[0], second) && under(rescanned[1], first), "RescanFiles keeps the order of the resource roots");

		Config::ReadConfig(options.configPath);
	}

	// 事件取字段：值通过回调交出来(不按值返回)；没有的字段不调回调。
	void CheckEventLocalValue() {
		TestRun& run = TestRun::Get();
		const string longMessage = "a message that is clearly longer than fifteen characters";
		GlobalMessageEvent event(longMessage);
		ValueType received;
		int calls = 0;
		const bool found = event.GetLocalValue("message", [&](const ValueType& value) { received = value; calls++; });
		run.Check(found && calls == 1 && ToString(received) == longMessage, "an event hands a field to the receiver exactly once", ToString(received));
		calls = 0;
		const bool missing = event.GetLocalValue("no_such_field", [&](const ValueType&) { calls++; });
		run.Check(!missing && calls == 0, "an unknown field returns false without calling the receiver");
		ScriptContext context;
		context.local = &event;
		ErrorReport report;
		string evaluated;
		const bool finished = GuardCall([&] { evaluated = ToString(EvaluateExpression("$$local.message", context)); }, report);
		run.Check(finished && evaluated == longMessage, "a script expression reads the event field through the receiver", finished ? evaluated : report.ToLine());
	}

	// 和AForeverFrameworkActor::EndPlay同一个销毁顺序。
	void DestroyWorld(TestWorld& world) {
		delete world.player; world.player = nullptr;
		delete world.traffic; world.traffic = nullptr;
		delete world.industry; world.industry = nullptr;
		delete world.polit; world.polit = nullptr;
		delete world.society; world.society = nullptr;
		delete world.story; world.story = nullptr;
		delete world.populace; world.populace = nullptr;
		delete world.map; world.map = nullptr;
	}

	// mod dll里抛出的异常。puzzle_fault在Forever_Mod/Test里(见它的.md)，默认不启用，这里临时启用。
	void CheckModExceptions() {
		TestRun& run = TestRun::Get();
		const string id = "puzzle_fault";
		Config::SetModEnabled("Puzzles", id, true);
		Registry::Get().RefreshMods();
		PuzzleFactory& factory = Registry::Get().GetPuzzleFactory();
		if (!factory.CheckRegistered(id)) {
			run.Check(false, "the fault-injection puzzle mod is available", "puzzle_fault is not registered: is Forever_Mod/Test built and in dll_paths?");
			return;
		}

		struct Case { const char* param; ErrorKind kind; const char* messagePart; const char* locationPart; };
		const Case cases[] = {
			{ "std", ErrorKind::Mod, "std::runtime_error on purpose", "" },
			{ "data", ErrorKind::Data, "DataException on purpose", "puzzle_fault.h" },
			{ "unknown", ErrorKind::Mod, "unknown exception", "" },
		};
		for (const Case& item : cases) {
			const string label = string(" (") + item.param + ")";
			Canvas canvas;
			canvas.Init(64, 64);
			ErrorReport report;
			bool created = false;
			const bool finished = GuardCall([&] {
				Puzzle puzzle(&factory, id);
				created = true;
				puzzle.SetParam(item.param);
				puzzle.Init(&canvas, nullptr); // 这里抛；Puzzle的析构在展开途中把mod实例还给它自己的dll
			}, report);
			run.Check(created && !finished, "an exception thrown inside a mod dll reaches the host" + label);
			run.Check(report.kind == item.kind, "it is classified correctly" + label, ErrorKindName(report.kind));
			run.Check(report.mod == id, "it names the mod that threw" + label, "mod=" + report.mod + " context=" + JoinContext(report));
			run.Check(Contains(report.message, item.messagePart), "its message crosses the dll boundary intact" + label, report.message);
			if (item.locationPart[0]) run.Check(Contains(report.location, item.locationPart), "its throw location points into the mod's source" + label, report.location);
		}
		// 正常路径没被弄坏：不给参数就不出错。
		ErrorReport report;
		Canvas canvas;
		canvas.Init(64, 64);
		const bool fine = GuardCall([&] {
			Puzzle puzzle(&factory, id);
			puzzle.Init(&canvas, nullptr);
		}, report);
		run.Check(fine, "the same mod works when it is not told to fail", report.ToLine());

		// 没注册的id：数据错误(config没启用/名字写错)，GuardData接得住。
		const bool unknownFinished = GuardData([&] { Puzzle puzzle(&factory, "puzzle_that_does_not_exist"); }, report);
		run.Check(!unknownFinished && report.kind == ErrorKind::Data, "creating a puzzle with an unknown id is a data error", report.ToLine());
	}

	// 坏的脚本、layout、表达式：都是数据错误，报告里带文件。
	void CheckBadData(const TestOptions& options) {
		TestRun& run = TestRun::Get();
		const filesystem::path dir = filesystem::path(options.childOut).parent_path() / "fault_data";
		error_code ec;
		filesystem::create_directories(dir, ec);
		ErrorReport report;

		const filesystem::path script = dir / "broken.script";
		WriteFile(script, "{ \"milestones\": [ { \"milestone\": ");
		bool finished = GuardCall([&] { Script::ReadScript(script.string()); }, report);
		run.Check(!finished && report.kind == ErrorKind::Data, "a script file with a json syntax error is a data error", report.ToLine());
		run.Check(!report.context.empty() && Contains(report.context[0], "broken.script"), "the report names the script file", JoinContext(report));

		const filesystem::path layout = dir / "broken.layout";
		WriteFile(layout, "{ \"singles\": [ ");
		BuildingLayoutLibrary library;
		finished = GuardCall([&] { library.ReadTemplates({ layout.string() }); }, report);
		run.Check(!finished && report.kind == ErrorKind::Data, "a layout file with a json syntax error is a data error", report.ToLine());
		run.Check(!report.context.empty() && Contains(report.context[0], "broken.layout"), "the report names the layout file", JoinContext(report));

		ScriptContext context;
		finished = GuardCall([&] { EvaluateExpression("1 / 0", context); }, report);
		run.Check(!finished && report.kind == ErrorKind::Data, "dividing by zero in a script expression is a data error", report.ToLine());
		// 一条指令出数据错误只跳过这一条：GuardData接住之后，后面的表达式照常求值。
		int after = 0;
		const bool skipped = !GuardData([&] { EvaluateExpression("1 / 0", context); }, report);
		const bool next = GuardData([&] { after = ToInt(EvaluateExpression("2 + 3", context)); }, report);
		run.Check(skipped && next && after == 5, "after a data error in one expression the next one still evaluates");
	}

	// 没有ScriptMod的脚本：Core不依赖任何具体的脚本mod。这个子进程用的config里已经把script_empty去掉了(见RunFaultSuite)，
	// 世界照样生成出来；带里程碑文件的实体脚本都没有mod，动作原样放行。
	void CheckScriptsWithoutMod(TestWorld& world) {
		TestRun& run = TestRun::Get();
		ScriptFactory& factory = Registry::Get().GetScriptFactory();
		run.Check(!factory.CheckRegistered("script_empty"), "the placeholder script mod is disabled in this child's config");

		int withScript = 0, withMod = 0;
		if (world.traffic) {
			for (const auto& [name, vehicle] : world.traffic->GetVehicles()) {
				if (!vehicle->GetScript()) continue;
				withScript++;
				if (vehicle->GetScript()->GetMod()) withMod++;
			}
		}
		run.Metric("vehicles with a script", withScript, "");
		run.Check(withScript > 0 && withMod == 0, "the world generates without the placeholder script mod: vehicle scripts exist and have no ScriptMod",
			to_string(withScript) + " scripts, " + to_string(withMod) + " with a mod");

		ErrorReport report;
		bool passedThrough = false, emptyType = false;
		const bool finished = GuardCall([&] {
			Script script(&factory, "");
			emptyType = script.GetMod() == nullptr && script.GetType().empty();
			const vector<ScriptAction> actions = { static_cast<const Change*>(nullptr), static_cast<const Dialog*>(nullptr) };
			ScriptContext context;
			const vector<ScriptAction> wrapped = script.WrapScript(nullptr, actions, context, nullptr);
			script.AutoPop();
			passedThrough = wrapped.size() == 2 && wrapped == actions;
		}, report);
		run.Check(finished && emptyType, "a Script can be created without a ScriptMod", report.ToLine());
		run.Check(passedThrough, "a Script without a ScriptMod passes matched actions through unchanged");

		// 点了名的mod拿不到仍然是数据错误，不会悄悄变成"没有mod"。
		const bool created = GuardData([&] { Script script(&factory, "script_that_does_not_exist"); }, report);
		run.Check(!created && report.kind == ErrorKind::Data, "naming a script mod that is not available is still a data error", report.ToLine());
	}

	// "拿不到mod就跳过"的地方都要留下一条警告(同一个mod只报一次)。用debugf的输出函数把警告截下来数。
	vector<string>& CapturedWarnings() {
		static vector<string> warnings;
		return warnings;
	}
	void CaptureSink(DebugLevel level, const char* text) {
		if (level == DebugLevel::Warning && text) CapturedWarnings().push_back(text);
	}
	int WarningsMentioning(const string& part) {
		int count = 0;
		for (const string& warning : CapturedWarnings()) if (Contains(warning, part)) count++;
		return count;
	}

	void CheckSkipWarnings(TestWorld& world) {
		TestRun& run = TestRun::Get();
		if (!world.industry || !world.traffic || !world.player) return;
		const DebugSink previous = GetDebugSink();
		CapturedWarnings().clear();
		SetDebugSink(CaptureSink);

		const bool noProduct = world.industry->CreateProduct("product_missing_x") == nullptr && world.industry->CreateProduct("product_missing_x") == nullptr;
		const bool noStorage = world.industry->CreateStorage("storage_missing_x", "fault room a") == nullptr && world.industry->CreateStorage("storage_missing_x", "fault room b") == nullptr;
		const bool noManufacture = world.industry->CreateManufacture("manufacture_missing_x", "fault line a") == nullptr && world.industry->CreateManufacture("manufacture_missing_x", "fault line b") == nullptr;
		const bool noVehicle = world.traffic->CreateVehicle("vehicle_missing_x", "FaultVehicleA") == nullptr && world.traffic->CreateVehicle("vehicle_missing_x", "FaultVehicleB") == nullptr;
		const bool noAsset = world.player->CreateAsset("asset_missing_x") == nullptr;

		SetDebugSink(previous);
		run.Check(noProduct && noStorage && noManufacture && noVehicle && noAsset, "creating something from a mod that is not available returns nothing");
		run.Check(WarningsMentioning("product_missing_x") == 1, "a skipped product is reported once", to_string(WarningsMentioning("product_missing_x")) + " warnings");
		run.Check(WarningsMentioning("storage_missing_x") == 1, "a skipped storage is reported once per mod", to_string(WarningsMentioning("storage_missing_x")) + " warnings");
		run.Check(WarningsMentioning("manufacture_missing_x") == 1, "a skipped production line is reported once per mod", to_string(WarningsMentioning("manufacture_missing_x")) + " warnings");
		run.Check(WarningsMentioning("vehicle_missing_x") == 1, "a skipped vehicle is reported once per mod", to_string(WarningsMentioning("vehicle_missing_x")) + " warnings");
		run.Check(WarningsMentioning("asset_missing_x") == 1, "a skipped item is reported", to_string(WarningsMentioning("asset_missing_x")) + " warnings");
		CapturedWarnings().clear();
	}

	// 占位mod靠自己"什么都不做"而不起作用，Core不认它们的名字。这个子进程的config里启用了占位的应用/线路/站点mod
	// (见RunFaultSuite)：应用自己声明不上桌面；线路什么都没排出来所以没有线路对象；站点不贴任何路。
	void CheckPlaceholderMods(TestWorld& world) {
		TestRun& run = TestRun::Get();
		AppFactory& apps = Registry::Get().GetAppFactory();
		const bool appEnabled = apps.CheckRegistered("app_empty");
		const bool routeEnabled = Registry::Get().GetRouteFactory().CheckRegistered("route_empty");
		const bool stationEnabled = Registry::Get().GetStationFactory().CheckRegistered("station_empty");
		run.Check(appEnabled && routeEnabled && stationEnabled, "the placeholder app/route/station mods are enabled in this child's config",
			string("app=") + (appEnabled ? "1" : "0") + " route=" + (routeEnabled ? "1" : "0") + " station=" + (stationEnabled ? "1" : "0"));

		Phone phone;
		phone.Init(360, 640);
		bool placeholderOnDesktop = false;
		for (const string& type : phone.GetAppTypes()) if (type == "app_empty") placeholderOnDesktop = true;
		run.Check(!placeholderOnDesktop, "an app that declares showOnDesktop=false gets no desktop icon");
		run.Check(phone.GetAppTypes().size() + 1 == apps.GetRegisteredIds().size(), "every other enabled app still gets its icon",
			to_string(phone.GetAppTypes().size()) + " icons for " + to_string(apps.GetRegisteredIds().size()) + " enabled apps");

		int emptyRoutes = 0;
		bool placeholderRoute = false;
		if (world.traffic) {
			for (const auto& [name, route] : world.traffic->GetRoutes()) {
				if (route->GetLines().empty() && route->GetTrackPaths().empty()) emptyRoutes++;
				if (Contains(name, "route_empty")) placeholderRoute = true;
			}
		}
		run.Check(emptyRoutes == 0 && !placeholderRoute, "a route mod that lays out nothing leaves no route object behind",
			to_string(emptyRoutes) + " empty routes");
	}

	// 载具的种类就是mod注册的那个字符串：Core不兜底成任何具体的种类名。没声明种类的mod注册时报一次警告，
	// 它的车种类是空串(不匹配任何车位)。用测试自己的工厂和两个假mod，不需要加载任何dll。
	struct FaultVehicleMod : VehicleMod {
		FOREVER_MOD_GENERIC_UNIT_TEST("vehicle")
		explicit FaultVehicleMod(const char* type) : type(type) {}
		const char* GetType() const override { return type; }
		const char* GetName() override { return type; }
		const char* type;
	};

	void CheckVehicleCategory() {
		TestRun& run = TestRun::Get();
		const string plain = "vehicle_fault_no_category", named = "vehicle_fault_with_category";
		const DebugSink previous = GetDebugSink();
		CapturedWarnings().clear();
		SetDebugSink(CaptureSink);
		VehicleFactory vehicles;
		ScriptFactory scripts;
		vehicles.SetModArgs({ { plain, "" }, { named, "" } });
		vehicles.RegisterVehicle(plain, [](const string&) -> VehicleMod* { return new FaultVehicleMod("vehicle_fault_no_category"); },
			[](VehicleMod* mod) { delete mod; }, nullptr);
		vehicles.RegisterVehicle(named, [](const string&) -> VehicleMod* { return new FaultVehicleMod("vehicle_fault_with_category"); },
			[](VehicleMod* mod) { delete mod; }, []() -> const char* { return "hovercraft"; });
		SetDebugSink(previous);
		run.Check(WarningsMentioning(plain) == 1 && WarningsMentioning(named) == 0, "registering a vehicle mod without a category is reported, one with a category is not",
			to_string(WarningsMentioning(plain)) + " / " + to_string(WarningsMentioning(named)));
		CapturedWarnings().clear();

		ErrorReport report;
		string plainCategory = "?", namedCategory = "?";
		bool valid = false;
		const bool finished = GuardCall([&] {
			Vehicle a(&vehicles, &scripts, plain, "FaultVehicleA");
			Vehicle b(&vehicles, &scripts, named, "FaultVehicleB");
			valid = a.IsValid() && b.IsValid();
			plainCategory = a.GetCategory();
			namedCategory = b.GetCategory();
		}, report);
		run.Check(finished && valid, "both test vehicles are created", report.ToLine());
		run.Check(plainCategory.empty(), "a vehicle whose mod declares no category has an empty category, not a made-up one", "category=" + plainCategory);
		run.Check(namedCategory == "hovercraft", "a vehicle keeps exactly the category its mod declared", "category=" + namedCategory);
	}

	// 出过这些错之后收尾仍然干净：世界能销毁、没有泄漏、mod dll全部卸掉。
	void CheckTeardown(TestWorld& world) {
		TestRun& run = TestRun::Get();
		DestroyWorld(world);
		run.Check(Script::ClearCache(), "no Script object is alive after the world is destroyed");
		vector<string> leaks;
		for (const auto& [id, count] : Registry::Get().GetLiveInstanceCounts()) leaks.push_back(id + " x" + to_string(count));
		string joined;
		for (const string& leak : leaks) joined += (joined.empty() ? "" : "; ") + leak;
		run.Check(leaks.empty(), "no mod instance leaked by the exceptions above", joined);
		const Registry::UnloadReport report = Registry::Get().UnloadMods();
		joined.clear();
		for (const string& kept : report.keptDlls) joined += (joined.empty() ? "" : "; ") + kept;
		run.Check(report.keptDlls.empty() && Registry::Get().GetLoadedDllPaths().empty(), "every mod dll still unloads after the exceptions above", joined);
	}
}

void FaultSuiteBody(TestWorld& world, const TestOptions& options) {
	CheckScriptsWithoutMod(world);
	CheckPlaceholderMods(world);
	CheckSkipWarnings(world);
	CheckModExceptions();
	CheckBadData(options);
	CheckTeardown(world);
}

void RunFaultSuite(const TestOptions& options) {
	TestRun::Get().Begin("fault", 0, options.selfCommand + " --suite fault");
	CheckKinds();
	CheckScopes();
	CheckParsers();
	CheckBadConfig(options);
	CheckSeedRange(options);
	CheckResourceOrder(options);
	CheckEventLocalValue();
	CheckVehicleCategory();
	// 后半要mod和世界：一个种子、小地图就够了。子进程用一份派生的config：把script_mods里的script_empty去掉
	// (世界还能生成出来就说明Core不依赖这个占位脚本mod；main_story用的是别的脚本mod时才这么做)，
	// 再启用占位的应用/线路/站点mod。
	TestOptions copy = options;
	copy.seeds = { options.EffectiveSeeds().front() };
	{
		ifstream in(options.configPath);
		JsonReader reader;
		JsonValue root;
		const bool parsed = in && reader.Parse(in, root);
		TestRun::Get().Check(parsed, "the config parses for the no-placeholder-script child", reader.GetErrorMessages());
		if (parsed && root["main_story"].AsString() != "script_empty") {
			JsonValue kept(DATA_ARRAY);
			for (const auto& entry : root["script_mods"]) {
				const string raw = entry.AsString();
				if (raw.substr(0, raw.find(' ')) != "script_empty") kept.append(JsonValue(raw));
			}
			root["script_mods"] = kept;
			// 再把占位的应用/线路/站点mod启用上：它们不起作用要靠自己，不靠Core认名字(CheckPlaceholderMods)。
			for (const char* key : { "app_mods", "route_mods", "station_mods" }) {
				JsonValue list(DATA_ARRAY);
				if (root[key].IsArray()) for (const auto& entry : root[key]) list.append(JsonValue(entry.AsString()));
				const string conceptName = string(key).substr(0, string(key).find('_'));
				list.append(JsonValue(conceptName + "_empty"));
				root[key] = list;
			}
			const filesystem::path dir = filesystem::absolute(filesystem::path(options.outDir) / "fault");
			error_code ec;
			filesystem::create_directories(dir, ec);
			const filesystem::path derived = dir / "config_no_script_empty.json";
			ofstream out(derived, ios::binary);
			StyledWriter writer;
			out << writer.Write(root);
			copy.configPath = derived.string();
		}
	}
	RunSuiteInChildren(copy, "fault", 256);
}
