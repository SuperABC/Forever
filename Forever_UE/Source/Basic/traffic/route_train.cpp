#include "route_train.h"

#include "route_common.h"

#include <algorithm>
#include <cmath>
#include <cfloat>

using namespace std;

// ---- TrainRoute::BuildTrack：过山判定和分段常量照抄roadnet_jing.cpp(JingRoadnet::buildArm)，
// 两边数值需要保持一致，改动时一起看。 ----
#define MOUNTAIN_MAX_SLOPE 0.5f
#define TERRAIN_FIT_FALLOFF 1.5f
#define TUNNEL_HEIGHT -1.f
#define TUNNEL_HATCH_LENGTH 8.f
#define TRACK_SAMPLE_STEP 1.f

// 车站两端各留这么多个采样点保持水平，不在车站范围内爬坡/下坡。
#define TRACK_STATION_FLAT_SAMPLES 2

namespace {
	float SmoothStep(float u) { return u * u * (3.f - 2.f * u); }
}

int TrainRoute::count = 0;

TrainRoute::TrainRoute() : id(count++) {
}

const char* TrainRoute::GetName() {
	name = "TrainRoute" + to_string(id);
	return name.data();
}

void TrainRoute::SetProperty() {
	stationType = "train";
	vehicleType = "vehicle_train";
	useRoadnet = false;
	instantiate = true;
	// 轨道mesh沿X轴延伸、轴心在起点边，长约250cm=0.25地图单位(1地图单位=10m)。
	// 没有隧道专用资产(trackTunnelMesh留空)，隧道内复用trackMesh。
	trackMesh = "/Game/3rdParty/SM_ufekaevdw_tier_3/StaticMeshes/SM_ufekaevdw_tier_3.SM_ufekaevdw_tier_3";
	trackTunnelMesh = "";
	trackUnit = 0.25f;
	trackWidth = 0.8f; // 比轨道本身宽，给列车和隧道口留余量

	// 校准依据见route_train.md：跑一圈约170真实秒、进站停约5真实秒。
	speed = 0.1f;
	dwellSeconds = 600.f;

	// 火车体型大，启动/刹车比普通载具更缓。
	easeSeconds = 40.f;
}

void TrainRoute::LayoutRoute(const vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) {
	LayoutDualTrackStationLoop(interfaces, sizeX, sizeY, false, 0.f, edgeStations, links, lines);
}

void TrainRoute::BuildTrack(const vector<RouteTrackLinkEnds>& linkEnds,
	const function<const string&(int, int)>& getTerrain,
	const function<pair<bool, float>(int, int)>& getWater,
	const function<float(float, float)>& getHeight) {
	trackEdges.assign(linkEnds.size(), TrackEdgeGeometry());

	for (size_t linkIndex = 0; linkIndex < linkEnds.size(); linkIndex++) {
		const RouteStationInfo& a = linkEnds[linkIndex].from;
		const RouteStationInfo& b = linkEnds[linkIndex].to;

		// ---- 底线：和Core单段贝塞尔(Route::BuildEdgeGeometry)同一条平面曲线，按弧长等距采样。 ----
		float c1x = a.x + a.departDirX * controlLength, c1y = a.y + a.departDirY * controlLength;
		float c2x = b.x - b.arriveDirX * controlLength, c2y = b.y - b.arriveDirY * controlLength;
		auto bezier = [&](float t, float& x, float& y) {
			float m = 1.f - t;
			x = m * m * m * a.x + 3.f * m * m * t * c1x + 3.f * m * t * t * c2x + t * t * t * b.x;
			y = m * m * m * a.y + 3.f * m * m * t * c1y + 3.f * m * t * t * c2y + t * t * t * b.y;
		};

		float chord = sqrtf((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
		int fine = max(16, static_cast<int>(ceilf(chord * 8.f)));
		vector<float> fineT(fine + 1), fineArc(fine + 1);
		float px, py;
		bezier(0.f, px, py);
		fineT[0] = 0.f; fineArc[0] = 0.f;
		for (int i = 1; i <= fine; i++) {
			float x, y;
			float t = static_cast<float>(i) / static_cast<float>(fine);
			bezier(t, x, y);
			fineT[i] = t;
			fineArc[i] = fineArc[i - 1] + sqrtf((x - px) * (x - px) + (y - py) * (y - py));
			px = x; py = y;
		}
		float totalLength = fineArc[fine];
		if (totalLength < 1e-3f) continue;

		int count = max(1, static_cast<int>(ceilf(totalLength / TRACK_SAMPLE_STEP)));
		vector<float> sx(count + 1), sy(count + 1), arc(count + 1);
		{
			int cursor = 0;
			for (int i = 0; i <= count; i++) {
				float target = totalLength * static_cast<float>(i) / static_cast<float>(count);
				while (cursor + 1 < fine && fineArc[cursor + 1] < target) cursor++;
				float span = fineArc[cursor + 1] - fineArc[cursor];
				float f = span > 1e-9f ? (target - fineArc[cursor]) / span : 0.f;
				bezier(fineT[cursor] + (fineT[cursor + 1] - fineT[cursor]) * f, sx[i], sy[i]);
				arc[i] = target;
			}
			sx[0] = a.x; sy[0] = a.y; sx[count] = b.x; sy[count] = b.y;
		}

		// ---- 第一遍：每个采样点的地形类型和高度。getTerrain返回""(出图)按平地处理——地图边缘站点正好
		// 落在边界上。 ----
		vector<string> terr(count + 1);
		vector<float> heights(count + 1);
		for (int i = 0; i <= count; i++) {
			terr[i] = getTerrain(static_cast<int>(floorf(sx[i])), static_cast<int>(floorf(sy[i])));
			heights[i] = getHeight(sx[i], sy[i]);
		}
		auto isGood = [&](int k) { return terr[k] == "" || terr[k] == "plain" || terr[k] == "construction"; };
		auto allGood = [&](int from, int to) {
			for (int i = from; i <= to; i++) if (!isGood(i)) return false;
			return true;
		};

		// ---- 切出每座山并决定做法(抬升/隧道)，规则同buildArm。 ----
		struct TunnelSpan { int p; int tIn; int tOut; int q; };
		vector<TunnelSpan> tunnels;
		vector<bool> raised(count + 1, false);
		const int rampSamples = static_cast<int>(ceilf(TUNNEL_HATCH_LENGTH / TRACK_SAMPLE_STEP));
		const int lastMountainSample = count - TRACK_STATION_FLAT_SAMPLES;
		int lastQ = TRACK_STATION_FLAT_SAMPLES;
		for (int k = TRACK_STATION_FLAT_SAMPLES; k <= lastMountainSample; ) {
			if (terr[k] != "terrain_mountain") { k++; continue; }
			int first = k;
			while (k + 1 <= lastMountainSample && terr[k + 1] == "terrain_mountain") k++;
			int last = k;
			k++;

			float maxSlope = 0.f;
			int from = max(0, first - 1), to = min(count, last + 1);
			for (int i = from; i < to; i++) {
				maxSlope = max(maxSlope, fabsf(heights[i + 1] - heights[i]) / max(1e-3f, arc[i + 1] - arc[i]));
			}
			bool seaBehind = last + 1 <= count && terr[last + 1] == "terrain_ocean";
			bool raise = seaBehind || maxSlope <= MOUNTAIN_MAX_SLOPE;
			if (!raise) {
				TunnelSpan span{ first - 1 - rampSamples, first - 1, last + 1, last + 1 + rampSamples };
				bool roomBefore = span.p >= lastQ && allGood(span.p, span.tIn);
				bool roomAfter = span.q <= lastMountainSample && allGood(span.tOut, span.q);
				if (roomBefore && roomAfter) {
					tunnels.push_back(span);
					lastQ = span.q;
				}
				else {
					raise = true;
				}
			}
			if (raise) {
				for (int i = first; i <= last; i++) raised[i] = true;
			}
			debugf("Log: TrainRoute link %d mountain samples [%d,%d] max slope %.3f sea behind %d -> %s.\n",
				static_cast<int>(linkIndex), first, last, maxSlope, seaBehind ? 1 : 0, raise ? "raise" : "tunnel");
		}

		// ---- 每个采样点的高度、坡度(Z每水平单位的变化量)和归类。基准高度在两端站点z之间线性过渡。 ----
		vector<float> z(count + 1), slope(count + 1, 0.f);
		vector<char> inTunnelLevel(count + 1, 0), inRamp(count + 1, 0);
		for (int i = 0; i <= count; i++) {
			float base = a.z + (b.z - a.z) * (arc[i] / totalLength);
			z[i] = raised[i] ? max(heights[i], base) : base;
		}
		for (const TunnelSpan& t : tunnels) {
			for (int i = t.p; i <= t.q; i++) {
				float base = a.z + (b.z - a.z) * (arc[i] / totalLength);
				float depth;
				float dDepth = 0.f; // 深度对弧长的导数
				if (i <= t.tIn) {
					float u = static_cast<float>(i - t.p) / static_cast<float>(t.tIn - t.p);
					depth = SmoothStep(u);
					dDepth = 6.f * u * (1.f - u) / (arc[t.tIn] - arc[t.p]);
				}
				else if (i < t.tOut) {
					depth = 1.f;
				}
				else {
					float u = static_cast<float>(i - t.tOut) / static_cast<float>(t.q - t.tOut);
					depth = 1.f - SmoothStep(u);
					dDepth = -6.f * u * (1.f - u) / (arc[t.q] - arc[t.tOut]);
				}
				z[i] = base + TUNNEL_HEIGHT * depth;
				slope[i] = TUNNEL_HEIGHT * dDepth;
				if (i >= t.tIn && i <= t.tOut) inTunnelLevel[i] = 1;
				else inRamp[i] = 1;
			}
		}
		// 抬升段的坡度取前后相邻点连线的坡度(Catmull-Rom)；山脚/平地/两端取0，不带着惯性冲到基准以下。
		for (int i = 1; i < count; i++) {
			if (!raised[i]) continue;
			float run = arc[i + 1] - arc[i - 1];
			slope[i] = run > 1e-6f ? (z[i + 1] - z[i - 1]) / run : 0.f;
		}

		// ---- 第二遍：每相邻两点一段三次贝塞尔(平面上是弦，Z用端点坡度做Hermite)，坡度夹到单调范围
		// (与这段高度差同号、不超过3倍平均坡度)，三次曲线不会冲出两端高度。完全水平且共线的连续
		// 采样点合并成一段，不然火车几百单位的线路会有几百个无谓的小段。 ----
		auto segmentTag = [&](int i) -> int { // 相邻两点(i,i+1)这一段的归类，合并只能在同类里进行
			if (inTunnelLevel[i] && inTunnelLevel[i + 1]) return 3;
			if (raised[i] || raised[i + 1]) return 1;
			if (inRamp[i] || inRamp[i + 1] || inTunnelLevel[i] || inTunnelLevel[i + 1]) return 2;
			return 0;
		};
		auto isFlat = [&](int i) {
			return fabsf(z[i] - z[i + 1]) < 1e-4f && fabsf(slope[i]) < 1e-6f && fabsf(slope[i + 1]) < 1e-6f;
		};

		TrackEdgeGeometry& geometry = trackEdges[linkIndex];
		int i = 0;
		while (i < count) {
			int j = i + 1;
			if (isFlat(i)) {
				int tag = segmentTag(i);
				while (j < count && isFlat(j) && segmentTag(j) == tag && fabsf(z[j + 1] - z[i]) < 1e-4f) {
					// 候选终点j+1：中间点都要贴着弦(偏差<0.01单位)
					float dx = sx[j + 1] - sx[i], dy = sy[j + 1] - sy[i];
					float len = sqrtf(dx * dx + dy * dy);
					bool straight = len > 1e-6f;
					for (int m = i + 1; m <= j && straight; m++) {
						float dev = fabsf((sx[m] - sx[i]) * dy - (sy[m] - sy[i]) * dx) / len;
						if (dev > 0.01f) straight = false;
					}
					if (!straight) break;
					j++;
				}
			}

			float dx = sx[j] - sx[i], dy = sy[j] - sy[i];
			float run = sqrtf(dx * dx + dy * dy);
			float delta = run > 1e-6f ? (z[j] - z[i]) / run : 0.f;
			auto monotone = [delta](float s) {
				if (delta == 0.f) return 0.f;
				float aligned = s * (delta > 0.f ? 1.f : -1.f);
				aligned = max(0.f, min(aligned, 3.f * fabsf(delta)));
				return aligned * (delta > 0.f ? 1.f : -1.f);
			};
			float startSlope = monotone(slope[i]), endSlope = monotone(slope[j]);

			TrackSegment seg;
			seg.x1 = sx[i]; seg.y1 = sy[i]; seg.z1 = z[i];
			seg.c1x = sx[i] + dx / 3.f; seg.c1y = sy[i] + dy / 3.f; seg.c1z = z[i] + startSlope * run / 3.f;
			seg.c2x = sx[i] + dx * 2.f / 3.f; seg.c2y = sy[i] + dy * 2.f / 3.f; seg.c2z = z[j] - endSlope * run / 3.f;
			seg.x2 = sx[j]; seg.y2 = sy[j]; seg.z2 = z[j];
			seg.tunnel = segmentTag(i) == 3;
			geometry.segments.push_back(seg);

			// 请求：抬升段贴地；隧道口(坡道，tag 2)挖带侧壁的洞。水平隧道段在山体里面不挖。
			int tag = segmentTag(i);
			if (tag == 1 || tag == 2) {
				Connection chordConnection(Node("route", seg.x1, seg.y1, seg.z1), Node("route", seg.x2, seg.y2, seg.z2));
				chordConnection.AddControls({ { Node("route_ctrl", seg.c1x, seg.c1y, seg.c1z), 1.f }, { Node("route_ctrl", seg.c2x, seg.c2y, seg.c2z), 1.f } });
				if (tag == 1) AddTerrainFit(&chordConnection, 0.f, 1.f, trackWidth, TERRAIN_FIT_FALLOFF);
				else AddTunnelHatch(&chordConnection, 0.f, 1.f, trackWidth);
			}
			i = j;
		}
	}
}
