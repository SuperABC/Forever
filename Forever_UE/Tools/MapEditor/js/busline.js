// 路网路线(公交)：公交站点放在路网某条路的某一侧，一串站首尾相接成一条环线。纯逻辑，无DOM。
//
// 模型(挂在 model.transit.roadLines，id与路网共用 model.nextId)：
//   roadLines: [{id, route:'route_bus', stops:[{id, road:路id, t:路上Bezier参数, side:0|1, station:'station_bus'}]}]
//   side0 = 沿 a→b 的右手边(横向偏移方向(-fy,fx)，行车方向 a→b，对应 lanes[0..2])；side1 = 另一侧(b→a，lanes[3..5])。
//   环首尾隐含相接(最后一站→第一站)，不存重复项。
// 导出(output.transit.roadLines)见 Source/Basic/map/roadnet_file.md：
//   [{route, stops:[{road:output.roads下标, x, y(路中线上的垂足), side, station}]}]
// 行车图：边 a→b 需 lanes[0]>0、b→a 需 lanes[3]>0(单行道)；城外连接点(extern)不可穿行；节点处允许掉头(游戏路口连接器允许同路U形)。
// 预览路线只是参考，游戏里的实际寻路由 Core 做。
(function (root) {
	const ME = root.ME = root.ME || {};

	const BUS_ROUTE_ID = 'route_bus';
	const BUS_STATION_IDS = ['station_bus'];
	const SNAP_PAD = 0.3;      // 吸附半径：半路宽之外再加这么多(世界单位)，调用方可传更大的
	const MIN_STOP_GAP = 1e-3; // 同一路侧上两站的t至少差这么多

	const lines = (model) => { const t = ME.ensureTransit(model); if (!t.roadLines) t.roadLines = []; return t.roadLines; };
	const lineOf = (model, id) => lines(model).find((l) => l.id === id);
	function stopOf(model, id) {
		for (const line of lines(model)) {
			const index = line.stops.findIndex((s) => s.id === id);
			if (index >= 0) return { line, stop: line.stops[index], index };
		}
		return null;
	}

	const sideHasLane = (road, side) => (side === 0 ? road.lanes[0] : road.lanes[3]) > 0;
	// 路中线到该侧车行道中线的有符号偏移(沿(-fy,fx))：side0 在分界线向外，side1 在分界线向另一边。
	function laneOffset(road, side) {
		const LW = ME.LANE_WIDTH, ln = road.lanes;
		const w0 = (ln[0] + ln[1] + ln[2]) * LW, total = w0 + (ln[3] + ln[4] + ln[5]) * LW;
		const divider = total / 2 - w0;
		return side === 0 ? divider + ln[0] * LW / 2 : divider - ln[3] * LW / 2;
	}

	// 路上t处该侧车道中线上的点 {x,y,fx,fy}(fx,fy=行车方向的单位向量)。路没了返回null。
	function stopPoint(model, stop) {
		const road = ME.roadOf(model, stop.road);
		if (!road) return null;
		const q = ME.quadOf(model, road);
		const p = ME.evalQuad(q, stop.t), f = ME.tangentAt(q, stop.t) || [1, 0];
		const off = laneOffset(road, stop.side);
		const dir = stop.side === 0 ? f : [-f[0], -f[1]];
		return { x: p[0] - f[1] * off, y: p[1] + f[0] * off, fx: dir[0], fy: dir[1], cx: p[0], cy: p[1] };
	}

	// 鼠标(x,y)吸附到路网某条路的某一侧：不吸附节点/路口，只取垂足。返回 {road, t, side, x, y, ok, reason}，没有路在范围内返回null。
	// 单行道自动取有车行道的一侧；ok=false 只在这条路两侧都没有车行道时出现。maxDist=比半路宽多出的容差。
	function busSnap(model, x, y, maxDist) {
		const pad = maxDist === undefined ? SNAP_PAD : maxDist;
		let best = null;
		for (const road of model.roads) {
			const hw = ME.roadWidth(road) / 2;
			const hit = ME.nearestOnRoad(model, road, x, y);
			if (hit.dist > hw + pad) continue;
			if (!best || hit.dist < best.hit.dist) best = { road, hit };
		}
		if (!best) return null;
		const q = ME.quadOf(model, best.road), f = ME.tangentAt(q, best.hit.t) || [1, 0];
		let side = ((x - best.hit.x) * -f[1] + (y - best.hit.y) * f[0]) >= 0 ? 0 : 1;
		// 单行道只有一侧有车行道：点哪边都吸到有车行道的那一侧(车行道是居中的，两边本来就分不清)
		if (!sideHasLane(best.road, side) && sideHasLane(best.road, 1 - side)) side = 1 - side;
		const ok = sideHasLane(best.road, side);
		return { road: best.road.id, t: best.hit.t, side, x: best.hit.x, y: best.hit.y, ok, reason: ok ? '' : '这条路没有车行道，不能放公交站' };
	}

	// ---------------- 行车图与路径 ----------------
	function driveEdges(model) {
		const out = new Map(); // nodeId -> [{to, road, forward, len}]
		const add = (from, e) => { if (!out.has(from)) out.set(from, []); out.get(from).push(e); };
		for (const r of model.roads) {
			const a = ME.nodeOf(model, r.a), b = ME.nodeOf(model, r.b);
			if (!a || !b) continue;
			const len = ME.roadLength(model, r);
			if (r.lanes[0] > 0 && !a.extern) add(r.a, { to: r.b, road: r, forward: true, len });
			if (r.lanes[3] > 0 && !b.extern) add(r.b, { to: r.a, road: r, forward: false, len });
		}
		return out;
	}

	// 路上从ta到tb(ta>tb时反向)沿该侧车道中线的采样折线 [[x,y]...]。
	function roadStretch(model, road, side, ta, tb) {
		const q = ME.quadOf(model, road), off = laneOffset(road, side);
		const lo = Math.min(ta, tb), hi = Math.max(ta, tb);
		const ts = [lo];
		for (const p of ME.polyline(q)) if (p.t > lo + 1e-9 && p.t < hi - 1e-9) ts.push(p.t);
		if (hi > lo + 1e-9) ts.push(hi);
		const pts = ts.map((t) => {
			const p = ME.evalQuad(q, t), f = ME.tangentAt(q, t) || [1, 0];
			return [p[0] - f[1] * off, p[1] + f[0] * off];
		});
		if (ta > tb) pts.reverse();
		return pts;
	}
	function polyLen(pts) { let s = 0; for (let i = 1; i < pts.length; i++) s += Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]); return s; }

	// 从站A(沿其侧的行车方向)开到站B的行车路线。返回 {pts:[[x,y]...], length} ，开不到返回null。A/B=站或吸附结果({road,t,side})。
	// graph可选(driveEdges的结果，同一帧里复用)。
	function busPath(model, A, B, graph) {
		const roadA = ME.roadOf(model, A.road), roadB = ME.roadOf(model, B.road);
		if (!roadA || !roadB || !sideHasLane(roadA, A.side) || !sideHasLane(roadB, B.side)) return null;
		const fwdA = A.side === 0, fwdB = B.side === 0;
		// 同一路同一侧、B在A的行车方向下游：直接开过去
		if (A.road === B.road && A.side === B.side && (fwdA ? B.t > A.t + MIN_STOP_GAP : B.t < A.t - MIN_STOP_GAP)) {
			const pts = roadStretch(model, roadA, A.side, A.t, B.t);
			return { pts, length: polyLen(pts) };
		}
		const startNode = fwdA ? roadA.b : roadA.a;
		const entryNode = fwdB ? roadB.a : roadB.b;
		const nA = ME.nodeOf(model, startNode), nB = ME.nodeOf(model, entryNode);
		if (!nA || !nB || nA.extern || nB.extern) return null;
		const g = graph || driveEdges(model);
		const dist = new Map([[startNode, 0]]), prev = new Map(), done = new Set();
		for (;;) {
			let cur = null, best = Infinity;
			for (const [id, d] of dist) if (!done.has(id) && d < best) { best = d; cur = id; }
			if (cur === null) return null;
			if (cur === entryNode) break;
			done.add(cur);
			for (const e of g.get(cur) || []) {
				const nd = best + e.len;
				if (nd < (dist.has(e.to) ? dist.get(e.to) : Infinity)) { dist.set(e.to, nd); prev.set(e.to, { from: cur, edge: e }); }
			}
		}
		const hops = [];
		for (let cur = entryNode; cur !== startNode;) { const p = prev.get(cur); hops.push(p.edge); cur = p.from; }
		hops.reverse();
		let pts = roadStretch(model, roadA, A.side, A.t, fwdA ? 1 : 0);
		const append = (more) => { for (let i = pts.length ? 1 : 0; i < more.length; i++) pts.push(more[i]); };
		for (const e of hops) append(roadStretch(model, e.road, e.forward ? 0 : 1, e.forward ? 0 : 1, e.forward ? 1 : 0));
		append(roadStretch(model, roadB, B.side, fwdB ? 0 : 1, B.t));
		return { pts, length: polyLen(pts) };
	}

	// 一条线每一段(站i→站i+1，最后一站→第一站)的路线，{pts,length}|null。
	function linePaths(model, line) {
		const g = driveEdges(model), n = line.stops.length;
		return line.stops.map((s, i) => busPath(model, s, line.stops[(i + 1) % n], g));
	}

	// ---------------- 增删改 ----------------
	// 新建一条线：stops=[{road,t,side,station?}]，至少2站，每一段都要开得通(含最后一站→第一站)。返回 {line}|{error}。
	function addBusLine(model, stops) {
		if (!stops || stops.length < 2) return { error: '公交线至少要2个站' };
		const g = driveEdges(model);
		const list = stops.map((s) => ({ id: 0, road: s.road, t: s.t, side: s.side, station: s.station || BUS_STATION_IDS[0] }));
		for (let i = 0; i < list.length; i++) {
			const a = list[i], b = list[(i + 1) % list.length];
			if (!busPath(model, a, b, g)) return { error: `第${i + 1}站到第${(i + 1) % list.length + 1}站开不通` };
		}
		for (const s of list) s.id = model.nextId++;
		const line = { id: model.nextId++, route: BUS_ROUTE_ID, stops: list };
		lines(model).push(line);
		return { line };
	}
	function setBusStopStation(model, stopId, station) {
		const hit = stopOf(model, stopId);
		if (!hit) return false;
		hit.stop.station = station;
		return true;
	}
	function deleteBusLine(model, lineId) {
		const t = ME.ensureTransit(model);
		t.roadLines = lines(model).filter((l) => l.id !== lineId);
	}
	// 删一个站；线剩不到2站就整条删。
	function deleteBusStop(model, stopId) {
		const hit = stopOf(model, stopId);
		if (!hit) return;
		hit.line.stops.splice(hit.index, 1);
		if (hit.line.stops.length < 2) deleteBusLine(model, hit.line.id);
	}
	// 路没了的站删掉，线不足2站的整条删。
	function cleanupBusLines(model) {
		const t = model.transit;
		if (!t || !t.roadLines) return;
		for (const l of t.roadLines) l.stops = l.stops.filter((s) => ME.roadOf(model, s.road));
		t.roadLines = t.roadLines.filter((l) => l.stops.length >= 2);
	}
	// 路road在t处被切开：second是切出的后半段(新id)，落在后半段的站改挂second并换算参数(de Casteljau是线性重参数化)。
	function remapBusStops(model, roadId, t, secondId) {
		for (const l of lines(model)) {
			for (const s of l.stops) {
				if (s.road !== roadId) continue;
				if (s.t > t) { s.road = secondId; s.t = Math.min(1, (s.t - t) / (1 - t)); }
				else s.t = t > 1e-9 ? s.t / t : 0;
			}
		}
	}

	// ---------------- 导出 ----------------
	// pieceRanges: Map(roadId -> [{t0,t1}])，和roadPieces一一对应(绿化同款)。
	function exportBusLines(model, output, roadPieces, pieceRanges, warnings) {
		const out = [];
		for (const line of lines(model)) {
			const stops = [];
			for (const s of line.stops) {
				const road = ME.roadOf(model, s.road);
				if (!road) { warnings.push(`公交线${line.id}的一个站所在的路已不存在，该站已跳过`); continue; }
				if (!sideHasLane(road, s.side)) { warnings.push(`公交线${line.id}的站${s.id}所在一侧没有车行道，该站已跳过`); continue; }
				const pieces = roadPieces ? roadPieces.get(road.id) : null;
				if (!pieces || !pieces.length) { warnings.push(`公交线${line.id}的站${s.id}所在的路没有导出，该站已跳过`); continue; }
				const k = ME.pieceIndexAt(pieceRanges ? pieceRanges.get(road.id) : null, s.t);
				const p = ME.evalQuad(ME.quadOf(model, road), s.t);
				stops.push({ road: pieces[Math.min(k, pieces.length - 1)], x: p[0], y: p[1], side: s.side, station: s.station });
			}
			if (stops.length < 2) { warnings.push(`公交线${line.id}有效站不足2个，已跳过`); continue; }
			out.push({ route: line.route || BUS_ROUTE_ID, stops });
		}
		if (!out.length) return;
		if (!output.transit) output.transit = { nodes: [], conns: [], loops: [] };
		output.transit.roadLines = out;
	}

	ME.BUS_ROUTE_ID = BUS_ROUTE_ID;
	ME.BUS_STATION_IDS = BUS_STATION_IDS;
	ME.busLines = lines;
	ME.busLineOf = lineOf;
	ME.busStopOf = stopOf;
	ME.busSideHasLane = sideHasLane;
	ME.busLaneOffset = laneOffset;
	ME.busStopPoint = stopPoint;
	ME.busSnap = busSnap;
	ME.busDriveGraph = driveEdges;
	ME.busPath = busPath;
	ME.busLinePaths = linePaths;
	ME.addBusLine = addBusLine;
	ME.setBusStopStation = setBusStopStation;
	ME.deleteBusLine = deleteBusLine;
	ME.deleteBusStop = deleteBusStop;
	ME.cleanupBusLines = cleanupBusLines;
	ME.remapBusStops = remapBusStops;
	ME.exportBusLines = exportBusLines;
	if (typeof module !== 'undefined') module.exports = ME;
})(typeof window !== 'undefined' ? window : globalThis);
