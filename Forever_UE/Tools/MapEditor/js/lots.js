// 地块(lot)：由两个节点A、B和一个带符号的深度决定一个矩形(A→B为一条边，深度沿其法线方向伸出)。
// 模型：{id, a, b, depth, area, margin:[西,东,北,南]各项null=自动}。导出格式见 Source/Basic/map/roadnet_file.md。
// 方向标签必须和Core(Lot::SetPosition 4节点版)一致：先把4个角按相对中心的atan2(y,x)降序排序得到n0..n3，
//   西=边(n3,n0)  东=边(n1,n2)  北=边(n2,n3)  南=边(n0,n1)；margin/boundary都按这个标签(0西1东2北3南)。
(function (root) {
	const ME = root.ME = root.ME || {};

	const AREAS = [
		'AREA_RESIDENCE_HIGH', 'AREA_RESIDENCE_MIDDLE', 'AREA_RESIDENCE_LOW',
		'AREA_COMMERCE_HIGH', 'AREA_COMMERCE_MIDDLE', 'AREA_COMMERCE_LOW',
		'AREA_INDUSTRY_HIGH', 'AREA_INDUSTRY_MIDDLE', 'AREA_INDUSTRY_LOW',
		'AREA_OFFICE_HIGH', 'AREA_OFFICE_MIDDLE', 'AREA_OFFICE_LOW',
		'AREA_TEST', 'AREA_PLACEHOLDER',
	];
	const DIRECTION_NAMES = ['西', '东', '北', '南'];
	const END_EPS = 0.06;

	// 矩形4个角(A, B, B+n*d, A+n*d)；depth带符号。
	function lotCorners(model, lot) {
		const a = ME.nodeOf(model, lot.a), b = ME.nodeOf(model, lot.b);
		if (!a || !b) return null;
		const dx = b.x - a.x, dy = b.y - a.y;
		const len = Math.hypot(dx, dy);
		if (len < 1e-6 || Math.abs(lot.depth) < 1e-6) return null;
		const nx = -dy / len * lot.depth, ny = dx / len * lot.depth;
		return [[a.x, a.y], [b.x, b.y], [b.x + nx, b.y + ny], [a.x + nx, a.y + ny]];
	}

	// 按Core的顺序排序，返回 {nodes:[n0..n3], edges:{0:[p,q],1:..,2:..,3:..}}。
	function orderCorners(corners) {
		let cx = 0, cy = 0;
		for (const c of corners) { cx += c[0]; cy += c[1]; }
		cx /= 4; cy /= 4;
		const nodes = corners.slice().sort((p, q) => Math.atan2(q[1] - cy, q[0] - cx) - Math.atan2(p[1] - cy, p[0] - cx));
		return { nodes, edges: { 0: [nodes[3], nodes[0]], 1: [nodes[1], nodes[2]], 2: [nodes[2], nodes[3]], 3: [nodes[0], nodes[1]] } };
	}

	// 找刚好贴在这条边上的用户road(两个端点各落在边的两个端点上)，且必须是展开后只有1段的直线地面路(Core的边界Road是单条Road)。
	function boundaryRoadFor(model, edge, roadPieces) {
		for (const r of model.roads) {
			if (r.type !== 'ground' || r.ctrl) continue;
			const pieces = roadPieces.get(r.id);
			if (!pieces || pieces.length !== 1) continue;
			const a = ME.nodeOf(model, r.a), b = ME.nodeOf(model, r.b);
			const d = (p, q) => Math.hypot(p[0] - q.x, p[1] - q.y);
			if ((d(edge[0], a) < END_EPS && d(edge[1], b) < END_EPS) || (d(edge[0], b) < END_EPS && d(edge[1], a) < END_EPS)) return r;
		}
		return null;
	}

	function inRect(x, y, nodes) {
		// 凸四边形，叉积同号。
		let sign = 0;
		for (let i = 0; i < 4; i++) {
			const p = nodes[i], q = nodes[(i + 1) % 4];
			const cross = (q[0] - p[0]) * (y - p[1]) - (q[1] - p[1]) * (x - p[0]);
			if (Math.abs(cross) < 1e-9) continue;
			const s = cross > 0 ? 1 : -1;
			if (sign === 0) sign = s; else if (s !== sign) return false;
		}
		return true;
	}

	// 地块覆盖的element里不可建造的个数(3x3邻域高度不全为0，和游戏的construction标记一致)。
	function nonBuildableCount(terrain, corners) {
		if (!terrain) return 0;
		const xs = corners.map((c) => c[0]), ys = corners.map((c) => c[1]);
		const x0 = Math.floor(Math.min(...xs)), x1 = Math.ceil(Math.max(...xs));
		const y0 = Math.floor(Math.min(...ys)), y1 = Math.ceil(Math.max(...ys));
		const nodes = orderCorners(corners).nodes;
		let bad = 0;
		for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++) {
			if (!inRect(x + 0.5, y + 0.5, nodes)) continue;
			if (terrain.countNonBuildable(x, y, x + 1, y + 1) > 0) bad++;
		}
		return bad;
	}

	function addLot(model, a, b, depth, area) {
		const lot = { id: model.nextId++, a, b, depth, area: area || 'AREA_RESIDENCE_HIGH', margin: [null, null, null, null], presets: [] };
		model.lots.push(lot);
		return lot;
	}

	// 自动margin和边界信息(UI显示用)。返回 {directions:[{name, road|null, margin}], corners}
	function describeLot(model, lot, roadPieces) {
		const corners = lotCorners(model, lot);
		if (!corners) return null;
		const ordered = orderCorners(corners);
		const directions = [];
		for (let d = 0; d < 4; d++) {
			const road = roadPieces ? boundaryRoadFor(model, ordered.edges[d], roadPieces) : null;
			const auto = road ? ME.roadWidth(road) / 2 : 0;
			const custom = lot.margin && lot.margin[d] !== null && lot.margin[d] !== undefined ? lot.margin[d] : null;
			directions.push({ name: DIRECTION_NAMES[d], road, auto, margin: custom !== null ? custom : auto, custom });
		}
		return { corners, ordered, directions };
	}

	// 扣掉四向缩进后的内矩形4个角(按n0..n3的顺序)。margin=[西,东,北,南]，都是数。缩进吃光时返回null。
	function insetCorners(corners, margin) {
		const [n0, n1, n2] = orderCorners(corners).nodes;
		const l1 = Math.hypot(n1[0] - n0[0], n1[1] - n0[1]), l2 = Math.hypot(n2[0] - n1[0], n2[1] - n1[1]);
		if (l1 < 1e-9 || l2 < 1e-9) return null;
		const [mw, me, mn, ms] = margin;
		if (l1 - mw - me <= 0.01 || l2 - mn - ms <= 0.01) return null;
		const ux = (n1[0] - n0[0]) / l1, uy = (n1[1] - n0[1]) / l1, vx = (n2[0] - n1[0]) / l2, vy = (n2[1] - n1[1]) / l2;
		const at = (b, su, sv) => [b[0] + ux * su + vx * sv, b[1] + uy * su + vy * sv];
		const nodes = orderCorners(corners).nodes;
		return [at(nodes[0], mw, ms), at(nodes[1], -me, ms), at(nodes[2], -me, -mn), at(nodes[3], mw, -mn)];
	}


	// ===================== 园区/建筑预设(preset) =====================
	// 预设=手动指定的Assign结果：{id, modId, dir, x0,x1,y0,y1}。矩形在lot"本地坐标系"里：缩进后的内矩形，原点=西北角、x向东、y向南
	// (和Core Lot::GetPosition约定一致)，dir=矩形贴靠的那条有路的边(0西1东2北3南，指矩形所在子地块的那一侧)。
	// 下面的pool模拟器逐行对应 Source/Dependence/map/general_geometry.cpp 的 Lot::RequestPlacement/SplitWithPath/SplitPlain，
	// 数值常量也必须一致：小路宽 PATH_WIDTH(=(0.3+0.2)*2=1)、MIN_LOT_EXTENT=2、minGap=小路宽+MIN_LOT_EXTENT=3。
	const PATH_HALF = 0.5, MIN_LOT_EXTENT = 2, MIN_GAP = 2 * PATH_HALF + MIN_LOT_EXTENT;
	const RECT_EPS = 1e-2;
	// 子块 {x0,y0,x1,y1, road:[西,东,北,南]各bool}，都在顶层lot本地坐标系里。
	const hasAnyRoad = (r) => r.road[0] || r.road[1] || r.road[2] || r.road[3];
	const rw = (r) => r.x1 - r.x0, rh = (r) => r.y1 - r.y0;

	// 内矩形的本地坐标系：origin=西北角(世界坐标)，ex=向东单位向量，ey=向南单位向量，W/H=内矩形尺寸，road=四边是否有路。
	function lotFrame(model, lot, roadPieces) {
		const info = describeLot(model, lot, roadPieces);
		if (!info) return null;
		const margin = info.directions.map((d) => d.margin);
		const [n0, n1, n2, n3] = info.ordered.nodes;
		const l1 = Math.hypot(n2[0] - n3[0], n2[1] - n3[1]), l2 = Math.hypot(n0[0] - n3[0], n0[1] - n3[1]);
		const W = l1 - margin[0] - margin[1], H = l2 - margin[2] - margin[3];
		if (W <= 0.05 || H <= 0.05) return null;
		const ex = [(n2[0] - n3[0]) / l1, (n2[1] - n3[1]) / l1], ey = [(n0[0] - n3[0]) / l2, (n0[1] - n3[1]) / l2];
		const origin = [n3[0] + ex[0] * margin[0] + ey[0] * margin[2], n3[1] + ex[1] * margin[0] + ey[1] * margin[2]];
		return { origin, ex, ey, W, H, road: info.directions.map((d) => !!d.road) };
	}
	const localToWorld = (f, x, y) => [f.origin[0] + f.ex[0] * x + f.ey[0] * y, f.origin[1] + f.ex[1] * x + f.ey[1] * y];
	const worldToLocal = (f, wx, wy) => {
		const dx = wx - f.origin[0], dy = wy - f.origin[1];
		return [dx * f.ex[0] + dy * f.ex[1], dx * f.ey[0] + dy * f.ey[1]];
	};

	// 沿一条线切一刀(带小路)。alongX=true：切割线垂直于X轴(两端落在北/南边)，coord是相对r.x0的位置；否则垂直于Y轴(两端落在西/东边)。
	// 返回 {lower, upper} 或 null(Core里SplitWithPath返回空pathRoad)。
	function splitWithPath(r, alongX, coord) {
		const extent = alongX ? rw(r) : rh(r);
		if (coord - PATH_HALF < 0 || coord + PATH_HALF > extent) return null;
		const endA = alongX ? 2 : 0, endB = alongX ? 3 : 1;
		if (!r.road[endA] && !r.road[endB]) return null;   // 两端都没有已有路可连，拒绝产生孤岛小路
		const lowKeep = alongX ? 0 : 2, upKeep = alongX ? 1 : 3, lowPath = alongX ? 1 : 3, upPath = alongX ? 0 : 2;
		let lower, upper;
		if (alongX) {
			lower = { x0: r.x0, y0: r.y0, x1: r.x0 + coord - PATH_HALF, y1: r.y1, road: [false, false, false, false] };
			upper = { x0: r.x0 + coord + PATH_HALF, y0: r.y0, x1: r.x1, y1: r.y1, road: [false, false, false, false] };
		} else {
			lower = { x0: r.x0, y0: r.y0, x1: r.x1, y1: r.y0 + coord - PATH_HALF, road: [false, false, false, false] };
			upper = { x0: r.x0, y0: r.y0 + coord + PATH_HALF, x1: r.x1, y1: r.y1, road: [false, false, false, false] };
		}
		if (rw(lower) < MIN_LOT_EXTENT || rh(lower) < MIN_LOT_EXTENT || rw(upper) < MIN_LOT_EXTENT || rh(upper) < MIN_LOT_EXTENT) return null;
		for (const e of [endA, endB]) { lower.road[e] = r.road[e]; upper.road[e] = r.road[e]; }
		lower.road[lowKeep] = r.road[lowKeep]; upper.road[upKeep] = r.road[upKeep];
		lower.road[lowPath] = true; upper.road[upPath] = true;
		return { lower, upper };
	}
	// 不带小路的普通一分为二，切割面上没有路。
	function splitPlain(r, alongX, coord) {
		const extent = alongX ? rw(r) : rh(r);
		if (coord < MIN_LOT_EXTENT || extent - coord < MIN_LOT_EXTENT) return null;
		const endA = alongX ? 2 : 0, endB = alongX ? 3 : 1;
		const lowKeep = alongX ? 0 : 2, upKeep = alongX ? 1 : 3;
		let lower, upper;
		if (alongX) {
			lower = { x0: r.x0, y0: r.y0, x1: r.x0 + coord, y1: r.y1, road: [false, false, false, false] };
			upper = { x0: r.x0 + coord, y0: r.y0, x1: r.x1, y1: r.y1, road: [false, false, false, false] };
		} else {
			lower = { x0: r.x0, y0: r.y0, x1: r.x1, y1: r.y0 + coord, road: [false, false, false, false] };
			upper = { x0: r.x0, y0: r.y0 + coord, x1: r.x1, y1: r.y1, road: [false, false, false, false] };
		}
		for (const e of [endA, endB]) { lower.road[e] = r.road[e]; upper.road[e] = r.road[e]; }
		lower.road[lowKeep] = r.road[lowKeep]; upper.road[upKeep] = r.road[upKeep];
		return { lower, upper };
	}

	// 对一块自由地(candidate)执行 RequestPlacement：rect=目标矩形(本地坐标，须完整落在candidate内)，dir=贴靠方向。
	// 成功返回 {placed, survivors}，失败返回 {error}。
	function requestPlacement(candidate, rect, dir) {
		if (!candidate.road[dir]) return { error: '这一侧没有路' };
		if (rect.x1 - rect.x0 < MIN_LOT_EXTENT - 1e-9 || rect.y1 - rect.y0 < MIN_LOT_EXTENT - 1e-9) return { error: `预设矩形太小(每边至少${MIN_LOT_EXTENT})` };
		let working = candidate;
		const survivors = [];
		const reproject = () => ({ xmin: rect.x0 - working.x0, xmax: rect.x1 - working.x0, ymin: rect.y0 - working.y0, ymax: rect.y1 - working.y0 });
		const keepSurvivor = (s) => { if (rw(s) >= MIN_LOT_EXTENT && rh(s) >= MIN_LOT_EXTENT && hasAnyRoad(s)) survivors.push(s); };
		const cutKeepLower = (alongX, coord) => { const res = splitWithPath(working, alongX, coord); if (!res) return false; keepSurvivor(res.upper); working = res.lower; return true; };
		const cutKeepUpper = (alongX, coord) => { const res = splitWithPath(working, alongX, coord); if (!res) return false; keepSurvivor(res.lower); working = res.upper; return true; };
		const plainSplit = (alongX, coord, keepLower, discardRest) => {
			const res = splitPlain(working, alongX, coord); if (!res) return false;
			const keep = keepLower ? res.lower : res.upper, rest = keepLower ? res.upper : res.lower;
			if (!discardRest) keepSurvivor(rest);
			working = keep; return true;
		};
		const depthAlongX = dir === 0 || dir === 1, touchesLowEnd = dir === 0 || dir === 2;
		let ok = true;
		{
			const p = reproject(), frontMin = depthAlongX ? p.ymin : p.xmin;
			if (frontMin >= MIN_GAP) ok = cutKeepUpper(!depthAlongX, frontMin - PATH_HALF);
		}
		if (ok) {
			const p = reproject(), frontMax = depthAlongX ? p.ymax : p.xmax, frontExtent = depthAlongX ? rh(working) : rw(working);
			if (frontExtent - frontMax >= MIN_GAP) ok = cutKeepLower(!depthAlongX, frontMax + PATH_HALF);
		}
		if (ok) {
			const p = reproject();
			const depthMin = depthAlongX ? p.xmin : p.ymin, depthMax = depthAlongX ? p.xmax : p.ymax, depthExtent = depthAlongX ? rw(working) : rh(working);
			const farRoad = working.road[dir ^ 1];
			if (touchesLowEnd) {
				if (depthExtent - depthMax >= MIN_GAP) {
					if (farRoad) plainSplit(depthAlongX, depthMax, true, false);
					else if (!cutKeepLower(depthAlongX, depthMax + PATH_HALF)) plainSplit(depthAlongX, depthMax, true, true);
				}
			} else if (depthMin >= MIN_GAP) {
				if (farRoad) plainSplit(depthAlongX, depthMin, false, false);
				else if (!cutKeepUpper(depthAlongX, depthMin - PATH_HALF)) plainSplit(depthAlongX, depthMin, false, true);
			}
		}
		if (!ok) return { error: '切分失败(小路两端都没有路可连，或剩余空间太窄)' };
		return { placed: working, survivors };
	}

	// 在pool里放一个预设矩形(同Map::PlacePresets)：找完整包含它的自由地(跨小路就找不到)，放成功返回 {pool, placed, parent}。
	function placePresetRect(pool, rect, dir) {
		const idx = pool.findIndex((c) => rect.x0 >= c.x0 - RECT_EPS && rect.x1 <= c.x1 + RECT_EPS && rect.y0 >= c.y0 - RECT_EPS && rect.y1 <= c.y1 + RECT_EPS);
		if (idx < 0) return { error: '预设矩形不在同一块自由地内(跨了小路，或和已有预设重叠)' };
		const res = requestPlacement(pool[idx], rect, dir);
		if (res.error) return res;
		const next = pool.slice(); next.splice(idx, 1);
		for (const s of res.survivors) next.push(s);
		return { pool: next, placed: res.placed, parent: pool[idx] };
	}

	// 按顺序重放一个lot的全部预设。返回 {frame, pool(最终), steps:[{placed|null, error|null}]}；某个预设失败就跳过它(pool不变)。
	function replayPresets(model, lot, roadPieces) {
		const frame = lotFrame(model, lot, roadPieces);
		if (!frame) return null;
		let pool = [{ x0: 0, y0: 0, x1: frame.W, y1: frame.H, road: frame.road.slice() }];
		const steps = [];
		for (const p of lot.presets || []) {
			const res = placePresetRect(pool, p, p.dir);
			if (res.error) { steps.push({ placed: null, error: res.error }); continue; }
			pool = res.pool;
			steps.push({ placed: res.placed, error: null });
		}
		return { frame, pool, steps };
	}

	// ---- 吸附(UI用，纯函数) ----
	// 沿路/进深坐标的强制吸附：离两端(子块边)不到minGap就吸到端点(Core里这种情况矩形本来就会被拉伸到那条边)，否则按0.5取整。
	function snapValue(v, lo, hi) {
		v = Math.max(lo, Math.min(hi, v));
		if (v - lo < MIN_GAP) return lo;
		if (hi - v < MIN_GAP) return hi;
		return Math.max(lo, Math.min(hi, Math.round(v * 2) / 2));
	}
	// 进深吸附：离对边不到minGap就吸到对边(整块吃满)，否则按0.5取整。
	function snapDepth(v, depthMax) {
		v = Math.max(0, Math.min(depthMax, v));
		if (depthMax - v < MIN_GAP) return depthMax;
		return Math.round(v * 2) / 2;
	}
	// 子块sub在dir这一侧的"沿路范围"和"进深范围"。
	function edgeSpan(sub, dir) {
		return dir <= 1 ? { lo: sub.y0, hi: sub.y1, depthMax: sub.x1 - sub.x0 } : { lo: sub.x0, hi: sub.x1, depthMax: sub.y1 - sub.y0 };
	}
	// 由沿路区间[a,b](本地坐标，a<b)和进深depth(从路边向内)得到矩形。
	function rectFromEdge(sub, dir, a, b, depth) {
		if (dir === 0) return { x0: sub.x0, x1: sub.x0 + depth, y0: a, y1: b };
		if (dir === 1) return { x0: sub.x1 - depth, x1: sub.x1, y0: a, y1: b };
		if (dir === 2) return { y0: sub.y0, y1: sub.y0 + depth, x0: a, x1: b };
		return { y0: sub.y1 - depth, y1: sub.y1, x0: a, x1: b };
	}
	// 本地坐标点(x,y)里pool中哪块自由地，没有返回-1。
	const poolIndexAt = (pool, x, y) => pool.findIndex((c) => x >= c.x0 && x <= c.x1 && y >= c.y0 && y <= c.y1);
	// 点(x,y)在子块里最近的有路的边(0西1东2北3南)，没有路返回-1。
	function nearestRoadEdge(sub, x, y) {
		const dist = [Math.abs(x - sub.x0), Math.abs(x - sub.x1), Math.abs(y - sub.y0), Math.abs(y - sub.y1)];
		let best = -1;
		for (let d = 0; d < 4; d++) if (sub.road[d] && (best < 0 || dist[d] < dist[best])) best = d;
		return best;
	}

	function addPreset(model, lot, dir, rect) {
		lot.presets = lot.presets || [];
		const preset = { id: model.nextId++, modId: '', dir, x0: rect.x0, x1: rect.x1, y0: rect.y0, y1: rect.y1 };
		lot.presets.push(preset);
		return preset;
	}

	function exportLots(model, terrain, output, roadPieces, warnings) {
		for (const lot of model.lots) {
			const info = describeLot(model, lot, roadPieces);
			if (!info) { warnings.push(`地块${lot.id}的节点已不存在或深度为0，已跳过`); continue; }
			const boundary = {};
			const margin = [];
			for (let d = 0; d < 4; d++) {
				const dir = info.directions[d];
				margin.push(dir.margin);
				if (dir.road) boundary[String(d)] = roadPieces.get(dir.road.id)[0];
			}
			// 地块边长不能被margin吃光。
			const [n0, n1, n2] = info.ordered.nodes;
			const len1 = Math.hypot(n1[0] - n0[0], n1[1] - n0[1]), len2 = Math.hypot(n2[0] - n1[0], n2[1] - n1[1]);
			if (len1 - margin[0] - margin[1] <= 0.05 || len2 - margin[2] - margin[3] <= 0.05) {
				warnings.push(`地块${lot.id}的缩进把地块吃光了，已跳过`);
				continue;
			}
			const bad = nonBuildableCount(terrain, info.corners);
			if (bad > 0) warnings.push(`地块${lot.id}有${bad}个格子不是可建设区域(自身或3×3邻域内有高度不为0的格子，或贴着地图边缘)，游戏里这块地会被丢弃`);
			output.lots.push({
				corners: info.corners.map((c) => [c[0], c[1]]),
				margin, boundary, area: lot.area,
			});
			const replay = (lot.presets && lot.presets.length) ? replayPresets(model, lot, roadPieces) : null;
			if (replay) {
				output.presets = output.presets || [];
				lot.presets.forEach((p, i) => {
					const step = replay.steps[i];
					if (!step.placed) { warnings.push(`地块${lot.id}的预设${p.id}无效：${step.error}，已跳过`); return; }
					if (!p.modId) warnings.push(`地块${lot.id}的预设${p.id}还没有填id(游戏里只会占位，不会生成园区/建筑)`);
					output.presets.push({ lot: output.lots.length - 1, id: p.modId || '', direction: p.dir, rect: [p.x0, p.x1, p.y0, p.y1] });
				});
			}
		}
	}

	ME.AREAS = AREAS;
	ME.DIRECTION_NAMES = DIRECTION_NAMES;
	ME.lotCorners = lotCorners;
	ME.orderCorners = orderCorners;
	ME.addLot = addLot;
	ME.describeLot = describeLot;
	ME.insetCorners = insetCorners;
	ME.nonBuildableCount = nonBuildableCount;
	ME.exportLots = exportLots;
	ME.PRESET = { PATH_HALF, MIN_LOT_EXTENT, MIN_GAP };
	ME.lotFrame = lotFrame;
	ME.localToWorld = localToWorld;
	ME.worldToLocal = worldToLocal;
	ME.requestPlacement = requestPlacement;
	ME.placePresetRect = placePresetRect;
	ME.replayPresets = replayPresets;
	ME.snapValue = snapValue;
	ME.snapDepth = snapDepth;
	ME.edgeSpan = edgeSpan;
	ME.rectFromEdge = rectFromEdge;
	ME.poolIndexAt = poolIndexAt;
	ME.nearestRoadEdge = nearestRoadEdge;
	ME.addPreset = addPreset;
	if (typeof module !== 'undefined') module.exports = ME;
})(typeof window !== 'undefined' ? window : globalThis);
