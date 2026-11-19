#include "room_residence.h"

#include "common/utility.h"
#include "room_furniture_catalog.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace std;

int ResidenceRoom::count = 0;

ResidenceRoom::ResidenceRoom() : id(count++) {
}

const char* ResidenceRoom::GetName() {
	name = "ResidenceRoom" + to_string(id);
	return name.data();
}

void ResidenceRoom::SetProperty() {
	isResidence = true;
	residenceCapacity = 1;
	rent = 2000; // 每日房租20元
	// 挂示例脚本entity_event(进出事件只发给这个实体自己的Script)，见Resource/Story/entity_event.script。
	milestoneNames = { "entity_event" };
	// 住宅房间的门：四种高密度住宅layout里single/row的门洞标签统一是"home"，用和其他房间一样的读卡器门。
	doorSpecs["home"].mesh = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/SKM_door_with_cardreader.SKM_door_with_cardreader";
	doorSpecs["home"].animClass = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/ABP_Door.ABP_Door_C";
}

// ---------------------------------------------------------------- 家具布局
// 设计见room_residence.md"家具布局"：房间局部系(米，u沿正面墙、v朝正面为正，yaw 0=朝+v)里，
// 公共区(客厅/厨房餐厅)靠门、私密区(卧室/卫生间/书房)在里侧，按户型面积决定卧室数；每个功能区是一个矩形，
// 里面按功能靠墙摆家具，摆不下的件直接丢弃。整个布局可随机镜像。

// 调参(米)：
#define ROOM_MARGIN 0.05f          // 家具离墙的最小间隙
#define ROOM_DOOR_CLEAR_DEPTH 1.6f // 门前留空深度(开门扫过的范围，比门宽留得更多)
#define ROOM_DOOR_CLEAR_SIDE 0.6f  // 门洞两侧各多留
#define ROOM_WINDOW_CLEAR 0.30f    // 窗洞前这个距离内不放高过窗台的家具
#define ROOM_WALL_CABINET_Z 1.4f   // 吊柜离地
#define ROOM_TWO_BAND_MIN_DEPTH 7.f
#define ROOM_TWO_BAND_MIN_WIDTH 6.f
#define ROOM_CORE_INSET_RATIO 0.25f // 中心留空区：每边内缩 房间短边*该比例，夹在下面两个值之间(缩小后的空间里，米)
#define ROOM_CORE_INSET_MIN 1.3f
#define ROOM_CORE_INSET_MAX 2.5f
#define ROOM_FURNITURE_SCALE 1.5f  // 家具整体放大倍数(含间距、走道、吊柜高度)，见 Layout()
#define ROOM_WINDOW_SILL_TOL 0.25f // 家具高出窗台这么多以内仍可贴窗放(床头板略高于窗台)
#define ROOM_ZONE_SHRINK 0.75f     // 功能区总最小长度允许压缩到多少比例而不丢区

namespace {
	const char* kMeshRoot = "/Game/Asset/Room/Mesh/";

	enum Side { SIDE_BACK = 0, SIDE_FRONT, SIDE_LEFT, SIDE_RIGHT }; // 矩形的 v小/v大/u小/u大 边
	enum Zone { ZONE_LIVING, ZONE_KITCHEN, ZONE_BEDROOM, ZONE_BATH, ZONE_STUDY };

	struct Rect {
		float u0 = 0, u1 = 0, v0 = 0, v1 = 0;
		float W() const { return u1 - u0; }
		float H() const { return v1 - v0; }
		bool Overlaps(const Rect& o, float eps = 0.005f) const {
			return u0 < o.u1 - eps && o.u0 < u1 - eps && v0 < o.v1 - eps && o.v0 < v1 - eps;
		}
	};

	bool Chance(float p) { return GetRandom(1000) < static_cast<int>(p * 1000.f); }
	float RandRange(float a, float b) { return a + (b - a) * GetRandom(1000) / 999.f; }

	const RoomFurnitureDims* FindDims(const string& name) {
		for (const RoomFurnitureDims& dims : kRoomFurnitureCatalog) {
			if (name == dims.name) return &dims;
		}
		return nullptr;
	}

	string Variant(const char* base, int count) {
		return string(base) + "_" + string(1, static_cast<char>('A' + GetRandom(count)));
	}

	template <class T> void Shuffle(vector<T>& v) {
		for (int i = static_cast<int>(v.size()) - 1; i > 0; i--) swap(v[i], v[GetRandom(i + 1)]);
	}

	// 布局器：全部在(可能镜像过的)房间局部系里算。
	class Planner {
	public:
		Planner(const RoomLayoutInfo& info) : W(info.width), D(info.depth), H(info.height) {
			room = { 0.f, W, 0.f, D };
			// 中心留空区：除地毯/吊柜/茶几椅子这类配套件外，家具中心不得落在这里，保证房间中间是空的、家具都在四周。
			float inset = min(max(min(W, D) * ROOM_CORE_INSET_RATIO, ROOM_CORE_INSET_MIN), ROOM_CORE_INSET_MAX);
			core = { inset, W - inset, inset, D - inset };
			for (const RoomOpening& door : info.doors) AddDoorZone(door);
			windows = info.windows;
		}

		void Run() {
			float area = W * D;
			if (W < 2.5f || D < 2.5f || area < 8.f) return; // 太小，不装
			int bedrooms = area < 40.f ? 0 : area < 85.f ? 1 : area < 150.f ? 2 : area < 230.f ? 3 : 4;
			int baths = area < 150.f ? 1 : 2;
			bool study = area >= 150.f;

			vector<Zone> publicZones = { ZONE_LIVING, ZONE_KITCHEN };
			vector<Zone> privateZones;
			for (int i = 0; i < bedrooms; i++) privateZones.push_back(ZONE_BEDROOM);
			for (int i = 0; i < baths; i++) privateZones.push_back(ZONE_BATH);
			if (study) privateZones.push_back(ZONE_STUDY);
			studio = bedrooms == 0;
			if (studio) { privateZones.clear(); privateZones.push_back(ZONE_BATH); }

			bool twoBand = D >= ROOM_TWO_BAND_MIN_DEPTH && W >= ROOM_TWO_BAND_MIN_WIDTH;
			twoBandMode = twoBand;
			bool doorAtBack = DoorWall() == ROOM_WALL_BACK;
			float doorU = DoorAlong(W * 0.5f);
			if (twoBand) {
				float publicDepth = min(max(D * 0.45f, 3.5f), D - 3.2f);
				Rect pub = doorAtBack ? Rect{ 0.f, W, 0.f, publicDepth } : Rect{ 0.f, W, D - publicDepth, D };
				Rect pri = doorAtBack ? Rect{ 0.f, W, publicDepth, D } : Rect{ 0.f, W, 0.f, D - publicDepth };
				Shuffle(privateZones);
				Strips(pub, true, OrderPublic(publicZones, doorU / W));
				Strips(pri, true, privateZones);
			} else {
				// 单带：沿长轴排，公共区在靠门的一端，私密区在另一端。
				vector<Zone> seq = publicZones;
				Shuffle(privateZones);
				seq.insert(seq.end(), privateZones.begin(), privateZones.end());
				bool alongU = W >= D;
				if (alongU) {
					if (doorU > W * 0.5f) reverse(seq.begin(), seq.end());
					Strips(room, true, seq);
				} else {
					// 沿v排：公共区靠正面(v大)；门在背面则反过来
					if (!doorAtBack) reverse(seq.begin(), seq.end());
					Strips(room, false, seq);
				}
			}
		}

		const vector<FurnitureSpec>& Result() const { return out; }

	private:
		float W, D, H;
		Rect room;
		Rect core{ 0.f, 0.f, 0.f, 0.f };
		bool studio = false;
		bool twoBandMode = false;
		vector<Rect> doorZones;
		vector<RoomOpening> windows;
		vector<RoomOpening> doorList;
		vector<Rect> occupied;
		vector<Rect> overheadOccupied; // 吊柜之间互不重叠(不同高度的落地件不和吊柜比)
		vector<FurnitureSpec> out;

		// ---- 门/窗 ----
		void AddDoorZone(const RoomOpening& door) {
			doorList.push_back(door);
			float a0 = door.a0 - ROOM_DOOR_CLEAR_SIDE, a1 = door.a1 + ROOM_DOOR_CLEAR_SIDE;
			switch (door.wall) {
			case ROOM_WALL_FRONT: doorZones.push_back({ a0, a1, D - ROOM_DOOR_CLEAR_DEPTH, D }); break;
			case ROOM_WALL_BACK: doorZones.push_back({ a0, a1, 0.f, ROOM_DOOR_CLEAR_DEPTH }); break;
			case ROOM_WALL_U_MIN: doorZones.push_back({ 0.f, ROOM_DOOR_CLEAR_DEPTH, a0, a1 }); break;
			default: doorZones.push_back({ W - ROOM_DOOR_CLEAR_DEPTH, W, a0, a1 }); break;
			}
		}
		int DoorWall() const { return doorList.empty() ? ROOM_WALL_FRONT : doorList[0].wall; }
		float DoorAlong(float fallback) const {
			if (doorList.empty()) return fallback;
			const RoomOpening& d = doorList[0];
			// 门在front/back墙上取u；在侧墙上折成"靠哪一端"
			if (d.wall == ROOM_WALL_U_MIN) return W * 0.1f;
			if (d.wall == ROOM_WALL_U_MAX) return W * 0.9f;
			return (d.a0 + d.a1) * 0.5f;
		}

		// ---- 摆放核心 ----
		// 家具footprint(u,v方向的范围)。yaw只会是0/180/90/-90。
		static void Extent(const RoomFurnitureDims& dims, int yaw, float& eu, float& ev) {
			bool sideways = (yaw == 90 || yaw == -90);
			eu = sideways ? dims.depth : dims.width;
			ev = sideways ? dims.width : dims.depth;
		}

		// 在(cu,cv)放一件家具(yaw 0/180/90/-90)，要求完全落在bound里、不压已占位置/门前、不挡窗；
		// overhead=吊柜类(只检查bound，不登记占位)，flat=地毯类(同)。成功返回true。
		bool TryPlace(const string& name, float cu, float cv, int yaw, const Rect& bound, float z = 0.f,
			float scale = 1.f, bool overhead = false, bool flat = false, bool accessory = false) {
			const RoomFurnitureDims* dims = FindDims(name);
			if (!dims) return false;
			float eu, ev;
			Extent(*dims, yaw, eu, ev);
			eu *= scale; ev *= scale;
			Rect box{ cu - eu * 0.5f, cu + eu * 0.5f, cv - ev * 0.5f, cv + ev * 0.5f };
			const float tol = 0.02f;
			if (box.u0 < bound.u0 - tol || box.u1 > bound.u1 + tol || box.v0 < bound.v0 - tol || box.v1 > bound.v1 + tol) return false;
			if (box.u0 < ROOM_MARGIN * 0.5f - tol || box.u1 > W + tol || box.v0 < -tol || box.v1 > D + tol) return false;
			// accessory=茶几/餐椅/书桌椅这类必须挨着主件放的配套件，不受中心留空限制。
			if (!overhead && !flat && !accessory && core.u1 > core.u0 && core.v1 > core.v0 &&
				cu > core.u0 && cu < core.u1 && cv > core.v0 && cv < core.v1) return false;
			if (overhead) {
				// 吊柜：不悬在门洞上方(开门会穿模)，吊柜之间不重叠。
				for (const Rect& r : doorZones) if (box.Overlaps(r)) return false;
				for (const Rect& r : overheadOccupied) if (box.Overlaps(r)) return false;
				overheadOccupied.push_back(box);
			}
			if (!overhead && !flat) {
				for (const Rect& r : occupied) if (box.Overlaps(r)) return false;
				for (const Rect& r : doorZones) if (box.Overlaps(r)) return false;
				// 窗：高过窗台的家具不贴着窗放。
				float top = z + dims->height * (flat ? 1.f : scale);
				for (const RoomOpening& win : windows) {
					if (top <= win.z0 + ROOM_WINDOW_SILL_TOL) continue;
					Rect zone;
					switch (win.wall) {
					case ROOM_WALL_FRONT: zone = { win.a0, win.a1, D - ROOM_WINDOW_CLEAR, D }; break;
					case ROOM_WALL_BACK: zone = { win.a0, win.a1, 0.f, ROOM_WINDOW_CLEAR }; break;
					case ROOM_WALL_U_MIN: zone = { 0.f, ROOM_WINDOW_CLEAR, win.a0, win.a1 }; break;
					default: zone = { W - ROOM_WINDOW_CLEAR, W, win.a0, win.a1 }; break;
					}
					if (box.Overlaps(zone)) return false;
				}
				occupied.push_back(box);
			}
			FurnitureSpec spec;
			spec.meshPath = string(kMeshRoot) + name + "." + name;
			spec.u = cu; spec.v = cv; spec.z = z; spec.yaw = static_cast<float>(yaw);
			spec.scaleX = spec.scaleY = spec.scaleZ = scale;
			out.push_back(spec);
			return true;
		}

		bool IsRealWall(const Rect& r, Side s) const {
			switch (s) {
			case SIDE_BACK: return r.v0 < 0.01f;
			case SIDE_FRONT: return r.v1 > D - 0.01f;
			case SIDE_LEFT: return r.u0 < 0.01f;
			default: return r.u1 > W - 0.01f;
			}
		}
		static float SideLength(const Rect& r, Side s) { return (s == SIDE_BACK || s == SIDE_FRONT) ? r.W() : r.H(); }
		static float SideDepth(const Rect& r, Side s) { return (s == SIDE_BACK || s == SIDE_FRONT) ? r.H() : r.W(); }
		static float SideStart(const Rect& r, Side s) { return (s == SIDE_BACK || s == SIDE_FRONT) ? r.u0 : r.v0; }
		static Side Opposite(Side s) {
			switch (s) { case SIDE_BACK: return SIDE_FRONT; case SIDE_FRONT: return SIDE_BACK; case SIDE_LEFT: return SIDE_RIGHT; default: return SIDE_LEFT; }
		}

		// 背靠rect的s边墙、面朝房内，沿墙中心坐标=along(u或v)。depthOffset=离墙额外距离。
		bool Against(const Rect& r, Side s, const string& name, float along, float z = 0.f, float depthOffset = 0.f,
			bool overhead = false) {
			const RoomFurnitureDims* dims = FindDims(name);
			if (!dims) return false;
			float d = dims->depth, g = ROOM_MARGIN * 0.4f + depthOffset;
			switch (s) {
			case SIDE_BACK: return TryPlace(name, along, r.v0 + d * 0.5f + g, 0, r, z, 1.f, overhead);
			case SIDE_FRONT: return TryPlace(name, along, r.v1 - d * 0.5f - g, 180, r, z, 1.f, overhead);
			case SIDE_LEFT: return TryPlace(name, r.u0 + d * 0.5f + g, along, -90, r, z, 1.f, overhead);
			default: return TryPlace(name, r.u1 - d * 0.5f - g, along, 90, r, z, 1.f, overhead);
			}
		}
		// 家具沿墙的宽度(米)。
		static float WidthOf(const string& name) { const RoomFurnitureDims* d = FindDims(name); return d ? d->width : 0.f; }
		static float DepthOf(const string& name) { const RoomFurnitureDims* d = FindDims(name); return d ? d->depth : 0.f; }

		// 沿s边从start端(fromEnd=false)或end端依次摆一排，返回用掉的沿墙长度。
		float Run(const Rect& r, Side s, const vector<string>& names, bool fromEnd, bool withWallCabinets = false) {
			float L = SideLength(r, s), start = SideStart(r, s);
			float pos = 0.f;
			for (const string& name : names) {
				float w = WidthOf(name);
				if (w <= 0.f || pos + w > L - 0.05f) continue;
				bool placedHere = false;
				float center = 0.f;
				// 窗洞/门挡住时沿墙往后挪着试。
				for (float p = pos; p + w <= L - 0.05f && !placedHere; p += 0.15f) {
					center = fromEnd ? start + L - p - w * 0.5f : start + p + w * 0.5f;
					if (Against(r, s, name, center)) { placedHere = true; pos = p; }
				}
				if (placedHere) {
					pos += w;
					if (withWallCabinets && (strstr(name.c_str(), "Counter") || strstr(name.c_str(), "Stove"))) {
						// 吊柜比灶台宽，和邻居的吊柜撞上就沿墙错开一点再试。
						const string cabinet = Variant("WallCabinet", 2);
						for (float shift : { 0.f, -0.25f, 0.25f, -0.5f, 0.5f })
							if (Against(r, s, cabinet, center + shift, ROOM_WALL_CABINET_Z, 0.f, true)) break;
					}
				}
			}
			return pos;
		}

		// 选一条适合"靠墙大件"的边：长度>=minLen，优先真墙、再按长度；exclude里的边不选。
		bool PickSide(const Rect& r, float minLen, float minDepth, const vector<Side>& exclude, Side& result, bool preferReal = true) {
			float best = -1.f;
			for (int i = 0; i < 4; i++) {
				Side s = static_cast<Side>(i);
				if (find(exclude.begin(), exclude.end(), s) != exclude.end()) continue;
				if (SideLength(r, s) < minLen || SideDepth(r, s) < minDepth) continue;
				float score = SideLength(r, s) * 0.3f + RandRange(0.f, 1.5f) + ((preferReal && IsRealWall(r, s)) ? 2.5f : 0.f);
				if (score > best) { best = score; result = s; }
			}
			return best >= 0.f;
		}

		// 在r的若干条边、每条边的多个沿墙位置上依次尝试把name靠墙放下(窗/门/已有家具挡住就换位置、换边)。
		// realOnly=只考虑真墙(分区之间没有隔墙，靠虚拟边放会像悬在房间中间)。extra=沿墙两端额外留空(给床头柜)。
		// 成功时把所用的边记入used，并返回边与沿墙中心坐标，调用方据此摆配套件。
		bool WallPlace(const Rect& r, const string& name, vector<Side>& used, float minDepth, bool realOnly,
			Side& outSide, float& outAlong, float extra = 0.f) {
			float w = WidthOf(name);
			if (w <= 0.f) return false;
			vector<pair<float, Side>> cands;
			for (int i = 0; i < 4; i++) {
				Side s = static_cast<Side>(i);
				if (find(used.begin(), used.end(), s) != used.end()) continue;
				bool real = IsRealWall(r, s);
				if (realOnly && !real) continue;
				if (SideLength(r, s) < w + 2.f * extra + 0.1f || SideDepth(r, s) < minDepth) continue;
				cands.push_back({ SideLength(r, s) * 0.3f + RandRange(0.f, 1.5f) + (real ? 2.5f : 0.f), s });
			}
			sort(cands.begin(), cands.end(), [](const pair<float, Side>& a, const pair<float, Side>& b) { return a.first > b.first; });
			for (const auto& cand : cands) {
				float L = SideLength(r, cand.second), start = SideStart(r, cand.second);
				float lo = start + w * 0.5f + extra + 0.05f, hi = start + L - w * 0.5f - extra - 0.05f;
				if (hi < lo) continue;
				for (int k = 0; k < 8; k++) {
					float along = k == 0 ? (lo + hi) * 0.5f + RandRange(-0.5f, 0.5f) * (hi - lo) * 0.5f : RandRange(lo, hi);
					if (Against(r, cand.second, name, along)) {
						used.push_back(cand.second);
						outSide = cand.second;
						outAlong = along;
						return true;
					}
				}
			}
			return false;
		}

		// ---- 分区 ----
		vector<Zone> OrderPublic(vector<Zone> zones, float doorFraction) const {
			// 客厅靠门的一侧
			if (doorFraction > 0.5f) reverse(zones.begin(), zones.end());
			return zones;
		}

		static float ZoneWeight(Zone z) {
			switch (z) { case ZONE_LIVING: return 3.f; case ZONE_KITCHEN: return 2.f; case ZONE_BEDROOM: return 2.4f; case ZONE_BATH: return 0.9f; default: return 1.2f; }
		}
		static float ZoneMin(Zone z) {
			switch (z) { case ZONE_LIVING: return 3.2f; case ZONE_KITCHEN: return 2.6f; case ZONE_BEDROOM: return 2.8f; case ZONE_BATH: return 1.8f; default: return 2.2f; }
		}

		// 把矩形沿一个轴切成顺序排列的功能区(alongU=沿u切)，放不下就从后面丢区。
		void Strips(const Rect& r, bool alongU, vector<Zone> zones) {
			float total = alongU ? r.W() : r.H();
			for (;;) {
				float mins = 0.f;
				for (Zone z : zones) mins += ZoneMin(z);
				if (mins * ROOM_ZONE_SHRINK <= total + 0.01f || zones.size() <= 1) break; // 稍挤一点也不丢区(小户型的厨卫要保住)
				// 丢优先级最低的：书房 > 多余卫生间 > 多余卧室 > 其它
				int drop = -1;
				for (Zone cand : { ZONE_STUDY, ZONE_BATH, ZONE_BEDROOM }) {
					int cnt = 0;
					for (Zone z : zones) cnt += (z == cand);
					int limit = (cand == ZONE_STUDY) ? 0 : 1;
					if (cnt > limit) { for (int i = static_cast<int>(zones.size()) - 1; i >= 0; i--) if (zones[i] == cand) { drop = i; break; } break; }
				}
				if (drop < 0) drop = static_cast<int>(zones.size()) - 1;
				zones.erase(zones.begin() + drop);
			}
			float weights = 0.f;
			for (Zone z : zones) weights += ZoneWeight(z);
			// 先给每区最小长度，剩下的按权重分
			float mins = 0.f;
			for (Zone z : zones) mins += ZoneMin(z);
			float spare = max(0.f, total - mins);
			float squeeze = mins > total ? total / mins : 1.f;
			float pos = alongU ? r.u0 : r.v0;
			for (size_t i = 0; i < zones.size(); i++) {
				float len = ZoneMin(zones[i]) * squeeze + spare * ZoneWeight(zones[i]) / weights;
				if (i + 1 == zones.size()) len = (alongU ? r.u1 : r.v1) - pos;
				Rect zr = alongU ? Rect{ pos, pos + len, r.v0, r.v1 } : Rect{ r.u0, r.u1, pos, pos + len };
				Furnish(zones[i], zr);
				pos += len;
			}
		}

		void Furnish(Zone z, const Rect& r) {
			float minSide = z == ZONE_BATH ? 1.1f : 1.6f; // 卫生间允许窄一点(小户型被压缩时)
			if (r.W() < minSide || r.H() < minSide) return;
			switch (z) {
			case ZONE_LIVING: Living(r); break;
			case ZONE_KITCHEN: Kitchen(r); break;
			case ZONE_BEDROOM: Bedroom(r); break;
			case ZONE_BATH: Bath(r); break;
			default: Study(r); break;
			}
		}

		// ---- 各功能区 ----
		void Entry(const Rect& r) {
			// 鞋柜：门洞旁边靠墙。
			for (const RoomOpening& door : doorList) {
				float lo = door.a0, hi = door.a1;
				Side s = door.wall == ROOM_WALL_FRONT ? SIDE_FRONT : door.wall == ROOM_WALL_BACK ? SIDE_BACK : door.wall == ROOM_WALL_U_MIN ? SIDE_LEFT : SIDE_RIGHT;
				if (!IsRealWall(r, s)) continue;
				float start = SideStart(r, s), end = start + SideLength(r, s);
				if (hi < start || lo > end) continue; // 门不在这个区
				string name = Variant("ShoeCabinet", 2);
				float w = WidthOf(name);
				bool preferHigh = Chance(0.5f);
				for (int k = 0; k < 2; k++) {
					bool high = (k == 0) == preferHigh;
					float along = high ? hi + ROOM_DOOR_CLEAR_SIDE + w * 0.5f + 0.1f : lo - ROOM_DOOR_CLEAR_SIDE - w * 0.5f - 0.1f;
					if (Against(r, s, name, along)) return;
				}
			}
		}

		void Living(const Rect& r) {
			Entry(r);
			vector<Side> used;
			if (studio) { // 单间：床比沙发优先(先占墙)，不够放双人床就放单人床
				string bed = Chance(0.5f) ? Variant("Bed_Double", 3) : Variant("Bed_Single", 2);
				Side bs; float bedAlong;
				bool ok = WallPlace(r, bed, used, DepthOf(bed) + 0.5f, true, bs, bedAlong, 0.2f) ||
					WallPlace(r, bed, used, DepthOf(bed) + 0.5f, false, bs, bedAlong, 0.2f);
				if (!ok) {
					bed = Variant("Bed_Single", 2);
					ok = WallPlace(r, bed, used, DepthOf(bed) + 0.5f, true, bs, bedAlong) || WallPlace(r, bed, used, DepthOf(bed) + 0.5f, false, bs, bedAlong);
				}
				Wardrobe(r, used);
			}
			Side sofaSide;
			string sofa =r.W() >= 3.4f || r.H() >= 3.4f ? Variant("Sofa3", 3) : Variant("Sofa2", 2);
			bool placedSofa = false;
			for (int attempt = 0; attempt < 1 && !placedSofa; attempt++) {
				float along = 0.f;
				placedSofa = WallPlace(r, sofa, used, DepthOf(sofa) + 1.0f, false, sofaSide, along);
				if (placedSofa) {
					float sofaDepth = DepthOf(sofa);
					float freeDepth = SideDepth(r, sofaSide) - sofaDepth; // 沙发前方可用的深度
					float tableD = 0.55f;
					// 茶几 + 地毯：沙发前方
					Side opp = Opposite(sofaSide);
					float edge = SideStart(r, sofaSide) < 0 ? 0.f : 0.f; (void)edge;
					auto place_front = [&](const string& n, float forward, int yawFacing, bool flat, float scale) {
						// forward=离沙发前缘的距离到物体中心
						float cu, cv;
						CenterFront(r, sofaSide, sofaDepth, forward, along, cu, cv);
						return TryPlace(n, cu, cv, yawFacing, r, 0.f, scale, false, flat, true);
					};
					int yawFacing = YawFacing(sofaSide);
					if (freeDepth > 1.4f) {
						if (Chance(0.8f)) place_front(Variant("Rug", 4), min(1.5f, freeDepth * 0.5f), yawFacing, true, RandRange(1.0f, 1.3f));
						place_front(Variant("CoffeeTable", 2), 0.35f + tableD * 0.5f, yawFacing, false, 1.f);
					}
					// 电视柜：对面墙(够深)，否则放在侧边
					if (freeDepth < 2.8f || !Against(r, opp, Variant("TvStand", 2), along)) {
						Side tvSide;
						vector<Side> ex = { sofaSide, opp };
						if (PickSide(r, 1.9f, 1.6f, ex, tvSide)) Against(r, tvSide, Variant("TvStand", 2), SideStart(r, tvSide) + SideLength(r, tvSide) * 0.5f);
					}
				}
			}
			// 点缀
			if (Chance(0.6f)) Corner(r, Variant("Plant", 2));
			if (Chance(0.5f)) Corner(r, "FloorLamp");
			if (Chance(0.5f)) {
				Side bs;
				if (PickSide(r, 1.3f, 1.0f, used, bs)) Against(r, bs, Variant("Bookshelf", 2), SideStart(r, bs) + SideLength(r, bs) * RandRange(0.25f, 0.75f));
			}
			if (Chance(0.45f)) {
				Side as;
				if (PickSide(r, 1.2f, 1.2f, used, as)) Against(r, as, Variant("Armchair", 2), SideStart(r, as) + SideLength(r, as) * RandRange(0.25f, 0.75f));
			}
		}

		// 把"朝向房内的yaw"和"距s边(沿法线)的中心位置"换算成(u,v)。
		static int YawFacing(Side s) {
			switch (s) { case SIDE_BACK: return 0; case SIDE_FRONT: return 180; case SIDE_LEFT: return -90; default: return 90; }
		}
		// 沿着s边墙的法线，从墙起算 wallToEdge+forward 处的点，沿墙坐标=along。
		static void CenterFront(const Rect& r, Side s, float wallToEdge, float forward, float along, float& cu, float& cv) {
			float dist = wallToEdge + forward;
			switch (s) {
			case SIDE_BACK: cu = along; cv = r.v0 + dist; break;
			case SIDE_FRONT: cu = along; cv = r.v1 - dist; break;
			case SIDE_LEFT: cu = r.u0 + dist; cv = along; break;
			default: cu = r.u1 - dist; cv = along; break;
			}
		}

		void Corner(const Rect& r, const string& name) {
			const RoomFurnitureDims* dims = FindDims(name);
			if (!dims) return;
			float hw = dims->width * 0.5f + 0.08f;
			int order[4] = { 0, 1, 2, 3 };
			for (int i = 3; i > 0; i--) swap(order[i], order[GetRandom(i + 1)]);
			for (int k = 0; k < 4; k++) {
				float cu = (order[k] & 1) ? r.u1 - hw : r.u0 + hw;
				float cv = (order[k] & 2) ? r.v1 - hw : r.v0 + hw;
				if (TryPlace(name, cu, cv, 0, r)) return;
			}
		}

		void Wardrobe(const Rect& r, vector<Side>& used) {
			Side s; float along;
			string name = Variant("Wardrobe", 3);
			// 大户型(分带)里分区之间没有隔墙，贴虚拟边的衣柜会悬在房间中间，宁可不放。
			if (!WallPlace(r, name, used, DepthOf(name) + 0.8f, true, s, along) && !twoBandMode) WallPlace(r, name, used, DepthOf(name) + 0.8f, false, s, along);
		}

		void Bedroom(const Rect& r) {
			vector<Side> used;
			float area = r.W() * r.H();
			string bed = (area >= 14.f && min(r.W(), r.H()) >= 3.0f) ? Variant("Bed_Double", 3) : Variant("Bed_Single", 2);
			Side bs; float along = 0.f;
			float bedExtra = WidthOf(bed) > 1.2f ? 0.5f : 0.25f; // 两侧给床头柜留位
			bool placedBed = WallPlace(r, bed, used, DepthOf(bed) + 0.4f, true, bs, along, bedExtra) ||
				(!twoBandMode && WallPlace(r, bed, used, DepthOf(bed) + 0.4f, false, bs, along, bedExtra));
			if (placedBed) {
				{
					float bw = WidthOf(bed);
					if (bw > 1.2f || Chance(0.5f)) {
						string stand = Variant("Nightstand", 2);
						Against(r, bs, stand, along - bw * 0.5f - WidthOf(stand) * 0.5f - 0.03f);
						if (bw > 1.2f) Against(r, bs, stand, along + bw * 0.5f + WidthOf(stand) * 0.5f + 0.03f);
					}
					if (Chance(0.5f)) { // 床尾地毯
						float cu, cv;
						CenterFront(r, bs, DepthOf(bed), 0.5f, along, cu, cv);
						TryPlace(Variant("Rug", 4), cu, cv, YawFacing(bs), r, 0.f, RandRange(0.9f, 1.2f), false, true);
					}
				}
			}
			Wardrobe(r, used);
			if (area >= 16.f && Chance(0.5f)) {
				Side ds; float dAlong; string desk = Variant("Desk", 2);
				if (WallPlace(r, desk, used, DepthOf(desk) + 1.2f, true, ds, dAlong)) {
					float cu, cv;
					CenterFront(r, ds, DepthOf(desk), 0.35f, dAlong, cu, cv);
					TryPlace(Variant("Chair", 2), cu, cv, YawFacing(Opposite(ds)), r, 0.f, 1.f, false, false, true);
				}
			}
			if (area >= 18.f && Chance(0.4f)) {
				Side ds; float dAlong;
				WallPlace(r, Variant("Dresser", 2), used, 1.0f, true, ds, dAlong);
			}
			if (Chance(0.3f)) Corner(r, Variant("Plant", 2));
		}

		void Bath(const Rect& r) {
			Side s;
			vector<Side> used;
			// 马桶/洗手台排最前(必需)，浴缸/淋浴放得下才放。
			vector<string> items = { "Toilet", "Washbasin", Chance(0.5f) && min(r.W(), r.H()) >= 1.7f ? "Bathtub" : "Shower" };
			if (r.W() * r.H() >= 6.f && Chance(0.6f)) items.push_back("WashingMachine");
			vector<string> rest;
			// 先在最长的边上排，排不下的再换别的边。
			// 卫浴件只贴真墙(没有隔墙，贴分区虚拟边会悬在房间中间)；真墙放不下的，只给马桶/洗手台再退一步用虚拟边。
			for (int round = 0; round < 6 && !items.empty(); round++) {
				bool onlyReal = round < 3;
				if (round == 3 && twoBandMode) break;
				if (round == 3) {
					vector<string> essential;
					for (const string& n : items) if (n == "Toilet" || n == "Washbasin") essential.push_back(n);
					items = essential;
					used.clear();
				}
				vector<Side> blocked = used;
				if (onlyReal) for (int i = 0; i < 4; i++) if (!IsRealWall(r, static_cast<Side>(i))) blocked.push_back(static_cast<Side>(i));
				if (!PickSide(r, 0.8f, 0.9f, blocked, s)) { if (onlyReal) { round = 2; } continue; }
				used.push_back(s);
				float L = SideLength(r, s), start = SideStart(r, s), pos = 0.f;
				vector<string> left;
				for (const string& n : items) {
					float w = WidthOf(n);
					bool placed = false;
					// 窗洞挡住时沿墙往后挪着试，而不是直接放弃这面墙。
					for (float p = pos; p + w <= L - 0.05f && !placed; p += 0.15f) {
						if (Against(r, s, n, start + p + w * 0.5f)) { placed = true; pos = p + w + 0.05f; }
					}
					if (!placed) left.push_back(n);
				}
				items = left;
			}
		}

		void Kitchen(const Rect& r) {
			Side s;
			vector<Side> used;
			float minRun = 2.4f;
			// 先只考虑真墙(吊柜要有墙可挂)，再退而求其次用分区虚拟边(不挂吊柜)。
			bool realWall = true, found = false;
			{
				vector<Side> notReal;
				for (int i = 0; i < 4; i++) if (!IsRealWall(r, static_cast<Side>(i))) notReal.push_back(static_cast<Side>(i));
				found = PickSide(r, 1.7f, 0.9f, notReal, s);
				if (!found) { realWall = false; found = PickSide(r, minRun, 0.9f, used, s); }
			}
			if (found) {
				used.push_back(s);
				vector<string> run = { Variant("Fridge", 2), Variant("Counter", 2), Variant("SinkCounter", 2), Variant("Counter", 2),
					Variant("Stove", 2), Variant("Counter", 2), Variant("Counter", 2) };
				// 同一排内台面款式统一
				string counter = Variant("Counter", 2);
				for (string& n : run) if (n.rfind("Counter_", 0) == 0) n = counter;
				Run(r, s, run, Chance(0.5f), realWall);
				// 对面再来一排台面(够宽时)，更像岛式/双排厨房
				float rest = SideDepth(r, s) - DepthOf(counter) - 0.2f;
				Side opp = Opposite(s);
				if (rest >= 4.0f && Chance(0.5f)) Run(r, opp, { counter, counter, counter }, Chance(0.5f), IsRealWall(r, opp));
				// 餐桌：剩余空间中心
				if (rest >= 2.2f) {
					Rect freeR = r;
					switch (s) {
					case SIDE_BACK: freeR.v0 += DepthOf(counter) + 0.8f; break;
					case SIDE_FRONT: freeR.v1 -= DepthOf(counter) + 0.8f; break;
					case SIDE_LEFT: freeR.u0 += DepthOf(counter) + 0.8f; break;
					default: freeR.u1 -= DepthOf(counter) + 0.8f; break;
					}
					Dining(freeR);
				}
			} else {
				Dining(r);
			}
		}

		void Dining(const Rect& r) {
			if (r.W() < 1.9f || r.H() < 1.9f) return;
			string table = Variant("DiningTable", 2);
			const RoomFurnitureDims* td = FindDims(table);
			if (!td) return;
			// 桌子长边贴着一面真墙(墙和桌子之间留出一圈椅子的位置)，不放房间中间。
			int order[4] = { 0, 1, 2, 3 };
			for (int i = 3; i > 0; i--) swap(order[i], order[GetRandom(i + 1)]);
			const float gap = 0.65f, dist = gap + td->depth * 0.5f;
			for (int k = 0; k < 4; k++) {
				Side s = static_cast<Side>(order[k]);
				if (!IsRealWall(r, s)) continue;
				bool alongU = (s == SIDE_BACK || s == SIDE_FRONT);
				int yaw = alongU ? 0 : 90; // yaw 0时桌子宽边沿u
				float L = SideLength(r, s), start = SideStart(r, s);
				float lo = start + td->width * 0.5f + 0.4f, hi = start + L - td->width * 0.5f - 0.4f;
				if (hi < lo) continue;
				for (int t = 0; t < 6; t++) {
					float along = t == 0 ? (lo + hi) * 0.5f : RandRange(lo, hi);
					float cu, cv;
					switch (s) {
					case SIDE_BACK: cu = along; cv = r.v0 + dist; break;
					case SIDE_FRONT: cu = along; cv = r.v1 - dist; break;
					case SIDE_LEFT: cu = r.u0 + dist; cv = along; break;
					default: cu = r.u1 - dist; cv = along; break;
					}
					if (!TryPlace(table, cu, cv, yaw, r)) continue;
					string chair = Variant("DiningChair", 2);
					// 桌子宽边沿u(yaw 0)：椅子放在v两侧，朝桌子；yaw 90：沿v排，椅子放在u两侧。
					float length = td->width, depth = td->depth;
					int n = length >= 1.7f ? 3 : 2;
					for (int side = -1; side <= 1; side += 2) {
						for (int i = 0; i < n; i++) {
							float off = (i - (n - 1) * 0.5f) * (length / n);
							float ccu = cu, ccv = cv;
							int cyaw;
							if (yaw == 0) { ccu = cu + off; ccv = cv + side * (depth * 0.5f + 0.28f); cyaw = side > 0 ? 180 : 0; }
							else { ccv = cv + off; ccu = cu + side * (depth * 0.5f + 0.28f); cyaw = side > 0 ? 90 : -90; }
							TryPlace(chair, ccu, ccv, cyaw, r, 0.f, 1.f, false, false, true);
						}
					}
					return;
				}
			}
		}

		void Study(const Rect& r) {
			vector<Side> used;
			Side s; float along;
			string desk = Variant("Desk", 2);
			if (WallPlace(r, desk, used, DepthOf(desk) + 1.2f, true, s, along)) {
				float cu, cv;
				CenterFront(r, s, DepthOf(desk), 0.35f, along, cu, cv);
				TryPlace(Variant("Chair", 2), cu, cv, YawFacing(Opposite(s)), r, 0.f, 1.f, false, false, true);
			}
			for (int i = 0; i < 2; i++) WallPlace(r, Variant("Bookshelf", 2), used, 0.9f, true, s, along);
			if (Chance(0.5f)) Corner(r, Variant("Plant", 2));
		}
	};
}

void ResidenceRoom::Layout(const RoomLayoutInfo& info) {
	// 随机镜像：门窗洞镜像后布局，再把结果镜回来(保证和真实门窗位置一致)。
	bool mirror = GetRandom(2) == 1;

	// 整体放大：把房间(尺寸、门窗洞)按1/S缩小后用原尺寸的家具和间距布局，结果再放大S倍，
	// 这样家具、走道、门前清空区、吊柜高度等所有长度一起放大，布局的疏密不变(游戏多是头顶第三视角，
	// 家具要比真实尺寸大才不显得挤)。
	const float S = ROOM_FURNITURE_SCALE;
	RoomLayoutInfo work = info;
	work.width /= S; work.depth /= S; work.height /= S;
	auto shrink = [S](RoomOpening& o) { o.a0 /= S; o.a1 /= S; o.z0 /= S; o.z1 /= S; };
	for (RoomOpening& o : work.doors) shrink(o);
	for (RoomOpening& o : work.windows) shrink(o);
	auto mirrorOpening = [&](RoomOpening& o) {
		if (o.wall == ROOM_WALL_FRONT || o.wall == ROOM_WALL_BACK) {
			float a0 = work.width - o.a1, a1 = work.width - o.a0;
			o.a0 = a0; o.a1 = a1;
		} else {
			o.wall = (o.wall == ROOM_WALL_U_MIN) ? ROOM_WALL_U_MAX : ROOM_WALL_U_MIN;
		}
	};
	if (mirror) {
		for (RoomOpening& o : work.doors) mirrorOpening(o);
		for (RoomOpening& o : work.windows) mirrorOpening(o);
	}

	Planner planner(work);
	planner.Run();

	for (FurnitureSpec spec : planner.Result()) {
		if (mirror) {
			spec.u = work.width - spec.u;
			spec.yaw = -spec.yaw; // 0->0, 180->-180(同朝向), 90<->-90
		}
		spec.u *= S; spec.v *= S; spec.z *= S;
		spec.scaleX *= S; spec.scaleY *= S; spec.scaleZ *= S;
		AddFurniture(spec);
	}
}
