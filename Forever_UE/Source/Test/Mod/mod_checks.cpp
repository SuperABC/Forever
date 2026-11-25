#include "mod_checks.h"

#include "test_framework.h"

#include "common/mod_test.h"
#include "common/random.h"
#include "map/terrain_factory.h"
#include "map/roadnet_factory.h"
#include "map/zone_factory.h"
#include "map/green_factory.h"
#include "map/building_factory.h"
#include "map/component_factory.h"
#include "map/room_factory.h"
#include "populace/name_factory.h"
#include "populace/scheduler_factory.h"
#include "society/job_factory.h"
#include "society/organization_factory.h"
#include "polit/party_factory.h"
#include "story/script_factory.h"
#include "industry/product_factory.h"
#include "industry/storage_factory.h"
#include "industry/manufacture_factory.h"
#include "traffic/route_factory.h"
#include "traffic/station_factory.h"
#include "traffic/vehicle_factory.h"
#include "player/asset_factory.h"
#include "player/app_factory.h"
#include "player/puzzle_factory.h"
#include "player/weapon_factory.h"
#include "../../Basic/map/room_furniture_catalog.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <tuple>


using namespace std;

namespace {
	// 23个Factory，和游戏里Registry持有的是同一批类，这里是ModTest自己的实例。检查一个mod时会用到别的concept的
	// Factory(比如组织要问"加载进来的房间类型有哪些")，所以整包传给检查函数。
	struct AllFactories {
		TerrainFactory terrain;
		RoadnetFactory roadnet;
		ZoneFactory zone;
		GreenFactory green;
		BuildingFactory building;
		ComponentFactory component;
		RoomFactory room;
		NameFactory name;
		SchedulerFactory scheduler;
		JobFactory job;
		OrganizationFactory organization;
		ScriptFactory script;
		ProductFactory product;
		StorageFactory storage;
		ManufactureFactory manufacture;
		VehicleFactory vehicle;
		StationFactory station;
		RouteFactory route;
		AssetFactory asset;
		PuzzleFactory puzzle;
		AppFactory app;
		WeaponFactory weapon;
		PartyFactory party;
	};

	// ---- 引用收集 ----
	// mod在字段里写了别的mod的id就是"引用"，被引用的id必须由加载进来的某个dll提供(写错id、忘了带dll都在这里暴露)。
	// 下面的检查函数把看到的引用记进outcome.references，RunMod最后统一核对(CheckReferences)。
	// 只核对"存在"，不核对依赖声明：引用方向不等于依赖方向(楼用stationMod点名站点，但语义上是站点依赖楼)，
	// 谁依赖谁由作者按语义写在GetModDependencies里。
	// 只有"字段里写出来的id"看得到：写在mod代码判断条件里的id(if (roomType == "room_shop"))看不到。
	void Refer(ModOutcome& outcome, const string& what, const string& id) {
		if (!id.empty()) outcome.references.push_back({ what, id });
	}

	// 脚本的引用。created=Core会不会给这个实体建Script(各concept条件不同，调用方算好)；会建而scriptModName为空时
	// Core用默认ScriptMod(kDefaultScriptModId)，同样算引用。不建Script时scriptModName的值没人读，不算引用。
	void ReferScript(ModOutcome& outcome, const string& what, const string& scriptModName, bool created) {
		if (!created) return;
		Refer(outcome, what, scriptModName.empty() ? string(kDefaultScriptModId) : scriptModName);
	}

	// Job/Scheduler/Organization/Asset/Weapon/Vehicle走Core的MakeScript：scriptModName和milestoneNames都空才没有脚本。
	template <typename Mod> void ReferMakeScript(ModOutcome& outcome, const string& what, const Mod* mod) {
		ReferScript(outcome, what, mod->scriptModName, !mod->scriptModName.empty() || !mod->milestoneNames.empty());
	}

	// 门：name非空才建Script，而且直接用scriptModName(没有默认值兜底，空着Core会抛异常)。
	void ReferDoor(ModOutcome& outcome, const string& what, const DoorSpec& door) {
		if (door.name.empty()) return;
		if (door.scriptModName.empty()) outcome.problems.push_back(what + " has a name but an empty scriptModName");
		Refer(outcome, what + " scriptModName", door.scriptModName);
	}

	// Zone/Building的引用在Layout里才填(内部建筑、房间、组件、电梯和门的脚本)，而且带随机：拿几块样例地块各Layout一次，
	// 把出现过的引用并起来。抽样查不全(某个分支没抽到就看不到)，但抽到的一定是真引用。
	constexpr int kLayoutRounds = 8;

	// ---- 各concept的字段合法性检查：默认空实现，下面按基类重载。mod在SetProperty等方法里填字段，这里统一校验。 ----
	template <typename Mod> void ValidateConcept(AllFactories&, Mod*, const ModCheckContext&, ModOutcome&) {}

	void ValidateConcept(AllFactories&, VehicleMod* mod, const ModCheckContext&, ModOutcome& outcome) {
		vector<string>& problems = outcome.problems;
		mod->SetProperty();
		if (mod->sizeX <= 0 || mod->sizeY <= 0 || mod->sizeZ <= 0) problems.push_back("vehicle size must be positive");
		if (mod->capacity < 0) problems.push_back("vehicle capacity is negative");
		if (mod->boardable && mod->capacity == 0) problems.push_back("boardable vehicle has capacity 0");
		if (mod->meshTransform.scale <= 0) problems.push_back("vehicle mesh scale must be positive");
		if (mod->blueprintPath.empty() && mod->transitMeshPath.empty() && mod->drivable) problems.push_back("drivable vehicle has neither blueprintPath nor transitMeshPath");
		ReferMakeScript(outcome, "vehicle scriptModName", mod);
	}

	void ValidateConcept(AllFactories&, ProductMod* mod, const ModCheckContext&, ModOutcome& outcome) {
		vector<string>& problems = outcome.problems;
		mod->SetProperty();
		// 占位mod故意不填价格，Core::Product构造时判无效，不会进游戏。
		if (mod->retailPrice == 0 && mod->importPrice == 0 && mod->exportPrice == 0 && mod->ingredients.empty()) return;
		if (mod->batchSize <= 0) problems.push_back("product batchSize must be positive");
		// 价格规则见product_mod.h：retail > import > export > 0。
		if (!(mod->retailPrice > mod->importPrice && mod->importPrice > mod->exportPrice && mod->exportPrice > 0)) problems.push_back("product prices must satisfy retail > import > export > 0");
		for (const auto& [type, amount] : mod->ingredients) {
			if (type.empty() || amount <= 0) problems.push_back("product ingredient needs a type and a positive amount");
			Refer(outcome, "product ingredient", type);
		}
		for (const auto& [type, amount] : mod->byproducts) {
			if (type.empty() || amount <= 0) problems.push_back("product byproduct needs a type and a positive amount");
			Refer(outcome, "product byproduct", type);
		}
		if (!mod->serviceType.empty() && mod->serviceQuality <= 0) problems.push_back("product with a serviceType needs a positive serviceQuality");
	}

	void ValidateConcept(AllFactories&, JobMod* mod, const ModCheckContext&, ModOutcome& outcome) {
		mod->SetProperty();
		ReferMakeScript(outcome, "job scriptModName", mod);
		for (const string& milestone : mod->milestoneNames) if (milestone.empty()) outcome.problems.push_back("job has an empty milestone name");
	}

	void ValidateConcept(AllFactories& all, ZoneMod*, const ModCheckContext& context, ModOutcome& outcome) {
		const unordered_map<int, Road*> noRoads;
		for (int round = 0; round < kLayoutRounds; round++) {
			ZoneMod* mod = all.zone.CreateZone(context.modId);
			if (!mod) return;
			// 300m~600m见方的样例园区(地图单位，1=10m)，四个朝向轮着来。
			const Quad quad(0.f, 0.f, 30.f + 10.f * (round % 4), 30.f + 15.f * (round / 4));
			{
				RandomScope scope(RandomRegistry::GetInstanceStream(mod));
				mod->Layout(FACE_WEST + round % 4, quad, noRoads);
			}
			ReferScript(outcome, "zone scriptModName", mod->scriptModName, !mod->milestoneNames.empty());
			for (const ZoneGateSpec& gate : mod->gates) ReferDoor(outcome, "zone gate", gate.door);
			for (const ZoneInternalBuildingSpec& building : mod->internalBuildings) {
				if (building.type.empty()) outcome.problems.push_back("zone internal building has an empty type");
				Refer(outcome, "zone internal building", building.type);
			}
			all.zone.DestroyZone(mod);
		}
	}

	void ValidateConcept(AllFactories& all, BuildingMod* first, const ModCheckContext& context, ModOutcome& outcome) {
		vector<string>& problems = outcome.problems;
		if (first->stateOwnedChance < 0.f || first->stateOwnedChance > 1.f) problems.push_back("building stateOwnedChance must be within [0,1]");
		const float acreageMin = all.building.GetAcreageMin(context.modId), acreageMax = all.building.GetAcreageMax(context.modId);
		const unordered_map<int, Road*> noRoads;
		for (int round = 0; round < kLayoutRounds; round++) {
			BuildingMod* mod = all.building.CreateBuilding(context.modId);
			if (!mod) return;
			// 样例地块：面积在这个建筑自己声明的范围里均匀取，长宽比两种轮换；没声明面积的(只做显式占位)给1公顷。
			// acreage是平方米，Quad是地图单位(1=10m)。朝向-1(由mod自己挑)和四个真实朝向都给到。
			const float acreage = acreageMax > 0.f ? acreageMin + (acreageMax - acreageMin) * round / (kLayoutRounds - 1) : 10000.f;
			const float area = max(acreage, 100.f) / 100.f, aspect = round % 2 ? 1.5f : 1.f / 1.5f;
			const Quad quad(0.f, 0.f, sqrt(area * aspect), sqrt(area / aspect));
			int direction = round % 5 == 4 ? -1 : FACE_WEST + round % 4;
			{
				RandomScope scope(RandomRegistry::GetInstanceStream(mod));
				mod->Layout(direction, quad, noRoads);
			}
			Refer(outcome, "building stationMod", mod->stationMod);
			ReferScript(outcome, "building scriptModName", mod->scriptModName, !mod->milestoneNames.empty());
			for (const ElevatorCabinSpec& cabin : mod->cabins) {
				if (cabin.minFloor > cabin.maxFloor) problems.push_back("elevator cabin minFloor > maxFloor");
				ReferScript(outcome, "elevator cabin scriptModName", cabin.scriptModName, !cabin.milestoneNames.empty());
			}
			for (const auto& [tag, door] : mod->doorSpecs) ReferDoor(outcome, "building door '" + tag + "'", door);
			// 房间和组件：AssignRoom/ArrangeRow/ArrangeRowEach记下来的类型都是mod id，Core按它们找RoomFactory/ComponentFactory。
			for (const auto& [key, slots] : mod->singles) {
				Refer(outcome, "building component", key.first);
				for (const auto& slot : slots) Refer(outcome, "building room", get<2>(slot));
			}
			for (const auto& [key, slots] : mod->rows) {
				Refer(outcome, "building component", key.first);
				for (const auto& slot : slots) Refer(outcome, "building room", get<2>(slot));
			}
			for (const auto& slot : mod->eachRows) {
				Refer(outcome, "building room", get<2>(slot));
				Refer(outcome, "building component", get<4>(slot));
			}
			all.building.DestroyBuilding(mod);
		}
	}

	void ValidateConcept(AllFactories&, StationMod* mod, const ModCheckContext&, ModOutcome& outcome) {
		mod->SetProperty();
		mod->Layout(0, 60.f, 60.f); // 端口在Layout里按站台尺寸摆
		const bool hasPorts = !mod->stops.empty() || !mod->inlets.empty() || !mod->outlets.empty();
		if (mod->stationType.empty() && !hasPorts) return; // 什么都不声明的占位站点
		if (mod->stationType.empty()) outcome.problems.push_back("station stationType is empty");
		// 没有端口的站点只能是贴路的(AssignRoads在注册时提供，这里看不到)，不算错。
	}

	void ValidateConcept(AllFactories&, RouteMod* mod, const ModCheckContext&, ModOutcome& outcome) {
		mod->SetProperty();
		Refer(outcome, "route vehicleType", mod->vehicleType);
	}

	void ValidateConcept(AllFactories& all, OrganizationMod* mod, const ModCheckContext& context, ModOutcome& outcome) {
		ReferMakeScript(outcome, "organization scriptModName", mod);
		// 岗位类型是DesignJobsForRoom按(组件类型, 房间类型)现填的：拿这个组织要求的组件 × 加载进来的全部房间类型各问一遍，
		// 把它排出来的岗位类型(job的id)都算引用。它认哪些房间是写在判断条件里的，这里看不到。
		const vector<string> roomIds = all.room.GetRegisteredIds();
		RandomScope scope(RandomRegistry::GetInstanceStream(mod));
		for (const auto& [component, range] : all.organization.GetRequirements(context.modId)) {
			if (component.empty()) outcome.problems.push_back("organization requirement has an empty component type");
			Refer(outcome, "organization required component", component);
			for (const string& room : roomIds) {
				mod->DesignJobsForRoom(component, "component", room, "room", 2, true);
				for (const Vacancy& vacancy : mod->vacancies) {
					if (vacancy.type.empty()) outcome.problems.push_back("organization plans a vacancy with an empty job type");
					Refer(outcome, "organization job", vacancy.type);
				}
			}
		}
	}

	// ---- 房间家具布局检查 ----
	// 对一批尺寸/门窗组合调Layout。家具的锚点必须在房间里、yaw是90度的倍数；尺寸表(Basic的room_furniture_catalog.h)里查得到
	// 的家具再查占地：整件在房间内、互不重叠、不压门前、不挡窗。尺寸表里没有的家具(mod自带的模型)只查锚点，记一条Note。
	// 环境变量FURNITURE_DUMP=<路径>时把每个样本的布局写成json给Tools/Furniture/preview.html看，
	// FURNITURE_DUMP_MOD指定导出哪个mod(不设=room_residence)。
	struct FootRect { float u0, u1, v0, v1; };
	bool Overlap(const FootRect& a, const FootRect& b, float eps) {
		return a.u0 < b.u1 - eps && b.u0 < a.u1 - eps && a.v0 < b.v1 - eps && b.v0 < a.v1 - eps;
	}

	string ReadEnv(const char* name) {
		char* value = nullptr;
		size_t length = 0;
		_dupenv_s(&value, &length, name);
		string result = value ? value : "";
		free(value);
		return result;
	}

	void CheckRoomLayouts(RoomFactory& factory, const string& id, vector<string>& problems) {
		struct Sample { float w, d; int doorWall; float doorA; bool windows; };
		const vector<Sample> samples = {
			{ 4, 3, ROOM_WALL_FRONT, 1.0f, false }, { 6, 5, ROOM_WALL_FRONT, 1.5f, true }, { 8, 7, ROOM_WALL_FRONT, 3.0f, true },
			{ 12, 10, ROOM_WALL_FRONT, 2.0f, true }, { 15, 20, ROOM_WALL_FRONT, 7.0f, true }, { 20, 15, ROOM_WALL_BACK, 10.0f, true },
			{ 10, 30, ROOM_WALL_FRONT, 4.0f, true }, { 3, 12, ROOM_WALL_FRONT, 1.0f, true }, { 25, 12, ROOM_WALL_U_MIN, 5.0f, true },
			{ 16, 16, ROOM_WALL_FRONT, 12.0f, true }, { 30, 8, ROOM_WALL_FRONT, 20.0f, true }, { 9, 9, ROOM_WALL_FRONT, 4.0f, false },
		};
		const string dumpFile = ReadEnv("FURNITURE_DUMP");
		string dumpMod = ReadEnv("FURNITURE_DUMP_MOD");
		if (dumpMod.empty()) dumpMod = "room_residence";
		const bool dumping = !dumpFile.empty() && id == dumpMod;
		string dump = "[";
		int knownTotal = 0;
		vector<string> sparse;        // "大房间里家具太少"：只对用了尺寸表里家具的mod才算问题
		vector<string> unknownMeshes; // 尺寸表里没有的家具
		for (int round = 0; round < 8; round++) {
			for (size_t si = 0; si < samples.size(); si++) {
				const Sample& sm = samples[si];
				RoomMod* mod = factory.CreateRoom(id);
				if (!mod) return;
				mod->SetProperty();
				const bool residence = mod->isResidence;
				RoomLayoutInfo info;
				info.width = sm.w; info.depth = sm.d; info.height = 4.f;
				RoomOpening door; door.wall = sm.doorWall; door.a0 = sm.doorA; door.a1 = sm.doorA + 1.0f; door.z0 = 0.f; door.z1 = 2.2f; door.tag = "home";
				info.doors.push_back(door);
				if (sm.windows) {
					float wallLen = sm.w;
					int wallForWindows = sm.doorWall == ROOM_WALL_BACK ? ROOM_WALL_FRONT : ROOM_WALL_BACK;
					for (int k = 0; k < 3; k++) {
						RoomOpening win; win.wall = wallForWindows; win.a0 = wallLen * (0.15f + 0.3f * k); win.a1 = win.a0 + 1.6f; win.z0 = 0.9f; win.z1 = 2.4f;
						if (win.a1 < wallLen) info.windows.push_back(win);
					}
				}
				{
					RandomScope scope(RandomRegistry::GetInstanceStream(mod));
					mod->Layout(info);
				}
				string label = "w" + to_string(static_cast<int>(sm.w)) + "d" + to_string(static_cast<int>(sm.d)) + " round" + to_string(round);
				vector<FootRect> feet, overheadFeet;
				vector<string> names;
				string items;
				int knownHere = 0;
				for (const FurnitureSpec& spec : mod->furniture) {
					const RoomFurnitureDims* dims = nullptr;
					string base = spec.meshPath.substr(spec.meshPath.rfind('/') + 1);
					base = base.substr(0, base.find('.'));
					for (const RoomFurnitureDims& d : kRoomFurnitureCatalog) if (base == d.name) dims = &d;
					int yaw = static_cast<int>(lround(spec.yaw));
					bool sideways = (abs(yaw) == 90);
					if (yaw != 0 && yaw != 180 && yaw != -180 && !sideways) problems.push_back(label + ": " + base + " has odd yaw " + to_string(yaw));
					if (!dims) {
						// 不知道尺寸：只能查锚点在不在房间里。
						if (spec.u < -0.03f || spec.u > sm.w + 0.03f || spec.v < -0.03f || spec.v > sm.d + 0.03f)
							problems.push_back(label + ": " + base + " anchor outside the room");
						if (find(unknownMeshes.begin(), unknownMeshes.end(), base) == unknownMeshes.end()) unknownMeshes.push_back(base);
						continue;
					}
					knownHere++;
					float eu = (sideways ? dims->depth : dims->width) * spec.scaleX, ev = (sideways ? dims->width : dims->depth) * spec.scaleX;
					FootRect f{ spec.u - eu / 2, spec.u + eu / 2, spec.v - ev / 2, spec.v + ev / 2 };
					bool flat = base.rfind("Rug", 0) == 0, overhead = spec.z > 1.5f;
					if (f.u0 < -0.03f || f.u1 > sm.w + 0.03f || f.v0 < -0.03f || f.v1 > sm.d + 0.03f)
						problems.push_back(label + ": " + base + " outside the room");
					if (!flat) {
						// 窗前禁放区：窗洞两侧各0.25、向房内0.7(缩小后空间)，家具整体放大1.5倍(见room_residence.cpp ROOM_WINDOW_*)，
						// 任何高度的家具(含吊柜)都不许压进来。
						const float k = 1.5f, winDepth = 0.7f * k, winSide = 0.25f * k;
						for (const RoomOpening& win : info.windows) {
							float a0 = win.a0 - winSide, a1 = win.a1 + winSide;
							FootRect wz{ 0, 0, 0, 0 };
							if (win.wall == ROOM_WALL_FRONT) wz = { a0, a1, sm.d - winDepth, sm.d };
							else if (win.wall == ROOM_WALL_BACK) wz = { a0, a1, 0.f, winDepth };
							else if (win.wall == ROOM_WALL_U_MIN) wz = { 0.f, winDepth, a0, a1 };
							else wz = { sm.w - winDepth, sm.w, a0, a1 };
							if (Overlap(wz, f, 0.02f)) problems.push_back(label + ": " + base + " blocks a window");
						}
					}
					if (overhead) {
						for (const FootRect& o : overheadFeet) if (Overlap(o, f, 0.02f)) problems.push_back(label + ": " + base + " (wall cabinet) overlaps another overhead piece");
						overheadFeet.push_back(f);
					}
					if (!flat && !overhead) {
						for (size_t i = 0; i < feet.size(); i++) {
							if (Overlap(feet[i], f, 0.02f)) problems.push_back(label + ": " + base + " overlaps " + names[i]);
						}
						// 门前清空区 = 1.6m深、门两侧各0.6m，家具整体放大1.5倍后清空区同比放大(见room_residence.cpp ROOM_FURNITURE_SCALE)。
						FootRect dz{ 0, 0, 0, 0 };
						const float k = 1.5f, clearDepth = 1.6f * k, clearSide = 0.6f * k;
						float a0 = door.a0 - clearSide, a1 = door.a1 + clearSide;
						if (door.wall == ROOM_WALL_FRONT) dz = { a0, a1, sm.d - clearDepth, sm.d };
						else if (door.wall == ROOM_WALL_BACK) dz = { a0, a1, 0.f, clearDepth };
						else dz = { 0.f, clearDepth, a0, a1 };
						if (Overlap(dz, f, 0.02f)) problems.push_back(label + ": " + base + " blocks the door");
						feet.push_back(f);
						names.push_back(base);
					}
					items += string(items.empty() ? "" : ",") + "{\"n\":\"" + base + "\",\"u\":" + to_string(spec.u) + ",\"v\":" + to_string(spec.v) +
						",\"yaw\":" + to_string(yaw) + ",\"z\":" + to_string(spec.z) + ",\"s\":" + to_string(spec.scaleX) +
						",\"d\":" + to_string(dims->depth) + ",\"w\":" + to_string(dims->width) + ",\"h\":" + to_string(dims->height) + "}";
				}
				knownTotal += knownHere;
				if (residence && min(sm.w, sm.d) >= 5.f && sm.w * sm.d >= 40.f && mod->furniture.size() < 5)
					sparse.push_back(label + ": only " + to_string(mod->furniture.size()) + " furniture in a " + to_string(sm.w * sm.d) + " m2 home");
				if (dumping) {
					auto openings = [](const vector<RoomOpening>& v) {
						string r = "[";
						for (size_t i = 0; i < v.size(); i++) r += string(i ? "," : "") + "[" + to_string(v[i].wall) + "," + to_string(v[i].a0) + "," + to_string(v[i].a1) + "]";
						return r + "]";
					};
					dump += string(dump.size() > 1 ? "," : "") + "{\"label\":\"" + label + "\",\"w\":" + to_string(sm.w) + ",\"d\":" + to_string(sm.d) +
						",\"doors\":" + openings(info.doors) + ",\"windows\":" + openings(info.windows) + ",\"items\":[" + items + "]}";
				}
				factory.DestroyRoom(mod);
			}
		}
		if (dumping) {
			dump += "]";
			FILE* file = nullptr;
			if (fopen_s(&file, dumpFile.c_str(), "wb") == 0 && file) { fwrite(dump.data(), 1, dump.size(), file); fclose(file); }
		}
		// 用了尺寸表里的家具=按内置家具集摆的住宅，大房间里摆不出几件算问题；完全不摆家具/全用自带模型的mod不查这一条。
		if (knownTotal > 0) problems.insert(problems.end(), sparse.begin(), sparse.end());
		if (!unknownMeshes.empty()) {
			string joined;
			for (const string& mesh : unknownMeshes) joined += (joined.empty() ? "" : ", ") + mesh;
			TestRun::Get().Note("note: " + id + " places furniture whose size is not in the built-in catalog (footprint checks skipped): " + joined);
		}
	}

	void ValidateConcept(AllFactories& all, RoomMod* mod, const ModCheckContext& context, ModOutcome& outcome) {
		vector<string>& problems = outcome.problems;
		mod->SetProperty();
		if (mod->residenceCapacity < 0 || mod->workspaceCapacity < 0) problems.push_back("room capacity is negative");
		if (mod->isResidence && mod->residenceCapacity == 0) problems.push_back("residence room has 0 beds");
		if (mod->isWorkspace && mod->workspaceCapacity == 0) problems.push_back("workspace room has 0 seats");
		if (mod->rent < 0) problems.push_back("room rent is negative");
		if (mod->isStorage) Refer(outcome, "room storageType", mod->storageType);
		if (mod->isManufacture) {
			for (const string& type : mod->manufactureTypes) {
				if (type.empty()) problems.push_back("room has an empty manufactureType");
				Refer(outcome, "room manufactureType", type);
			}
		}
		ReferScript(outcome, "room scriptModName", mod->scriptModName, !mod->milestoneNames.empty());
		for (const auto& [tag, door] : mod->doorSpecs) ReferDoor(outcome, "room door '" + tag + "'", door);
		CheckRoomLayouts(all.room, context.modId, problems);
	}

	void ValidateConcept(AllFactories&, SchedulerMod* mod, const ModCheckContext&, ModOutcome& outcome) {
		mod->SetProperty();
		ReferMakeScript(outcome, "scheduler scriptModName", mod);
	}

	void ValidateConcept(AllFactories&, AssetMod* mod, const ModCheckContext&, ModOutcome& outcome) {
		vector<string>& problems = outcome.problems;
		mod->SetProperty();
		if (mod->weight < 0 || mod->size < 0 || mod->volume < 0) problems.push_back("asset weight/size/volume is negative");
		if (mod->weapon && mod->weaponType.empty()) problems.push_back("weapon asset has no weaponType");
		Refer(outcome, "asset weaponType", mod->weaponType);
		Refer(outcome, "asset productType", mod->productType);
		ReferMakeScript(outcome, "asset scriptModName", mod);
	}

	void ValidateConcept(AllFactories&, TerrainMod* mod, const ModCheckContext& context, ModOutcome& outcome) {
		mod->SetupTexture();
		if (!mod->hasMapColor && !context.isTemplate) outcome.problems.push_back("terrain has no map colour after SetupTexture (call SetMapColor)");
	}

	void ValidateConcept(AllFactories&, StorageMod* mod, const ModCheckContext&, ModOutcome& outcome) {
		vector<string>& problems = outcome.problems;
		mod->SetProperty();
		if (mod->capacity < 0) problems.push_back("storage capacity is negative");
		if (!mod->canTake && !mod->canPut) problems.push_back("storage can neither take nor put");
		for (const auto& [type, demand] : mod->serviceTypeDemand) if (type.empty() || demand < 0) problems.push_back("storage serviceTypeDemand entry invalid");
		for (const auto& [type, demand] : mod->productDemand) {
			if (type.empty() || demand < 0) problems.push_back("storage productDemand entry invalid");
			Refer(outcome, "storage productDemand", type);
		}
	}

	void ValidateConcept(AllFactories&, ManufactureMod* mod, const ModCheckContext&, ModOutcome& outcome) {
		mod->SetTargets();
		for (const auto& [type, batches] : mod->targets) {
			if (type.empty() || batches <= 0) outcome.problems.push_back("manufacture target needs a product type and a positive batch count");
			Refer(outcome, "manufacture target", type);
		}
	}

	void ValidateConcept(AllFactories&, WeaponMod* mod, const ModCheckContext&, ModOutcome& outcome) {
		vector<string>& problems = outcome.problems;
		mod->SetProperty();
		Refer(outcome, "weapon ammoType", mod->ammoType);
		ReferMakeScript(outcome, "weapon scriptModName", mod);
		// 不开火的工具(如相机)弹匣/射程都是0，不按枪检查。
		if (mod->magazineCapacity == 0 && mod->maxRange == 0) return;
		if (mod->magazineCapacity <= 0) problems.push_back("weapon magazine capacity must be positive");
		if (mod->maxRange <= 0) problems.push_back("weapon maxRange must be positive");
		if (mod->modelScale <= 0) problems.push_back("weapon modelScale must be positive");
	}

	// 引用核对：被引用的id必须有dll提供。同一个id只报一次。
	void CheckReferences(const ModCheckContext& context, ModOutcome& outcome) {
		vector<string> seen;
		for (const auto& [what, id] : outcome.references) {
			if (id == context.modId) continue;
			if (find(seen.begin(), seen.end(), id) != seen.end()) continue;
			seen.push_back(id);
			if (!context.isKnownId(id)) outcome.referenceProblems.push_back(what + " '" + id + "' is not provided by any loaded dll");
		}
	}

	// 几轮Layout会把同一条问题重复记几遍，只留第一次。
	void DropDuplicates(vector<string>& lines) {
		vector<string> unique;
		for (const string& line : lines) if (find(unique.begin(), unique.end(), line) == unique.end()) unique.push_back(line);
		lines.swap(unique);
	}

	// 创建实例 -> 在和Core包装类相同的随机作用域里跑UnitTest -> 字段检查(顺带收集引用) -> 核对引用
	// -> 再创建第二个实例检查名字唯一 -> 销毁。
	template <typename Factory, typename Mod>
	void RunMod(AllFactories& all, Factory& factory, Mod* (Factory::* create)(const string&), void (Factory::* destroy)(Mod*),
		const ModCheckContext& context, ModOutcome& outcome) {
		Mod* mod = (factory.*create)(context.modId);
		if (!mod) return;
		outcome.created = true;
		const unsigned long long before = RandomRegistry::UnscopedDraws();
		try {
			{
				RandomScope scope(RandomRegistry::GetInstanceStream(mod));
				outcome.passed = mod->UnitTest(TestPrintLine);
			}
			outcome.unscoped = RandomRegistry::UnscopedDraws() - before;
			ValidateConcept(all, mod, context, outcome);
			DropDuplicates(outcome.problems);
			CheckReferences(context, outcome);
		}
		catch (const exception& e) {
			outcome.passed = false;
			outcome.error = e.what();
		}

		const string firstName = mod->GetName() ? mod->GetName() : "";
		Mod* second = (factory.*create)(context.modId);
		if (second) {
			const string secondName = second->GetName() ? second->GetName() : "";
			outcome.nameUnique = firstName != secondName;
			(factory.*destroy)(second);
		}
		(factory.*destroy)(mod);
	}

	template <typename Factory, typename Mod>
	ModConceptOps MakeOps(const char* conceptKey, AllFactories& all, Factory& factory, Mod* (Factory::* create)(const string&), void (Factory::* destroy)(Mod*)) {
		ModConceptOps ops;
		ops.conceptKey = conceptKey;
		ops.setArgs = [&factory](const unordered_map<string, string>& args) { factory.SetModArgs(args); };
		ops.registerDll = [&factory](void* registerSymbol, void* finishSymbol) {
			using Function = void(*)(Factory*);
			if (registerSymbol) reinterpret_cast<Function>(registerSymbol)(&factory);
			if (finishSymbol) reinterpret_cast<Function>(finishSymbol)(&factory);
		};
		ops.isRegistered = [&factory](const string& id) { return factory.CheckRegistered(id); };
		ops.run = [&all, &factory, create, destroy](const ModCheckContext& context, ModOutcome& outcome) {
			RunMod(all, factory, create, destroy, context, outcome);
		};
		return ops;
	}
}

struct ModTestHost::Factories : AllFactories {};

ModTestHost::ModTestHost() : factories(new Factories()) {
	Factories& f = *factories;
	// 和Dependence/common/mod_concepts.cpp的表一一对应(main.cpp启动时会核对有没有漏)。
	ops.push_back(MakeOps("Terrains", f, f.terrain, &TerrainFactory::CreateTerrain, &TerrainFactory::DestroyTerrain));
	ops.push_back(MakeOps("Roadnets", f, f.roadnet, &RoadnetFactory::CreateRoadnet, &RoadnetFactory::DestroyRoadnet));
	ops.push_back(MakeOps("Zones", f, f.zone, &ZoneFactory::CreateZone, &ZoneFactory::DestroyZone));
	ops.push_back(MakeOps("Greens", f, f.green, &GreenFactory::CreateGreen, &GreenFactory::DestroyGreen));
	ops.push_back(MakeOps("Buildings", f, f.building, &BuildingFactory::CreateBuilding, &BuildingFactory::DestroyBuilding));
	ops.push_back(MakeOps("Components", f, f.component, &ComponentFactory::CreateComponent, &ComponentFactory::DestroyComponent));
	ops.push_back(MakeOps("Rooms", f, f.room, &RoomFactory::CreateRoom, &RoomFactory::DestroyRoom));
	ops.push_back(MakeOps("Names", f, f.name, &NameFactory::CreateName, &NameFactory::DestroyName));
	ops.push_back(MakeOps("Schedulers", f, f.scheduler, &SchedulerFactory::CreateScheduler, &SchedulerFactory::DestroyScheduler));
	ops.push_back(MakeOps("Jobs", f, f.job, &JobFactory::CreateJob, &JobFactory::DestroyJob));
	ops.push_back(MakeOps("Organizations", f, f.organization, &OrganizationFactory::CreateOrganization, &OrganizationFactory::DestroyOrganization));
	ops.push_back(MakeOps("Scripts", f, f.script, &ScriptFactory::CreateScript, &ScriptFactory::DestroyScript));
	ops.push_back(MakeOps("Products", f, f.product, &ProductFactory::CreateProduct, &ProductFactory::DestroyProduct));
	ops.push_back(MakeOps("Storages", f, f.storage, &StorageFactory::CreateStorage, &StorageFactory::DestroyStorage));
	ops.push_back(MakeOps("Manufactures", f, f.manufacture, &ManufactureFactory::CreateManufacture, &ManufactureFactory::DestroyManufacture));
	ops.push_back(MakeOps("Vehicles", f, f.vehicle, &VehicleFactory::CreateVehicle, &VehicleFactory::DestroyVehicle));
	ops.push_back(MakeOps("Stations", f, f.station, &StationFactory::CreateStation, &StationFactory::DestroyStation));
	ops.push_back(MakeOps("Routes", f, f.route, &RouteFactory::CreateRoute, &RouteFactory::DestroyRoute));
	ops.push_back(MakeOps("Assets", f, f.asset, &AssetFactory::CreateAsset, &AssetFactory::DestroyAsset));
	ops.push_back(MakeOps("Puzzles", f, f.puzzle, &PuzzleFactory::CreatePuzzle, &PuzzleFactory::DestroyPuzzle));
	ops.push_back(MakeOps("Apps", f, f.app, &AppFactory::CreateApp, &AppFactory::DestroyApp));
	ops.push_back(MakeOps("Weapons", f, f.weapon, &WeaponFactory::CreateWeapon, &WeaponFactory::DestroyWeapon));
	ops.push_back(MakeOps("Parties", f, f.party, &PartyFactory::CreateParty, &PartyFactory::DestroyParty));
}

ModTestHost::~ModTestHost() {
	delete factories;
}

const vector<ModConceptOps>& ModTestHost::GetOps() const {
	return ops;
}

const ModConceptOps* ModTestHost::FindOps(const string& conceptKey) const {
	for (const ModConceptOps& entry : ops) {
		if (entry.conceptKey == conceptKey) return &entry;
	}
	return nullptr;
}
