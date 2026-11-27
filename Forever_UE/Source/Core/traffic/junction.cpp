#include "junction.h"

#include "map/map.h"
#include "map/roadnet.h"
#include "map/general_geometry.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <unordered_set>

using namespace std;

namespace {
	constexpr int kPolylineSteps = 16;
	constexpr float kMaxPedestrianJunctionEdge = 3.f; // 地图单位；比它长的人行边是沿路的人行道，不是路口里的横道/转角

	float Dot(float ax, float ay, float bx, float by) { return ax * bx + ay * by; }

	float PointSegmentDistSq(float px, float py, float ax, float ay, float bx, float by) {
		float abx = bx - ax, aby = by - ay;
		float len2 = Dot(abx, aby, abx, aby);
		float t = len2 > 1e-12f ? clamp(Dot(px - ax, py - ay, abx, aby) / len2, 0.f, 1.f) : 0.f;
		float dx = ax + abx * t - px, dy = ay + aby * t - py;
		return dx * dx + dy * dy;
	}

	bool SegmentsCross(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) {
		auto cross = [](float ux, float uy, float vx, float vy) { return ux * vy - uy * vx; };
		float d1 = cross(bx - ax, by - ay, cx - ax, cy - ay);
		float d2 = cross(bx - ax, by - ay, dx - ax, dy - ay);
		float d3 = cross(dx - cx, dy - cy, ax - cx, ay - cy);
		float d4 = cross(dx - cx, dy - cy, bx - cx, by - cy);
		return ((d1 > 0.f) != (d2 > 0.f)) && ((d3 > 0.f) != (d4 > 0.f));
	}

	float SegmentDistSq(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) {
		if (SegmentsCross(ax, ay, bx, by, cx, cy, dx, dy)) return 0.f;
		return min(min(PointSegmentDistSq(ax, ay, cx, cy, dx, dy), PointSegmentDistSq(bx, by, cx, cy, dx, dy)),
			min(PointSegmentDistSq(cx, cy, ax, ay, bx, by), PointSegmentDistSq(dx, dy, ax, ay, bx, by)));
	}
}

JunctionTraffic::JunctionTraffic(const Config& config) : config(config) {
}

int64_t JunctionTraffic::MakeEntity(EntityKind kind, int id) {
	return (static_cast<int64_t>(kind == EntityKind::Pedestrian ? 1 : 0) << 32) | static_cast<uint32_t>(id);
}

JunctionTraffic::EntityKind JunctionTraffic::GetEntityKind(int64_t entity) {
	return ((entity >> 32) & 1) ? EntityKind::Pedestrian : EntityKind::Vehicle;
}

void JunctionTraffic::AddConnection(const Connection* connection, Kind kind, int junctionId, int approach) {
	if (!connection || index.count(connection)) return;
	Entry entry;
	entry.connection = connection;
	entry.kind = kind;
	entry.junctionId = junctionId;
	entry.startId = connection->GetStart().GetId();
	entry.endId = connection->GetEnd().GetId();
	entry.approach = kind == Kind::Corner ? -1 : approach; // 转角不横穿任何道路，不受红绿灯管
	for (int i = 0; i <= kPolylineSteps; i++) {
		Node p = connection->GetPoint(static_cast<float>(i) / kPolylineSteps);
		entry.polyline.push_back({ p.GetX(), p.GetY() });
	}
	index[connection] = static_cast<int>(entries.size());
	entries.push_back(std::move(entry));
}

void JunctionTraffic::BuildConflicts() {
	entriesByExit.clear();
	crosswalkGrid.clear();
	vehicleStartGrid.clear();
	for (Entry& entry : entries) entry.conflicts.clear();

	unordered_map<int, vector<int>> byJunction;
	for (int i = 0; i < static_cast<int>(entries.size()); i++) {
		byJunction[entries[i].junctionId].push_back(i);
		if (entries[i].kind == Kind::Vehicle) {
			entriesByExit[entries[i].endId].push_back(i);
			const Point& start = entries[i].polyline.front();
			vehicleStartGrid[CellKey(static_cast<int>(floorf(start.x)), static_cast<int>(floorf(start.y)))].push_back(i);
		}
		if (entries[i].kind == Kind::Crosswalk) {
			const Point& a = entries[i].polyline.front();
			const Point& b = entries[i].polyline.back();
			crosswalkGrid[CellKey(static_cast<int>(floorf((a.x + b.x) * 0.5f)), static_cast<int>(floorf((a.y + b.y) * 0.5f)))].push_back(i);
		}
	}

	auto polylineDistSq = [](const Entry& a, const Entry& b, float limitSq) {
		float best = FLT_MAX;
		for (size_t i = 0; i + 1 < a.polyline.size(); i++) {
			const Point &p = a.polyline[i], &q = a.polyline[i + 1];
			for (size_t j = 0; j + 1 < b.polyline.size(); j++) {
				const Point &r = b.polyline[j], &s = b.polyline[j + 1];
				best = min(best, SegmentDistSq(p.x, p.y, q.x, q.y, r.x, r.y, s.x, s.y));
				if (best <= limitSq) return best;
			}
		}
		return best;
	};

	for (const auto& [junctionId, list] : byJunction) {
		for (size_t i = 0; i < list.size(); i++) {
			for (size_t j = i + 1; j < list.size(); j++) {
				Entry& a = entries[list[i]];
				Entry& b = entries[list[j]];
				if (a.kind != Kind::Vehicle && b.kind != Kind::Vehicle) continue; // 人和人不互相阻挡

				bool conflict = false;
				if (a.kind == Kind::Vehicle && b.kind == Kind::Vehicle) {
					if (a.startId == b.startId) continue;  // 同一条进车道出发的各个转向：分叉，不冲突
					if (a.endId == b.endId) conflict = true; // 汇入同一条出车道
					else conflict = polylineDistSq(a, b, config.vehicleTolerance * config.vehicleTolerance)
						<= config.vehicleTolerance * config.vehicleTolerance;
				}
				else {
					const Entry& other = a.kind == Kind::Vehicle ? b : a;
					float tol = other.kind == Kind::Crosswalk ? config.crosswalkTolerance : config.cornerTolerance;
					conflict = polylineDistSq(a, b, tol * tol) <= tol * tol;
				}
				if (conflict) {
					a.conflicts.push_back(list[j]);
					b.conflicts.push_back(list[i]);
				}
			}
		}
	}
}

void JunctionTraffic::BuildFromMap(const Map& map) {
	const auto& vehicleGraph = map.GetVehicleNavGraph();
	const auto& pedestrianGraph = map.GetPedestrianNavGraph();

	for (const RoadJunction* junction : map.GetJunctions()) {
		if (!junction || !junction->GetNode()) continue;
		int junctionId = junction->GetNode()->GetId();

		unordered_set<int> outboundIds;
		for (const RoadJunctionApproach& approach : junction->GetApproaches()) {
			for (const Node* node : approach.vehicleOutbound) outboundIds.insert(node->GetId());
		}
		const auto& approachList = junction->GetApproaches();
		for (size_t approachIndex = 0; approachIndex < approachList.size(); approachIndex++) {
			for (const Node* inbound : approachList[approachIndex].vehicleInbound) {
				auto it = vehicleGraph.find(inbound->GetId());
				if (it == vehicleGraph.end()) continue;
				for (const auto& [neighborId, connection] : it->second) {
					if (outboundIds.count(neighborId)) AddConnection(connection, Kind::Vehicle, junctionId, static_cast<int>(approachIndex));
				}
			}
		}
		// 红绿灯：只有多于两条路的路口才有。
		if (approachList.size() > 2) {
			vector<float> angles;
			vector<bool> hasInbound;
			for (const RoadJunctionApproach& approach : approachList) {
				angles.push_back(approach.angle);
				hasInbound.push_back(!approach.vehicleInbound.empty());
			}
			SetSignalGroups(junctionId, PairApproaches(angles, hasInbound));
		}

		unordered_map<int, int> approachOfAnchor; // 行人锚点id -> 所属approach下标
		const auto& approaches = junction->GetApproaches();
		for (size_t i = 0; i < approaches.size(); i++) {
			for (const Node* node : approaches[i].pedestrianSide) {
				if (node) approachOfAnchor[node->GetId()] = static_cast<int>(i);
			}
		}
		for (const auto& [anchorId, approachIndex] : approachOfAnchor) {
			auto it = pedestrianGraph.find(anchorId);
			if (it == pedestrianGraph.end()) continue;
			for (const auto& [neighborId, connection] : it->second) {
				auto other = approachOfAnchor.find(neighborId);
				if (other == approachOfAnchor.end()) continue;
				if (connection->CalcDistance() > kMaxPedestrianJunctionEdge) continue;
				// 横道两端在同一条道路的两侧：横穿的就是这条路，红绿灯看它的灯色。
				if (other->second == approachIndex) AddConnection(connection, Kind::Crosswalk, junctionId, approachIndex);
				else AddConnection(connection, Kind::Corner, junctionId);
			}
		}
	}
	BuildConflicts();
}

vector<int> JunctionTraffic::PairApproaches(const vector<float>& angles, const vector<bool>& hasInbound) {
	vector<int> group(angles.size(), -1);
	vector<int> left;
	for (size_t i = 0; i < angles.size(); i++) {
		if (i < hasInbound.size() && hasInbound[i]) left.push_back(static_cast<int>(i));
	}
	constexpr float kPi = 3.14159265358979f;
	auto straightness = [&](int a, int b) { // 两条路夹角离180°差多少(弧度，0=正好一条直线)
		float diff = fabsf(angles[a] - angles[b]);
		diff = fmodf(diff, 2.f * kPi);
		if (diff > kPi) diff = 2.f * kPi - diff;
		return kPi - diff;
	};
	int next = 0;
	while (left.size() >= 2) {
		size_t bestI = 0, bestJ = 1;
		float best = FLT_MAX;
		for (size_t i = 0; i < left.size(); i++) {
			for (size_t j = i + 1; j < left.size(); j++) {
				const float value = straightness(left[i], left[j]);
				if (value < best - 1e-5f) { best = value; bestI = i; bestJ = j; }
			}
		}
		group[left[bestI]] = next;
		group[left[bestJ]] = next;
		next++;
		left.erase(left.begin() + bestJ); // 先删后面的，前面的下标不变
		left.erase(left.begin() + bestI);
	}
	if (!left.empty()) group[left.front()] = next;
	return group;
}

void JunctionTraffic::SetSignalGroups(int junctionId, const vector<int>& groupOfApproach) {
	int count = 0;
	for (int g : groupOfApproach) count = max(count, g + 1);
	if (count <= 0) {
		signals.erase(junctionId);
		return;
	}
	SignalPlan& plan = signals[junctionId];
	plan.groupOfApproach = groupOfApproach;
	plan.groupCount = count;
}

bool JunctionTraffic::HasSignal(int junctionId) const {
	return signals.count(junctionId) != 0;
}

void JunctionTraffic::SetSignalActive(int junctionId, bool active, double now) {
	auto it = signals.find(junctionId);
	if (it == signals.end()) return;
	if (active && !it->second.active) it->second.cycleStart = now; // 每次激活从第0组绿灯开始
	it->second.active = active;
}

bool JunctionTraffic::IsSignalActive(int junctionId) const {
	auto it = signals.find(junctionId);
	return it != signals.end() && it->second.active;
}

JunctionTraffic::Signal JunctionTraffic::GetSignal(int junctionId, int approach, double now) const {
	auto it = signals.find(junctionId);
	if (it == signals.end() || !it->second.active) return Signal::None;
	const SignalPlan& plan = it->second;
	if (approach < 0 || approach >= static_cast<int>(plan.groupOfApproach.size())) return Signal::None;
	const int group = plan.groupOfApproach[approach];
	if (group < 0) return Signal::None;
	const double slot = kGreenSeconds + kYellowSeconds;
	double t = fmod(max(0.0, now - plan.cycleStart), slot * plan.groupCount);
	const int current = min(plan.groupCount - 1, static_cast<int>(t / slot));
	if (current != group) return Signal::Red;
	return t - current * slot < kGreenSeconds ? Signal::Green : Signal::Yellow;
}

const vector<int>* JunctionTraffic::GetSignalGroups(int junctionId) const {
	auto it = signals.find(junctionId);
	return it != signals.end() ? &it->second.groupOfApproach : nullptr;
}

bool JunctionTraffic::IsManaged(const Connection* connection) const {
	return index.count(connection) != 0;
}

bool JunctionTraffic::GetKind(const Connection* connection, Kind& out) const {
	auto it = index.find(connection);
	if (it == index.end()) return false;
	out = entries[it->second].kind;
	return true;
}

bool JunctionTraffic::AreConflicting(const Connection* a, const Connection* b) const {
	auto ia = index.find(a), ib = index.find(b);
	if (ia == index.end() || ib == index.end()) return false;
	const vector<int>& list = entries[ia->second].conflicts;
	return find(list.begin(), list.end(), ib->second) != list.end();
}

size_t JunctionTraffic::GetConnectionCount() const {
	return entries.size();
}

int64_t JunctionTraffic::CellKey(int cx, int cy) {
	return (static_cast<int64_t>(cx) << 32) ^ static_cast<uint32_t>(cy);
}

const Connection* JunctionTraffic::FindCrosswalk(float ax, float ay, float bx, float by, float tolerance) const {
	const int cx = static_cast<int>(floorf((ax + bx) * 0.5f)), cy = static_cast<int>(floorf((ay + by) * 0.5f));
	const float tolSq = tolerance * tolerance;
	auto closeTo = [tolSq](float x0, float y0, float x1, float y1) { return (x0 - x1) * (x0 - x1) + (y0 - y1) * (y0 - y1) <= tolSq; };
	for (int dx = -1; dx <= 1; dx++) {
		for (int dy = -1; dy <= 1; dy++) {
			auto it = crosswalkGrid.find(CellKey(cx + dx, cy + dy));
			if (it == crosswalkGrid.end()) continue;
			for (int i : it->second) {
				const Point& p = entries[i].polyline.front();
				const Point& q = entries[i].polyline.back();
				if ((closeTo(ax, ay, p.x, p.y) && closeTo(bx, by, q.x, q.y)) || (closeTo(ax, ay, q.x, q.y) && closeTo(bx, by, p.x, p.y))) {
					return entries[i].connection;
				}
			}
		}
	}
	return nullptr;
}

vector<JunctionTraffic::PathSpan> JunctionTraffic::FindVehicleSpans(const vector<pair<float, float>>& points, float tolerance) const {
	vector<PathSpan> spans;
	const float tolSq = tolerance * tolerance;
	auto closeTo = [tolSq](const pair<float, float>& p, const Point& q) {
		return (p.first - q.x) * (p.first - q.x) + (p.second - q.y) * (p.second - q.y) <= tolSq;
	};
	constexpr int kMaxSpanPoints = 256; // 一条路口连线在路径里最多占这么多个点(实际2~64)
	const int count = static_cast<int>(points.size());
	for (int i = 0; i < count; i++) {
		const int cx = static_cast<int>(floorf(points[i].first)), cy = static_cast<int>(floorf(points[i].second));
		int bestEntry = -1, bestEnd = count;
		for (int dx = -1; dx <= 1; dx++) {
			for (int dy = -1; dy <= 1; dy++) {
				auto it = vehicleStartGrid.find(CellKey(cx + dx, cy + dy));
				if (it == vehicleStartGrid.end()) continue;
				for (int e : it->second) {
					if (!closeTo(points[i], entries[e].polyline.front())) continue;
					// 同一条驶入车道有左转/直行/右转几条连线：看终点先在路径里出现的是哪一条。
					const int limit = min(count, i + kMaxSpanPoints);
					for (int j = i + 1; j < limit && j < bestEnd; j++) {
						if (closeTo(points[j], entries[e].polyline.back())) { bestEntry = e; bestEnd = j; break; }
					}
				}
			}
		}
		if (bestEntry < 0) continue;
		spans.push_back({ entries[bestEntry].connection, i, bestEnd });
		i = bestEnd - 1; // 连线终点(驶出锚点)之后的点从下一个开始找
	}
	return spans;
}

void JunctionTraffic::Purge(Entry& entry, double now) const {
	for (auto it = entry.holders.begin(); it != entry.holders.end();) {
		if (it->second.expire < now) it = entry.holders.erase(it); else ++it;
	}
	for (auto it = entry.waiters.begin(); it != entry.waiters.end();) {
		if (it->second.expire < now) it = entry.waiters.erase(it); else ++it;
	}
}

float JunctionTraffic::ReservedLength(int exitId, double now) const {
	auto it = entriesByExit.find(exitId);
	if (it == entriesByExit.end()) return 0.f;
	float total = 0.f;
	for (int i : it->second) {
		for (const auto& [entity, holder] : entries[i].holders) {
			if (holder.expire >= now) total += holder.length;
		}
	}
	return total;
}

bool JunctionTraffic::HasConflictHolder(const Entry& entry) const {
	for (int c : entry.conflicts) {
		if (!entries[c].holders.empty()) return true;
	}
	return false;
}

bool JunctionTraffic::HasEarlierConflictWaiter(const Entry& entry, double t0, int64_t entity, bool pedestriansOnly) const {
	for (int c : entry.conflicts) {
		for (const auto& [other, waiter] : entries[c].waiters) {
			if (other == entity) continue;
			if (pedestriansOnly && GetEntityKind(other) != EntityKind::Pedestrian) continue;
			if (waiter.t0 < t0 || (waiter.t0 == t0 && other < entity)) return true;
		}
	}
	return false;
}

bool JunctionTraffic::Request(const Connection* connection, int64_t entity, double now, float length, float exitFree) {
	auto found = index.find(connection);
	if (found == index.end()) return true;
	Entry& entry = entries[found->second];

	Purge(entry, now);
	for (int c : entry.conflicts) Purge(entries[c], now);

	auto holder = entry.holders.find(entity);
	if (holder != entry.holders.end()) { // 已放行的续租：粘性，不再重新检查
		holder->second.expire = now + config.leaseSeconds;
		return true;
	}

	const bool isVehicle = GetEntityKind(entity) == EntityKind::Vehicle;
	// 红绿灯：已激活的路口里，自己那条道路不是绿灯就不放(黄灯也不放新车，已经放行的车上面续租照常走完)。
	// 被拒的车不进排队：它的排队时间比绿灯方早，进了排队会让绿灯方一直等一辆走不了的车。
	// 行人反过来：横穿的那条路是红灯(那条路的车停着)才放，绿灯/黄灯都不放；已经在横道上的人上面续租照常走完。
	if (entry.approach >= 0) {
		const Signal signal = GetSignal(entry.junctionId, entry.approach, now);
		const bool blocked = entry.kind == Kind::Vehicle
			? signal == Signal::Red || signal == Signal::Yellow
			: signal == Signal::Green || signal == Signal::Yellow;
		if (blocked) {
			entry.waiters.erase(entity);
			return false;
		}
	}
	auto waiter = entry.waiters.find(entity);
	const double t0 = waiter != entry.waiters.end() ? waiter->second.t0 : now;
	const bool exitOk = !isVehicle || exitFree - ReservedLength(entry.endId, now) >= length;

	bool grant = exitOk;
	if (grant) {
		if (!entry.holders.empty()) {
			// 连线已开：同连线的新请求直接放行；只有更早在等的冲突方的人能拦住车，
			// 以及(可选)连续开放太久时让给冲突方。
			if (isVehicle && HasEarlierConflictWaiter(entry, t0, entity, true)) grant = false;
			else if (config.maxOpenSeconds > 0.f && now - entry.openSince > config.maxOpenSeconds
				&& HasEarlierConflictWaiter(entry, t0, entity, false)) grant = false;
		}
		else if (HasConflictHolder(entry) || HasEarlierConflictWaiter(entry, t0, entity, false)) {
			grant = false;
		}
	}

	if (grant) {
		if (entry.holders.empty()) entry.openSince = now;
		entry.waiters.erase(entity);
		entry.holders[entity] = { now + config.leaseSeconds, length };
		return true;
	}
	entry.waiters[entity] = { t0, now + config.leaseSeconds };
	return false;
}

void JunctionTraffic::Leave(const Connection* connection, int64_t entity) {
	auto found = index.find(connection);
	if (found == index.end()) return;
	entries[found->second].holders.erase(entity);
	entries[found->second].waiters.erase(entity);
}

bool JunctionTraffic::Holds(const Connection* connection, int64_t entity) const {
	auto found = index.find(connection);
	return found != index.end() && entries[found->second].holders.count(entity) != 0;
}

bool JunctionTraffic::CheckInvariant(double now) const {
	for (const Entry& entry : entries) {
		bool open = false;
		for (const auto& [entity, holder] : entry.holders) open |= holder.expire >= now;
		if (!open) continue;
		for (int c : entry.conflicts) {
			for (const auto& [entity, holder] : entries[c].holders) {
				if (holder.expire >= now) return false;
			}
		}
	}
	return true;
}
