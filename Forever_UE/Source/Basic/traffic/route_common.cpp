#include "route_common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>

using namespace std;

// 射线从(x,y)沿(dirX,dirY)方向和[0,sizeX]x[0,sizeY]地图边界的交点——dirX/dirY不会同时为0
// (接口朝向不可能是零向量)。
pair<float, float> RayBoxIntersection(float x, float y, float dirX, float dirY, int sizeX, int sizeY) {
	float tBest = 1e9f;
	if (dirX > 0.f) tBest = min(tBest, (static_cast<float>(sizeX) - x) / dirX);
	else if (dirX < 0.f) tBest = min(tBest, (0.f - x) / dirX);
	if (dirY > 0.f) tBest = min(tBest, (static_cast<float>(sizeY) - y) / dirY);
	else if (dirY < 0.f) tBest = min(tBest, (0.f - y) / dirY);
	if (tBest < 0.f || tBest > 1e8f) tBest = 0.f; // 理论不会发生，防御性兜底(站点已经在图内)
	return { x + dirX * tBest, y + dirY * tBest };
}

TrackSegment MakeBezierSegment(float x1, float y1, float z1, float h1x, float h1y,
	float x2, float y2, float z2, float h2x, float h2y, float arm, bool terrainFollow) {
	TrackSegment seg;
	seg.x1 = x1; seg.y1 = y1; seg.z1 = z1;
	seg.x2 = x2; seg.y2 = y2; seg.z2 = z2;
	if (arm <= 0.f) {
		seg.c1x = x1 + (x2 - x1) / 3.f; seg.c1y = y1 + (y2 - y1) / 3.f;
		seg.c2x = x1 + (x2 - x1) * 2.f / 3.f; seg.c2y = y1 + (y2 - y1) * 2.f / 3.f;
	}
	else {
		seg.c1x = x1 + h1x * arm; seg.c1y = y1 + h1y * arm;
		seg.c2x = x2 - h2x * arm; seg.c2y = y2 - h2y * arm;
	}
	seg.c1z = z1; seg.c2z = z2;
	seg.terrainFollow = terrainFollow;
	return seg;
}

vector<CurveSample> SampleBezier(const TrackSegment& curve, float step, float fallbackX, float fallbackY) {
	auto pointAt = [&](float t, float& x, float& y) {
		float m = 1.f - t;
		x = m * m * m * curve.x1 + 3.f * m * m * t * curve.c1x + 3.f * m * t * t * curve.c2x + t * t * t * curve.x2;
		y = m * m * m * curve.y1 + 3.f * m * m * t * curve.c1y + 3.f * m * t * t * curve.c2y + t * t * t * curve.y2;
	};
	auto tangentAt = [&](float t, float& tx, float& ty) {
		float m = 1.f - t;
		tx = 3.f * m * m * (curve.c1x - curve.x1) + 6.f * m * t * (curve.c2x - curve.c1x) + 3.f * t * t * (curve.x2 - curve.c2x);
		ty = 3.f * m * m * (curve.c1y - curve.y1) + 6.f * m * t * (curve.c2y - curve.c1y) + 3.f * t * t * (curve.y2 - curve.c2y);
		float length = sqrtf(tx * tx + ty * ty);
		if (length < 1e-6f) { tx = fallbackX; ty = fallbackY; length = sqrtf(tx * tx + ty * ty); }
		if (length > 1e-6f) { tx /= length; ty /= length; }
	};

	float chord = sqrtf((curve.x2 - curve.x1) * (curve.x2 - curve.x1) + (curve.y2 - curve.y1) * (curve.y2 - curve.y1));
	int fine = max(64, static_cast<int>(ceilf(chord * 16.f)));
	vector<float> fineArc(fine + 1, 0.f);
	float px, py;
	pointAt(0.f, px, py);
	for (int i = 1; i <= fine; i++) {
		float x, y;
		pointAt(static_cast<float>(i) / static_cast<float>(fine), x, y);
		fineArc[i] = fineArc[i - 1] + sqrtf((x - px) * (x - px) + (y - py) * (y - py));
		px = x; py = y;
	}
	float total = fineArc[fine];
	int count = max(1, static_cast<int>(ceilf(total / max(step, 1e-3f))));

	vector<CurveSample> samples(count + 1);
	int cursor = 0;
	for (int i = 0; i <= count; i++) {
		float target = total * static_cast<float>(i) / static_cast<float>(count);
		while (cursor + 1 < fine && fineArc[cursor + 1] < target) cursor++;
		float span = fineArc[cursor + 1] - fineArc[cursor];
		float f = span > 1e-9f ? (target - fineArc[cursor]) / span : 0.f;
		float t = (static_cast<float>(cursor) + f) / static_cast<float>(fine);
		pointAt(t, samples[i].x, samples[i].y);
		tangentAt(t, samples[i].tx, samples[i].ty);
	}
	samples[0].x = curve.x1; samples[0].y = curve.y1;
	samples[count].x = curve.x2; samples[count].y = curve.y2;
	return samples;
}

vector<TrackSegment> OffsetRail(const vector<CurveSample>& samples, float offset, bool reverse,
	float z1, float z2, bool terrainFollow) {
	size_t n = samples.size();
	vector<TrackSegment> segments;
	if (n < 2) return segments;

	// 按行驶顺序排好的偏移点和行驶方向。
	vector<float> px(n), py(n), hx(n), hy(n);
	for (size_t k = 0; k < n; k++) {
		const CurveSample& s = samples[reverse ? n - 1 - k : k];
		px[k] = s.x - s.ty * offset;
		py[k] = s.y + s.tx * offset;
		hx[k] = reverse ? -s.tx : s.tx;
		hy[k] = reverse ? -s.ty : s.ty;
	}
	for (size_t k = 0; k + 1 < n; k++) {
		float za = z1 + (z2 - z1) * static_cast<float>(k) / static_cast<float>(n - 1);
		float zb = z1 + (z2 - z1) * static_cast<float>(k + 1) / static_cast<float>(n - 1);
		float arm = sqrtf((px[k + 1] - px[k]) * (px[k + 1] - px[k]) + (py[k + 1] - py[k]) * (py[k + 1] - py[k])) / 3.f;
		segments.push_back(MakeBezierSegment(px[k], py[k], za, hx[k], hy[k], px[k + 1], py[k + 1], zb, hx[k + 1], hy[k + 1], arm, terrainFollow));
	}
	return segments;
}


// ================= 手动线路公共部分 =================
TrackSegment ManualAxis(const TransitData& data, int connIndex) {
	const TransitData::Conn& conn = data.conns[connIndex];
	const TransitData::Node& a = data.nodes[conn.from];
	const TransitData::Node& b = data.nodes[conn.to];
	TrackSegment seg;
	seg.x1 = a.x; seg.y1 = a.y; seg.z1 = a.z;
	seg.x2 = b.x; seg.y2 = b.y; seg.z2 = b.z;
	if (conn.hasCtrl) { // 二次Bezier升阶成三次
		seg.c1x = a.x + (conn.cx - a.x) * 2.f / 3.f; seg.c1y = a.y + (conn.cy - a.y) * 2.f / 3.f;
		seg.c2x = b.x + (conn.cx - b.x) * 2.f / 3.f; seg.c2y = b.y + (conn.cy - b.y) * 2.f / 3.f;
	}
	else {
		seg.c1x = a.x + (b.x - a.x) / 3.f; seg.c1y = a.y + (b.y - a.y) / 3.f;
		seg.c2x = a.x + (b.x - a.x) * 2.f / 3.f; seg.c2y = a.y + (b.y - a.y) * 2.f / 3.f;
	}
	seg.c1z = a.z; seg.c2z = b.z; // 控制点z取端点z：z(t)=z0+(z1-z0)*smoothstep(t)
	return seg;
}

void CubicPoint(const TrackSegment& c, float t, float& x, float& y, float& z) {
	float m = 1.f - t;
	float w0 = m * m * m, w1 = 3.f * m * m * t, w2 = 3.f * m * t * t, w3 = t * t * t;
	x = w0 * c.x1 + w1 * c.c1x + w2 * c.c2x + w3 * c.x2;
	y = w0 * c.y1 + w1 * c.c1y + w2 * c.c2y + w3 * c.y2;
	z = w0 * c.z1 + w1 * c.c1z + w2 * c.c2z + w3 * c.z2;
}

void CubicTangent(const TrackSegment& c, float t, float& tx, float& ty) {
	float m = 1.f - t;
	tx = 3.f * m * m * (c.c1x - c.x1) + 6.f * m * t * (c.c2x - c.c1x) + 3.f * t * t * (c.x2 - c.c2x);
	ty = 3.f * m * m * (c.c1y - c.y1) + 6.f * m * t * (c.c2y - c.c1y) + 3.f * t * t * (c.y2 - c.c2y);
}

namespace {
	struct P3 { float x, y, z; };
	P3 Lerp3(const P3& a, const P3& b, float t) { return P3{ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t }; }
	// de Casteljau：把q在t处切成两半。
	void Split3(const array<P3, 4>& q, float t, array<P3, 4>& left, array<P3, 4>& right) {
		P3 ab = Lerp3(q[0], q[1], t), bc = Lerp3(q[1], q[2], t), cd = Lerp3(q[2], q[3], t);
		P3 abc = Lerp3(ab, bc, t), bcd = Lerp3(bc, cd, t);
		P3 abcd = Lerp3(abc, bcd, t);
		left = { q[0], ab, abc, abcd };
		right = { abcd, bcd, cd, q[3] };
	}
}

TrackSegment SubCubic(const TrackSegment& c, float t0, float t1) {
	array<P3, 4> q = { P3{ c.x1, c.y1, c.z1 }, P3{ c.c1x, c.c1y, c.c1z }, P3{ c.c2x, c.c2y, c.c2z }, P3{ c.x2, c.y2, c.z2 } };
	array<P3, 4> left, right;
	array<P3, 4> sub = q;
	if (t1 < 1.f - 1e-6f) { Split3(sub, t1, left, right); sub = left; }
	if (t0 > 1e-6f) { Split3(sub, t0 / max(t1, 1e-6f), left, right); sub = right; }
	TrackSegment seg = c;
	seg.x1 = sub[0].x; seg.y1 = sub[0].y; seg.z1 = sub[0].z;
	seg.c1x = sub[1].x; seg.c1y = sub[1].y; seg.c1z = sub[1].z;
	seg.c2x = sub[2].x; seg.c2y = sub[2].y; seg.c2z = sub[2].z;
	seg.x2 = sub[3].x; seg.y2 = sub[3].y; seg.z2 = sub[3].z;
	return seg;
}

pair<float, float> ManualEndDirection(const TransitData& data, int connIndex, int node) {
	TrackSegment axis = ManualAxis(data, connIndex);
	const TransitData::Conn& conn = data.conns[connIndex];
	float tx, ty;
	if (conn.from == node) { CubicTangent(axis, 0.f, tx, ty); }
	else { CubicTangent(axis, 1.f, tx, ty); tx = -tx; ty = -ty; }
	float length = sqrtf(tx * tx + ty * ty);
	if (length < 1e-6f) return { 1.f, 0.f };
	return { tx / length, ty / length };
}

namespace {
	float ConnLengthApprox(const TransitData& data, int connIndex) {
		TrackSegment axis = ManualAxis(data, connIndex);
		float length = 0.f, px = axis.x1, py = axis.y1;
		for (int i = 1; i <= 16; i++) {
			float x, y, z;
			CubicPoint(axis, static_cast<float>(i) / 16.f, x, y, z);
			length += sqrtf((x - px) * (x - px) + (y - py) * (y - py));
			px = x; py = y;
		}
		return length;
	}

	// 本route连线图上node到node的最短路：steps={连线下标, 是否沿from->to走}。
	bool ManualPath(const TransitData& data, const vector<int>& routeConns, const vector<float>& lengths, int from, int to, vector<pair<int, bool>>& steps) {
		steps.clear();
		if (from == to) return true;
		const int n = static_cast<int>(data.nodes.size());
		vector<float> dist(n, 1e30f);
		vector<int> viaConn(n, -1), prevNode(n, -1);
		vector<char> viaForward(n, 0), done(n, 0);
		dist[from] = 0.f;
		for (;;) {
			int cur = -1;
			for (int i = 0; i < n; i++) if (!done[i] && dist[i] < 1e29f && (cur < 0 || dist[i] < dist[cur])) cur = i;
			if (cur < 0) return false;
			if (cur == to) break;
			done[cur] = 1;
			for (size_t k = 0; k < routeConns.size(); k++) {
				const TransitData::Conn& conn = data.conns[routeConns[k]];
				int other = conn.from == cur ? conn.to : (conn.to == cur ? conn.from : -1);
				if (other < 0 || done[other]) continue;
				float nd = dist[cur] + lengths[k];
				if (nd < dist[other]) { dist[other] = nd; viaConn[other] = routeConns[k]; viaForward[other] = conn.from == cur ? 1 : 0; prevNode[other] = cur; }
			}
		}
		for (int cur = to; cur != from; cur = prevNode[cur]) steps.push_back({ viaConn[cur], viaForward[cur] != 0 });
		reverse(steps.begin(), steps.end());
		return true;
	}

	pair<float, float> Unit(float dx, float dy) {
		float length = sqrtf(dx * dx + dy * dy);
		return length > 1e-6f ? make_pair(dx / length, dy / length) : make_pair(1.f, 0.f);
	}

	int BestPort(const RouteStationInfo& info, float dx, float dy) {
		int best = 0;
		float bestDot = -1e30f;
		for (int t = 0; t < 2; t++) {
			auto heading = Unit(info.inlets[t].dirX, info.inlets[t].dirY);
			float dot = heading.first * dx + heading.second * dy;
			if (dot > bestDot) { bestDot = dot; best = t; }
		}
		return best;
	}
}

ManualPlan PlanManualRoute(const TransitData& data, const string& routeId, const vector<RouteStationInfo>& stations, bool allowTrackChange) {
	ManualPlan plan;
	vector<int> routeConns;
	vector<float> lengths;
	for (size_t i = 0; i < data.conns.size(); i++) {
		if (data.conns[i].route != routeId) continue;
		routeConns.push_back(static_cast<int>(i));
		lengths.push_back(ConnLengthApprox(data, static_cast<int>(i)));
	}
	// 预设下标 -> stations下标(只收端口齐全的站)
	map<int, int> stationOfPreset;
	for (size_t i = 0; i < stations.size(); i++) {
		const RouteStationInfo& info = stations[i];
		if (info.presetIndex < 0 || info.inlets.size() < 2 || info.outlets.size() < 2 || info.stops.size() < 2) continue;
		stationOfPreset[info.presetIndex] = static_cast<int>(i);
	}

	set<int> usedConns;
	set<array<int, 3>> turnSet;
	set<string> gapKeys;
	set<int> hubNodes, uturnNodes;
	auto portKey = [](const ManualPort& p) {
		return to_string(static_cast<int>(p.kind)) + ":" + to_string(p.station) + ":" + to_string(p.track) + ":" + to_string(p.node) + ":" + to_string(p.conn);
	};
	auto addGap = [&](const ManualPort& from, const ManualPort& to, bool withinStation) {
		if (!gapKeys.insert(portKey(from) + ">" + portKey(to)).second) return;
		ManualGap gap;
		gap.from = from; gap.to = to; gap.withinStation = withinStation;
		plan.gaps.push_back(gap);
	};

	for (size_t loopIndex = 0; loopIndex < data.loops.size(); loopIndex++) {
		const TransitData::Loop& loop = data.loops[loopIndex];
		if (loop.route != routeId) continue;
		struct Item { bool station = false; int st = -1; bool stop = false; int node = -1; float x = 0.f, y = 0.f; };
		vector<Item> items;
		for (const TransitData::Item& source : loop.items) {
			Item item;
			if (source.preset >= 0) {
				auto it = stationOfPreset.find(source.preset);
				if (it == stationOfPreset.end()) {
					plan.warnings.push_back("loop " + to_string(loopIndex) + ": preset " + to_string(source.preset) + " has no usable station of this route (not placed / not a station / ports missing), item dropped");
					continue;
				}
				item.station = true; item.st = it->second; item.stop = source.stop;
				item.x = stations[it->second].x; item.y = stations[it->second].y;
			}
			else {
				if (source.node < 0 || source.node >= static_cast<int>(data.nodes.size())) continue;
				item.node = source.node; item.x = data.nodes[source.node].x; item.y = data.nodes[source.node].y;
			}
			items.push_back(item);
		}
		bool anyStop = false;
		for (const Item& item : items) if (item.station && item.stop) anyStop = true;
		if (items.size() < 2) { plan.warnings.push_back("loop " + to_string(loopIndex) + ": fewer than 2 usable items, loop dropped"); continue; }
		if (!anyStop) { plan.warnings.push_back("loop " + to_string(loopIndex) + ": no stopping station, loop dropped"); continue; }

		const int n = static_cast<int>(items.size());
		struct Leg { vector<pair<int, bool>> steps; bool rail = false; };
		vector<Leg> legs(n);
		for (int i = 0; i < n; i++) {
			const Item& a = items[i];
			const Item& b = items[(i + 1) % n];
			if (a.station || b.station) continue;
			if (ManualPath(data, routeConns, lengths, a.node, b.node, legs[i].steps) && !legs[i].steps.empty()) legs[i].rail = true;
		}

		vector<ManualPort> entry(n), exitPort(n);
		vector<int> trackIn(n, 0), trackOut(n, 0);
		for (int i = 0; i < n; i++) {
			const Item& item = items[i];
			const Item& prev = items[(i + n - 1) % n];
			const Item& next = items[(i + 1) % n];
			if (item.station) {
				auto din = Unit(item.x - prev.x, item.y - prev.y);
				auto dout = Unit(next.x - item.x, next.y - item.y);
				const RouteStationInfo& info = stations[item.st];
				int tIn, tOut;
				if (allowTrackChange && item.stop) { tIn = BestPort(info, din.first, din.second); tOut = BestPort(info, dout.first, dout.second); }
				else tIn = tOut = BestPort(info, din.first + dout.first, din.second + dout.second);
				trackIn[i] = tIn; trackOut[i] = tOut;
				entry[i].kind = ManualPort::StationIn; entry[i].station = item.st; entry[i].track = tIn;
				exitPort[i].kind = ManualPort::StationOut; exitPort[i].station = item.st; exitPort[i].track = tOut;
				continue;
			}
			const Leg& prevLeg = legs[(i + n - 1) % n];
			const Leg& nextLeg = legs[i];
			int cin = prevLeg.rail ? prevLeg.steps.back().first : -1;
			int cout = nextLeg.rail ? nextLeg.steps.front().first : -1;
			ManualPort in, out;
			in.node = out.node = item.node;
			if (cin >= 0) { in.kind = ManualPort::NodeIn; in.conn = cin; }
			else if (cout >= 0) { in.kind = ManualPort::NodeOut; in.conn = cout; }
			else {
				in.kind = ManualPort::Hub;
				auto h = Unit(next.x - prev.x, next.y - prev.y);
				in.hx = h.first; in.hy = h.second;
			}
			bool uturn = false; // 前后都是缺口且折回(终点节点夹在同一个站/同一侧之间)：到达和离开要分开，中间掉头
			if (cin < 0 && cout < 0) {
				auto hin = Unit(item.x - prev.x, item.y - prev.y), hout = Unit(next.x - item.x, next.y - item.y);
				if (hin.first * hout.first + hin.second * hout.second < -0.5f) {
					uturn = true;
					in.hx = hin.first; in.hy = hin.second;
				}
			}
			if (cout >= 0) { out.kind = ManualPort::NodeOut; out.conn = cout; }
			else if (cin >= 0) { out.kind = ManualPort::NodeIn; out.conn = cin; }
			else if (uturn) { out = in; out.kind = ManualPort::HubOut; out.hx = -in.hx; out.hy = -in.hy; }
			else out = in;
			entry[i] = in; exitPort[i] = out;
			if (in.kind == ManualPort::Hub && hubNodes.insert(item.node).second) plan.hubs.push_back(in);
			if (uturn && uturnNodes.insert(item.node).second) plan.hubs.push_back(out);
			if (cin >= 0 && cout >= 0) turnSet.insert({ item.node, cin, cout });
		}

		// 腿：有实体线路的沿线路(记用到的连线和途经节点的转向)，否则补缺口
		for (int i = 0; i < n; i++) {
			if (legs[i].rail) {
				const auto& steps = legs[i].steps;
				for (size_t k = 0; k < steps.size(); k++) {
					usedConns.insert(steps[k].first);
					if (k + 1 < steps.size()) {
						const TransitData::Conn& conn = data.conns[steps[k].first];
						int via = steps[k].second ? conn.to : conn.from;
						turnSet.insert({ via, steps[k].first, steps[k + 1].first });
					}
				}
			}
			else addGap(exitPort[i], entry[(i + 1) % n], false);
		}

		// 线路条目
		vector<ManualLineEntry> entries;
		for (int i = 0; i < n; i++) {
			const Item& item = items[i];
			if (item.station) {
				if (!item.stop) continue; // 过站不停：靠两侧的缺口连线穿过站，不进条目
				ManualLineEntry e;
				e.station = item.st; e.track = trackIn[i];
				entries.push_back(e);
				if (trackOut[i] != trackIn[i]) {
					ManualPort taxiFrom, taxiTo;
					taxiFrom.kind = ManualPort::StationOut; taxiFrom.station = item.st; taxiFrom.track = trackIn[i];
					taxiTo.kind = ManualPort::StationIn; taxiTo.station = item.st; taxiTo.track = trackOut[i];
					addGap(taxiFrom, taxiTo, true);
					ManualLineEntry e2;
					e2.station = item.st; e2.track = trackOut[i];
					entries.push_back(e2);
				}
			}
			else {
				ManualLineEntry e;
				e.via = entry[i];
				entries.push_back(e);
			}
		}
		plan.lines.push_back(std::move(entries));
	}

	plan.conns.assign(usedConns.begin(), usedConns.end());
	plan.turns.assign(turnSet.begin(), turnSet.end());
	return plan;
}
