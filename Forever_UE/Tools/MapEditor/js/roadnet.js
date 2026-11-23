// 路网编辑模型 + 几何 + 导出(纯逻辑，无DOM)。导出格式见 Source/Basic/map/roadnet_file.md。
//
// 模型(source)：
//   version: 2
//   nodes: [{id, x, y, z, extern?}]                   用户节点(路口/端点)，z=节点高度(element,10m)，extern=城外连接点(必须在地图边界上)
//   roads: [{id, a, b, ctrl:[x,y]|null, type, asset, lanes:[6], name, tree, lamp}]   tree/lamp=行道树/路灯tag字符串，''或缺省=没有
//          type: ground平地 / hug贴合地形 / mouth隧道口 / tunnel隧道 / bridge桥；ctrl是二次Bezier控制点(null=直线)
//          路面高度由头尾节点的z决定：相同走直线(平路)，不同走扁S形(两端水平、1/3长度处的控制点)；hug则跟随地形，头尾按节点z做偏移。
//          mouth用平地的路面资产但要给地形开口；tunnel用隧道资产；bridge放桥墩。
//   lots:  见lots.js
//   greens: 见greens.js(绿化区多边形，可连道路)
//   transit: 见transit.js(独立线路：火车铁轨/飞机航线 + 线路环)
// 一条用户road在导出时展开成Core Road("piece")：除hug(每1单位一段，跟随地形)外一对一。
// 坐标：element单位(10m)，+Y向南。
(function (root) {
	const ME = root.ME = root.ME || {};

	const LANE_WIDTH = 0.5;
	const FLAT_APPROACH = 1.5;      // (仅旧版文件迁移用)隧道口/桥头的水平引道
	const TUNNEL_RAMP = 8;          // (仅旧版文件迁移用)隧道口S坡长度
	const MAX_SLOPE = 0.25;          // 扁S坡最大坡度(每单位水平长度的高度变化)，超过就警告
	const CORNER_MIN_BEND = 5 * Math.PI / 180; // 度数2节点的拐弯超过这个角度才导成路口
	const HUG_STEP = 1;             // 贴地形爬升采样间距
	const HUG_LEVEL_RATIO = 0.25;   // 贴合段接非hug路的那一端，最后这么大比例的路长缓入到水平(两端各自算)
	const NODE_EPS = 0.05;          // 判断"就在节点上"的距离
	const SAME_LEVEL = 0.25;        // 交叉时两条路高度差小于这个才算同一层，才建路口

	// 车道资产(road.asset)：资产名，资产文件是 <asset>_<车道类型id>(隧道内加_T)，目前游戏里只有 default。
	const DEFAULT_ASSET = 'default';

	// 车道类型(road.lanes = 左车行,左停车,左人行,右车行,右停车,右人行)，是 default 资产的全集：
	// 每个类型都有对应的 default_<id> 资产、_T隧道资产和"无人行道"资产。
	// 方向：车道数组的前三项是道路正向(起点a→终点b)那一侧，后三项是反向(b→a)那一侧；下拉里名字的"左x右y"是道路从下往上走(屏幕上)时左边/右边的车行道数，所以 右y=lanes[0](a→b)、左x=lanes[3](b→a)；
	// 车行道只在某一侧时就是单行道，通行方向由哪一侧有车行道决定(forward=a→b的车道数，backward=b→a的车道数)。
	const LANE_PRESETS = [
		{ id: '1_0_1_1_0_1', name: '双向2车道(左1右1)', lanes: [1, 0, 1, 1, 0, 1] },
		{ id: '2_0_1_1_0_1', name: '3车道(左1右2)', lanes: [2, 0, 1, 1, 0, 1] },
		{ id: '1_0_1_2_0_1', name: '3车道(左2右1)', lanes: [1, 0, 1, 2, 0, 1] },
		{ id: '2_0_1_2_0_1', name: '双向4车道(左2右2)', lanes: [2, 0, 1, 2, 0, 1] },
		{ id: '2_0_1_0_0_1', name: '单向2车道(左0右2)', lanes: [2, 0, 1, 0, 0, 1] },
		{ id: '0_0_1_2_0_1', name: '单向2车道(左2右0)', lanes: [0, 0, 1, 2, 0, 1] },
	];
	for (const p of LANE_PRESETS) { p.forward = p.lanes[0]; p.backward = p.lanes[3]; }

	function roadWidth(road) {
		let n = 0;
		for (let i = 0; i < 6; i++) n += road.lanes[i];
		return n * LANE_WIDTH;
	}

	// ---------------- 模型 ----------------
	function newModel(mapW, mapH) {
		return { version: 2, mapW, mapH, nodes: [], roads: [], lots: [], greens: [], transit: { nodes: [], conns: [], loops: [], roadLines: [] }, nextId: 1 };
	}
	function cloneModel(model) { return JSON.parse(JSON.stringify(model)); }
	function nodeOf(model, id) { return model.nodes.find((n) => n.id === id); }
	function roadOf(model, id) { return model.roads.find((r) => r.id === id); }

	function addNode(model, x, y, extern, z) {
			const node = { id: model.nextId++, x, y, z: z || 0 };
		if (extern) node.extern = true;
		model.nodes.push(node);
		return node;
	}
	function nodeDegree(model, id) {
		let d = 0;
		for (const r of model.roads) { if (r.a === id) d++; if (r.b === id) d++; }
		return d;
	}

	// ---------------- 几何 ----------------
	const lerp = (p, q, t) => [p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t];
	function quadOf(model, road) {
		const a = nodeOf(model, road.a), b = nodeOf(model, road.b);
		return { p0: [a.x, a.y], c: road.ctrl ? [road.ctrl[0], road.ctrl[1]] : null, p1: [b.x, b.y] };
	}
	function evalQuad(q, t) {
		if (!q.c) return lerp(q.p0, q.p1, t);
		const u = 1 - t;
		return [u * u * q.p0[0] + 2 * u * t * q.c[0] + t * t * q.p1[0], u * u * q.p0[1] + 2 * u * t * q.c[1] + t * t * q.p1[1]];
	}
	function splitQuad(q, t) {
		if (!q.c) { const m = lerp(q.p0, q.p1, t); return [{ p0: q.p0, c: null, p1: m }, { p0: m, c: null, p1: q.p1 }]; }
		const q0 = lerp(q.p0, q.c, t), q1 = lerp(q.c, q.p1, t), m = lerp(q0, q1, t);
		return [{ p0: q.p0, c: q0, p1: m }, { p0: m, c: q1, p1: q.p1 }];
	}
	function subQuad(q, t0, t1) {
		const left = splitQuad(q, t1)[0];
		if (t0 <= 1e-9) return left;
		return splitQuad(left, t0 / t1)[1];
	}
	// 采样折线：[{x,y,t,s}]，s是累计弧长。
	function polyline(q) {
		const chord = Math.hypot(q.p1[0] - q.p0[0], q.p1[1] - q.p0[1]);
		const n = q.c ? Math.max(24, Math.ceil(chord * 6)) : 1;
		const points = [];
		let s = 0;
		for (let i = 0; i <= n; i++) {
			const t = i / n;
			const p = evalQuad(q, t);
			if (i > 0) s += Math.hypot(p[0] - points[i - 1].x, p[1] - points[i - 1].y);
			points.push({ x: p[0], y: p[1], t, s });
		}
		return points;
	}
	function tAtS(poly, s) {
		const total = poly[poly.length - 1].s;
		if (s <= 0) return 0;
		if (s >= total) return 1;
		let lo = 0, hi = poly.length - 1;
		while (hi - lo > 1) { const mid = (lo + hi) >> 1; if (poly[mid].s <= s) lo = mid; else hi = mid; }
		const span = poly[hi].s - poly[lo].s;
		const f = span > 1e-12 ? (s - poly[lo].s) / span : 0;
		return poly[lo].t + (poly[hi].t - poly[lo].t) * f;
	}
	function roadLength(model, road) { const p = polyline(quadOf(model, road)); return p[p.length - 1].s; }

	// 点到road的最近点：{t, dist, x, y}
	function nearestOnRoad(model, road, x, y) {
		const poly = polyline(quadOf(model, road));
		let best = { dist: Infinity };
		for (let i = 0; i + 1 < poly.length; i++) {
			const a = poly[i], b = poly[i + 1];
			const dx = b.x - a.x, dy = b.y - a.y;
			const len2 = dx * dx + dy * dy;
			let f = len2 > 1e-12 ? ((x - a.x) * dx + (y - a.y) * dy) / len2 : 0;
			f = Math.max(0, Math.min(1, f));
			const px = a.x + dx * f, py = a.y + dy * f;
			const dist = Math.hypot(x - px, y - py);
			if (dist < best.dist) best = { dist, x: px, y: py, t: a.t + (b.t - a.t) * f };
		}
		return best;
	}
	// 路在参数t处的单位切线(沿a→b方向)；退化返回null。
	function tangentAt(q, t) {
		let dx, dy;
		if (!q.c) { dx = q.p1[0] - q.p0[0]; dy = q.p1[1] - q.p0[1]; }
		else {
			const u = 1 - t;
			dx = 2 * u * (q.c[0] - q.p0[0]) + 2 * t * (q.p1[0] - q.c[0]);
			dy = 2 * u * (q.c[1] - q.p0[1]) + 2 * t * (q.p1[1] - q.c[1]);
		}
		const len = Math.hypot(dx, dy);
		return len < 1e-9 ? null : [dx / len, dy / len];
	}

	// 吸附角度：从已有道路的节点(origin={node:id})或路中间(origin={road:id,t})往外拉新路时，
	// 把朝向(x,y)的射线吸到特殊角度上：直角(垂直于已连道路，两侧都算)；节点上还有平角(沿已连道路的延长线继续直走，kind='straight')。
	// 节点上连了多条路时，取与鼠标方向夹角最小的那个候选。路中间只有直角(沿路走会和原路重叠，没有意义)。
	// 夹角在maxDeg度内才吸附。返回 {point:[x,y], dir:[dx,dy], angle(度), kind:'right'|'straight'}；不适用(节点没连路、鼠标太近、夹角太大)返回null。
	function angleSnapAt(model, origin, x, y, maxDeg) {
		let ox, oy;
		let straight = false; // 平角只对节点起点有效
		const dirs = [];
		if (origin && origin.node !== undefined) {
			const n = nodeOf(model, origin.node);
			if (!n) return null;
			ox = n.x; oy = n.y; straight = true;
			for (const r of model.roads) {
				let toward = null;
				if (r.a === n.id) toward = r.ctrl ? r.ctrl : (() => { const b = nodeOf(model, r.b); return [b.x, b.y]; })();
				else if (r.b === n.id) toward = r.ctrl ? r.ctrl : (() => { const a = nodeOf(model, r.a); return [a.x, a.y]; })();
				if (!toward) continue;
				const dx = toward[0] - ox, dy = toward[1] - oy, len = Math.hypot(dx, dy);
				if (len > 1e-9) dirs.push([dx / len, dy / len]);
			}
		} else if (origin && origin.road !== undefined) {
			const r = roadOf(model, origin.road);
			if (!r) return null;
			const q = quadOf(model, r);
			const p = evalQuad(q, origin.t), d = tangentAt(q, origin.t);
			if (!d) return null;
			ox = p[0]; oy = p[1]; dirs.push(d);
		} else return null;
		if (!dirs.length) return null;
		const vx = x - ox, vy = y - oy, vlen = Math.hypot(vx, vy);
		if (vlen < 0.05) return null;
		let best = null;
		const consider = (px, py, kind) => {
			const dot = (vx * px + vy * py) / vlen;
			const ang = Math.acos(Math.max(-1, Math.min(1, dot))) * 180 / Math.PI;
			if (!best || ang < best.angle) best = { angle: ang, dir: [px, py], kind };
		};
		for (const d of dirs) {
			consider(-d[1], d[0], 'right'); consider(d[1], -d[0], 'right');
			if (straight) consider(-d[0], -d[1], 'straight'); // d指向已连道路那一侧，延长线是反方向
		}
		if (best.angle > maxDeg) return null;
		const along = vx * best.dir[0] + vy * best.dir[1];
		return { point: [ox + best.dir[0] * along, oy + best.dir[1] * along], dir: best.dir, origin: [ox, oy], angle: best.angle, kind: best.kind };
	}
	function pickNode(model, x, y, radius) {
		let best = null, bestD = radius;
		for (const n of model.nodes) { const d = Math.hypot(n.x - x, n.y - y); if (d <= bestD) { best = n; bestD = d; } }
		return best;
	}
	function pickRoad(model, x, y, radius) {
		let best = null;
		for (const r of model.roads) {
			const hit = nearestOnRoad(model, r, x, y);
			if (hit.dist <= radius && (!best || hit.dist < best.dist)) best = { road: r, t: hit.t, dist: hit.dist, x: hit.x, y: hit.y };
		}
		return best;
	}

	// ---------------- 高度剖面 ----------------
	const smooth = (u) => u * u * (3 - 2 * u);
	// t(二次Bezier参数)对应的弧长比例。
	function sFrac(poly, t) {
		const L = poly[poly.length - 1].s;
		if (L <= 0) return 0;
		for (let i = 0; i + 1 < poly.length; i++) {
			if (t <= poly[i + 1].t) { const f = (t - poly[i].t) / Math.max(1e-12, poly[i + 1].t - poly[i].t); return (poly[i].s + (poly[i + 1].s - poly[i].s) * f) / L; }
		}
		return 1;
	}
	// road的高度剖面z(t)：
	//  - hug：地形高度 + 头尾偏移(节点z - 该处地形高度)按弧长线性过渡，所以紧贴地形又正好接上两端节点；
	//  - 其它：头尾节点z相同走平，不同走扁S：z0 + (z1-z0)*smooth(t)(导出的三次Bezier控制点正好是这条曲线)。
	function profileOf(model, road, ctx) {
		const a = nodeOf(model, road.a), b = nodeOf(model, road.b);
		const z0 = a.z || 0, z1 = b.z || 0;
		if (road.type !== 'hug') return (t) => z0 + (z1 - z0) * smooth(Math.max(0, Math.min(1, t)));
		const q = quadOf(model, road);
		const poly = polyline(q);
		const e0 = z0 - ctx.height(a.x, a.y), e1 = z1 - ctx.height(b.x, b.y);
		return (t) => { const p = evalQuad(q, t); return ctx.height(p[0], p[1]) + e0 + (e1 - e0) * sFrac(poly, t); };
	}
	function zAt(model, road, ctx, t) { return profileOf(model, road, ctx)(t); }

	// (仅迁移旧版文件用)旧的"隧道/桥自动拆段"布局。
	function planLayout(type, L, height) {
		const a0 = FLAT_APPROACH;
		const R0 = type === 'bridge' ? Math.max(TUNNEL_RAMP, Math.abs(height) * 8) : TUNNEL_RAMP;
		const need = 2 * (a0 + R0);
		const k = L < need + 0.5 ? Math.max(0.05, (L - 0.5) / need) : 1;
		const a = a0 * k, R = R0 * k;
		const mid = L - 2 * (a + R);
		return { a, R, mid: Math.max(0, mid), flatEnds: mid >= 0 };
	}

	// ---------------- 编辑操作 ----------------
	function canSplit(road) { return road.type === 'ground' || road.type === 'hug'; }

	// 在road的t处切开，返回新节点(已存在的端点/近端点直接返回对应节点)。隧道/桥不能从中间切，返回null。
	function splitRoadAt(model, roadId, t, existingNodeId, ctx) {
		const road = roadOf(model, roadId);
		const q = quadOf(model, road);
		const p = evalQuad(q, t);
		const a = nodeOf(model, road.a), b = nodeOf(model, road.b);
		if (Math.hypot(p[0] - a.x, p[1] - a.y) < NODE_EPS) return a;
		if (Math.hypot(p[0] - b.x, p[1] - b.y) < NODE_EPS) return b;
		if (!canSplit(road)) return null;
		const node = existingNodeId ? nodeOf(model, existingNodeId) : addNode(model, p[0], p[1], false, zAt(model, road, ctx || { height: () => 0 }, t));
		const [left, right] = splitQuad(q, t);
		const second = Object.assign({}, road, { id: model.nextId++, a: node.id, b: road.b, ctrl: right.c ? [right.c[0], right.c[1]] : null, lanes: road.lanes.slice() });
		road.b = node.id;
		road.ctrl = left.c ? [left.c[0], left.c[1]] : null;
		model.roads.push(second);
		if (ME.remapBusStops) ME.remapBusStops(model, road.id, t, second.id); // 公交站跟着落到后半段
		return node;
	}

	// 两条折线的第一个交点(按poly1上的s最小)。忽略离两条路任一端点 < NODE_EPS 的交点。
	function polyCross(p1, p2, endpoints) {
		let best = null;
		for (let i = 0; i + 1 < p1.length; i++) {
			for (let j = 0; j + 1 < p2.length; j++) {
				const a = p1[i], b = p1[i + 1], c = p2[j], d = p2[j + 1];
				const r = [b.x - a.x, b.y - a.y], s = [d.x - c.x, d.y - c.y];
				const den = r[0] * s[1] - r[1] * s[0];
				if (Math.abs(den) < 1e-12) continue;
				const u = ((c.x - a.x) * s[1] - (c.y - a.y) * s[0]) / den;
				const v = ((c.x - a.x) * r[1] - (c.y - a.y) * r[0]) / den;
				if (u < 0 || u > 1 || v < 0 || v > 1) continue;
				const x = a.x + r[0] * u, y = a.y + r[1] * u;
				if (endpoints.some((e) => Math.hypot(e[0] - x, e[1] - y) < NODE_EPS)) continue;
				const t1 = a.t + (b.t - a.t) * u, t2 = c.t + (d.t - c.t) * v;
				if (!best || t1 < best.t1) best = { x, y, t1, t2 };
			}
		}
		return best;
	}

	// 找road和别的road之间需要建路口的第一个交点(两条都是ground/hug且同一高度)，没有返回null。
	function findJunctionCrossing(model, road, ctx) {
		const q = quadOf(model, road);
		const poly = polyline(q);
		const ends = [q.p0, q.p1];
		let best = null;
		for (const other of model.roads) {
			if (other.id === road.id) continue;
			if (!canSplit(other) && !canSplit(road)) continue;
			const oq = quadOf(model, other);
			const cross = polyCross(poly, polyline(oq), ends.concat([oq.p0, oq.p1]));
			if (!cross) continue;
			if (!canSplit(road) || !canSplit(other)) continue;
			const z1 = zAt(model, road, ctx, cross.t1), z2 = zAt(model, other, ctx, cross.t2);
			if (Math.abs(z1 - z2) >= SAME_LEVEL) continue;
			if (!best || cross.t1 < best.t1) best = Object.assign({ other: other.id, z: (z1 + z2) / 2 }, cross);
		}
		return best;
	}

	// 把一个端点引用解析成节点：{node:id} | {road:id,t} | {x,y}。
	function resolveEnd(model, ref, ctx) {
		if (ref.node !== undefined) return nodeOf(model, ref.node);
		if (ref.road !== undefined) return splitRoadAt(model, ref.road, ref.t, undefined, ctx);
				return addNode(model, ref.x, ref.y, ref.extern, ref.z);
	}

	// 新建一条路。spec: {a, b, ctrl, type, lanes, name}，a/b是端点引用：{node:id}(用已有节点，保持它的高度) | {road:id,t}(切开那条路，新节点高度取路在该处的高度)
		// | {x,y,z?,extern?}(新建节点，z=节点高度)。路面高度由头尾节点的z决定。返回 {road, error?}。
	// 自动处理：吸附(由引用表达)、T口(引用{road,t}会切开那条路)、与同层路的十字交叉(切开两条路并共用新节点)。
	function addRoad(model, spec, ctx) {
		ctx = ctx || { height: () => 0 };
		const a = resolveEnd(model, spec.a, ctx);
		const b = resolveEnd(model, spec.b, ctx);
		if (!a || !b) return { error: '隧道/桥不能从中间接路，请接在两端的节点上' };
		if (a.id === b.id) return { error: '起点和终点相同' };
		const road = {
			id: model.nextId++, a: a.id, b: b.id,
			ctrl: spec.ctrl ? [spec.ctrl[0], spec.ctrl[1]] : null,
			type: spec.type || 'ground',
			asset: spec.asset || DEFAULT_ASSET,
			lanes: (spec.lanes || LANE_PRESETS[0].lanes).slice(),
			name: spec.name || '',
			tree: spec.tree || '', lamp: spec.lamp || '',   // 行道树/路灯tag(''=没有)，游戏侧roadnet_file按tag选资产
		};
		// (路面高度来自头尾节点z)
		model.roads.push(road);
		if (!road.name) road.name = '路' + road.id;
		// 新路穿过已有节点(正好经过别的路的端点/路口)：在该节点处切开，让那里成为路口。
		const pieces = [road];
		let tail = road;
		for (let guard = 0; guard < 256 && canSplit(tail); guard++) {
			const q = quadOf(model, tail);
			let best = null;
			const zOfTail = profileOf(model, tail, ctx);
						for (const n of model.nodes) {
							if (n.id === tail.a || n.id === tail.b) continue;
							const hit = nearestOnRoad(model, tail, n.x, n.y);
							if (hit.dist < 0.08 && hit.t > 0.001 && hit.t < 0.999 && Math.abs(zOfTail(hit.t) - (n.z || 0)) < SAME_LEVEL && (!best || hit.t < best.t)) best = { node: n, t: hit.t };
						}
			if (!best) break;
			const before = model.roads.length;
			splitRoadAt(model, tail.id, best.t, best.node.id);
			if (model.roads.length === before) break;
			tail = model.roads[model.roads.length - 1];
			pieces.push(tail);
			void q;
		}
		// 十字交叉：对每一段反复找第一个交点，切开这一段和被穿过的路，直到没有。
		for (const piece of pieces) {
			let guard = 0;
			let current = piece;
			while (guard++ < 64) {
				const cross = findJunctionCrossing(model, current, ctx);
				if (!cross) break;
				const node = addNode(model, cross.x, cross.y, false, cross.z);
				splitRoadAt(model, cross.other, cross.t2, node.id);
				// 切这一段：current保留前半段，后半段(新road)继续找交点。
				const before = model.roads.length;
				splitRoadAt(model, current.id, cross.t1, node.id);
				if (model.roads.length === before) break;
				current = model.roads[model.roads.length - 1];
			}
		}
		return { road, endNode: b.id };
	}

	function deleteRoad(model, roadId) {
		model.roads = model.roads.filter((r) => r.id !== roadId);
		cleanup(model);
	}
	// 清理：删掉没有路连着的孤立节点(extern保留)，以及引用了不存在节点的地块。
	function cleanup(model) {
		const used = new Set();
		for (const r of model.roads) { used.add(r.a); used.add(r.b); }
		for (const l of model.lots) { used.add(l.a); used.add(l.b); }
		model.nodes = model.nodes.filter((n) => used.has(n.id) || n.extern);
		const ids = new Set(model.nodes.map((n) => n.id));
		model.lots = model.lots.filter((l) => ids.has(l.a) && ids.has(l.b));
		if (ME.cleanupBusLines) ME.cleanupBusLines(model);
	}
	function deleteNode(model, nodeId) {
		model.roads = model.roads.filter((r) => r.a !== nodeId && r.b !== nodeId);
		model.lots = model.lots.filter((l) => l.a !== nodeId && l.b !== nodeId);
		model.nodes = model.nodes.filter((n) => n.id !== nodeId);
		cleanup(model);
	}
	function moveNode(model, nodeId, x, y) {
		const n = nodeOf(model, nodeId);
		if (!n) return;
		const dx = x - n.x, dy = y - n.y;
		n.x = x; n.y = y;
		// 控制点跟着移动一半，避免曲线形状突变。
		for (const r of model.roads) {
			if ((r.a === nodeId || r.b === nodeId) && r.ctrl) { r.ctrl = [r.ctrl[0] + dx / 2, r.ctrl[1] + dy / 2]; }
		}
	}

	// ---------------- 导出(展开成Core的Road) ----------------
	// 把sub曲线[t0,t1]做成一条piece的控制点。z0/z1端点高度，sl0/sl1端点坡度(每单位水平长度的z变化)。
	function makeControls(sub, z0, z1, sl0, sl1, length) {
		const flat = Math.abs(z0 - z1) < 1e-6 && Math.abs(sl0) < 1e-9 && Math.abs(sl1) < 1e-9;
		if (flat) return sub.c ? [[sub.c[0], sub.c[1], z0, 1]] : [];
		let c1, c2;
		if (sub.c) {
			c1 = [sub.p0[0] + (2 / 3) * (sub.c[0] - sub.p0[0]), sub.p0[1] + (2 / 3) * (sub.c[1] - sub.p0[1])];
			c2 = [sub.p1[0] + (2 / 3) * (sub.c[0] - sub.p1[0]), sub.p1[1] + (2 / 3) * (sub.c[1] - sub.p1[1])];
		} else {
			c1 = lerp(sub.p0, sub.p1, 1 / 3);
			c2 = lerp(sub.p0, sub.p1, 2 / 3);
		}
		return [[c1[0], c1[1], z0 + sl0 * length / 3, 1], [c2[0], c2[1], z1 - sl1 * length / 3, 1]];
	}

	// 单调三次(PCHIP)斜率，防止爬升段上下过冲。
	function pchipSlopes(ss, zs) {
		const n = ss.length;
		const d = [];
		for (let i = 0; i + 1 < n; i++) d.push((zs[i + 1] - zs[i]) / Math.max(1e-9, ss[i + 1] - ss[i]));
		const m = new Array(n).fill(0);
		if (n === 2) { m[0] = m[1] = d[0]; return m; }
		for (let i = 1; i + 1 < n; i++) m[i] = d[i - 1] * d[i] <= 0 ? 0 : 2 * d[i - 1] * d[i] / (d[i - 1] + d[i]);
		m[0] = d[0]; m[n - 1] = d[n - 2];
		if (m[0] * d[0] <= 0) m[0] = 0;
		if (m[n - 1] * d[n - 2] <= 0) m[n - 1] = 0;
		return m;
	}

	// 展开一条用户road：返回 [{t0,t1,z0,z1,sl0,sl1,kind}]。
	// 除hug外一对一：头尾节点z相同走平、不同走扁S(sl0/sl1=0表示两端水平)；hug按地形每HUG_STEP一段，坡度用PCHIP保证不过冲。
	function planPieces(model, road, ctx) {
		const q = quadOf(model, road);
		const a = nodeOf(model, road.a), b = nodeOf(model, road.b);
		const z0 = a.z || 0, z1 = b.z || 0;
		if (road.type !== 'hug') return [{ t0: 0, t1: 1, z0, z1, sl0: 0, sl1: 0, kind: road.type }];
		const poly = polyline(q);
		const L = poly[poly.length - 1].s;
		const zOf = profileOf(model, road, ctx);
		const count = Math.max(1, Math.ceil(L / HUG_STEP));
		const ss = [], ts = [], zs = [];
		let maxZ = 0;
		for (let i = 0; i <= count; i++) {
			const s = L * i / count, t = tAtS(poly, s);
			const z = zOf(t);
			ss.push(s); ts.push(t); zs.push(z); maxZ = Math.max(maxZ, Math.abs(z));
		}
		if (maxZ < 0.02) return [{ t0: 0, t1: 1, z0: 0, z1: 0, sl0: 0, sl1: 0, kind: 'ground' }];
		// 连着非hug路(mouth/tunnel/ground/bridge，它们的端点都是水平的)的那一端：最后HUG_LEVEL_RATIO(1/4)路长内
		// 用smoothstep从"贴地形的高度"缓入到端点高度，终点斜率为0，和后面的平路接上不留折角。
		const levelEnd = (nodeId) => model.roads.some((r) => r.id !== road.id && (r.a === nodeId || r.b === nodeId) && r.type !== 'hug');
		const ramp = L * HUG_LEVEL_RATIO;
		const level0 = levelEnd(road.a), level1 = levelEnd(road.b);
		if (ramp > 1e-9) {
			const zFirst = zs[0], zLast = zs[count];
			for (let i = 0; i <= count; i++) {
				if (level0 && ss[i] < ramp) zs[i] = zFirst + (zs[i] - zFirst) * smooth(ss[i] / ramp);
				if (level1 && L - ss[i] < ramp) zs[i] = zLast + (zs[i] - zLast) * smooth((L - ss[i]) / ramp);
			}
		}
		const m = pchipSlopes(ss, zs);
		if (level0) m[0] = 0;
		if (level1) m[count] = 0;
		const out = [];
		for (let i = 0; i < count; i++) out.push({ t0: ts[i], t1: ts[i + 1], z0: zs[i], z1: zs[i + 1], sl0: m[i], sl1: m[i + 1], kind: 'hug' });
		return out;
	}

	// 桥墩链：连续的桥(经过只连着两条桥的节点)归成一条，按行进顺序排好并标出每段是否要反向，
	// 游戏侧(roadnet_file)据此拼出首尾相接的Connection链。返回 Map(roadId -> {chain, order, reverse})。
	function bridgeChains(model) {
		const degree = new Map();
		for (const r of model.roads) { degree.set(r.a, (degree.get(r.a) || 0) + 1); degree.set(r.b, (degree.get(r.b) || 0) + 1); }
		const byNode = new Map();
		for (const r of model.roads) {
			if (r.type !== 'bridge') continue;
			for (const id of [r.a, r.b]) if (degree.get(id) === 2) { if (!byNode.has(id)) byNode.set(id, []); byNode.get(id).push(r); }
		}
		const visited = new Set();
		const result = new Map();
		let chainId = 0;
		for (const r of model.roads) {
			if (r.type !== 'bridge' || visited.has(r.id)) continue;
			visited.add(r.id);
			const list = [{ road: r, reverse: false }];
			let endNode = r.b;
			for (;;) {
				const next = (byNode.get(endNode) || []).find((x) => !visited.has(x.id));
				if (!next) break;
				visited.add(next.id);
				if (next.a === endNode) { list.push({ road: next, reverse: false }); endNode = next.b; } else { list.push({ road: next, reverse: true }); endNode = next.a; }
			}
			let startNode = r.a;
			for (;;) {
				const prev = (byNode.get(startNode) || []).find((x) => !visited.has(x.id));
				if (!prev) break;
				visited.add(prev.id);
				if (prev.b === startNode) { list.unshift({ road: prev, reverse: false }); startNode = prev.a; } else { list.unshift({ road: prev, reverse: true }); startNode = prev.b; }
			}
			list.forEach((e, order) => result.set(e.road.id, { chain: chainId, order, reverse: e.reverse }));
			chainId++;
		}
		return result;
	}

	// 度数2的节点是不是拐角：两条路离开节点的方向夹角小于180度-CORNER_MIN_BEND(约5度)。
	function isCorner(model, nodeId) {
		const dirs = [];
		for (const r of model.roads) {
			if (r.a !== nodeId && r.b !== nodeId) continue;
			const q = quadOf(model, r);
			const from = r.a === nodeId ? q.p0 : q.p1, toward = r.a === nodeId ? (q.c || q.p1) : (q.c || q.p0);
			const dx = toward[0] - from[0], dy = toward[1] - from[1], len = Math.hypot(dx, dy);
			if (len > 1e-9) dirs.push([dx / len, dy / len]);
		}
		if (dirs.length !== 2) return false;
		const cos = dirs[0][0] * dirs[1][0] + dirs[0][1] * dirs[1][1];
		return Math.acos(Math.max(-1, Math.min(1, cos))) < Math.PI - CORNER_MIN_BEND;
	}

	function exportRoadnet(model, terrain) {
		const ctx = { height: terrain ? (x, y) => terrain.sampleHeight(x, y) : () => 0 };
		const warnings = [];
		const nodes = [];
		const roads = [];
		const roadPieces = new Map();   // 用户roadId -> output roads下标数组
		const pieceRanges = new Map();  // 用户roadId -> [{t0,t1}]，和roadPieces一一对应(每段占用户road的Bezier参数区间)
		let internal = 0;

		// 用户节点：类型按度数定，高度就是节点z。
		const degree = new Map();
		for (const r of model.roads) { degree.set(r.a, (degree.get(r.a) || 0) + 1); degree.set(r.b, (degree.get(r.b) || 0) + 1); }
		for (const n of model.nodes) {
			const d = degree.get(n.id) || 0;
			if (d === 0 && !n.extern) continue;
			// 度数2的节点：两条路夹角偏离直线(拐角)才导成路口(才有缩进和路口mesh，避免拐角处两条路重叠)；近似直线的仍是plain(坡段/隧道衔接点)。
			const type = n.extern ? 'extern' : (d === 2 ? (isCorner(model, n.id) ? 'intersection' : 'plain') : 'intersection');
			nodes.push({ id: 'n' + n.id, x: n.x, y: n.y, z: n.z || 0, type });
		}
		const chains = bridgeChains(model);
		// 连着隧道(type=tunnel的路)的节点：隧道口(mouth)靠这些节点的那一端要加端墙，封住隧道资产房顶和地形之间的空洞。
		const tunnelNodes = new Set();
		for (const r of model.roads) if (r.type === 'tunnel') { tunnelNodes.add(r.a); tunnelNodes.add(r.b); }

		for (const r of model.roads) {
			const q = quadOf(model, r);
			const pieces = planPieces(model, r, ctx);
			const name = r.name || ('路' + r.id);
			// 内部节点：每段首尾相接，端点用用户节点，中间点新建plain节点。
			const idx = [];
			let prevId = 'n' + r.a;
			for (let i = 0; i < pieces.length; i++) {
				const p = pieces[i];
				const sub = subQuad(q, p.t0, p.t1);
				let toId;
				if (i === pieces.length - 1) toId = 'n' + r.b;
				else {
					toId = 'p' + (internal++);
					nodes.push({ id: toId, x: sub.p1[0], y: sub.p1[1], z: p.z1, type: 'plain' });
				}
				const length = polyline(sub).pop().s;
				const entry = {
					name, from: prevId, to: toId,
					controls: makeControls(sub, p.z0, p.z1, p.sl0, p.sl1, length),
					asset: r.asset || DEFAULT_ASSET, lanes: r.lanes.slice(), kind: p.kind,
				};
				// 行道树/路灯tag：只给平面(ground)路段导出；空串=这条路不种树/不放灯。
				if (p.kind === 'ground') { entry.treeTag = r.tree || ''; entry.lampTag = r.lamp || ''; }
				if (p.kind === 'mouth') {
					// 隧道口靠隧道的那一端加端墙(写wallFrom/wallTo给游戏侧)，和两端高度无关
					if (i === 0 && tunnelNodes.has(r.a)) entry.wallFrom = true;
					if (i === pieces.length - 1 && tunnelNodes.has(r.b)) entry.wallTo = true;
				}
				if (p.kind === 'bridge' && chains.has(r.id)) Object.assign(entry, chains.get(r.id));
				roads.push(entry);
				idx.push(roads.length - 1);
				prevId = toId;
			}
			roadPieces.set(r.id, idx);
			pieceRanges.set(r.id, pieces.map((p) => ({ t0: p.t0, t1: p.t1 })));
		}

		const output = { nodes, roads, lots: [], greens: [] };
		if (ME.exportLots) ME.exportLots(model, terrain, output, roadPieces, warnings);
		if (ME.exportGreens) ME.exportGreens(model, terrain, output, roadPieces, warnings, pieceRanges);
		if (ME.exportTransit) ME.exportTransit(model, terrain, output, warnings);
		if (ME.exportBusLines) ME.exportBusLines(model, output, roadPieces, pieceRanges, warnings);
		for (const w of validate(model, terrain, ctx)) warnings.push(w);
		return { output, warnings, roadPieces };
	}

	// ---------------- 校验(返回警告字符串数组) ----------------
	function validate(model, terrain, ctx) {
		ctx = ctx || { height: terrain ? (x, y) => terrain.sampleHeight(x, y) : () => 0 };
		const warnings = [];
		for (const r of model.roads) {
			const q = quadOf(model, r);
			const poly = polyline(q);
			const total = poly[poly.length - 1].s;
			const a = nodeOf(model, r.a), b = nodeOf(model, r.b);
			if (r.type === 'ground' && terrain) {
				// 平地路必须贴着地形(高度一致)且不能压在水上。
				const zOf = profileOf(model, r, ctx);
				let outside = false, floating = false, wet = false;
				const count = Math.max(1, Math.ceil(total / 0.5));
				for (let k = 0; k <= count; k++) {
					const t = tAtS(poly, total * k / count);
					const pp = evalQuad(q, t);
					if (pp[0] < 0 || pp[1] < 0 || pp[0] > terrain.w || pp[1] > terrain.h) { outside = true; continue; }
					if (Math.abs(ctx.height(pp[0], pp[1]) - zOf(t)) > 0.05) floating = true;
					const cx = Math.min(terrain.w - 1, Math.floor(pp[0])), cy = Math.min(terrain.h - 1, Math.floor(pp[1]));
					const level = terrain.water[cy * terrain.w + cx];
					if (!isNaN(level) && level > zOf(t) + 0.02) wet = true; // 水面高过路面才算泡在水里；地面以下的水(岸边平原格)没问题
				}
				if (outside) warnings.push(`路${r.id}超出地图`);
				if (floating) warnings.push(`路${r.id}(平地)的高度和地形对不上，建议改成"贴合"/隧道/桥，或把节点高度设成地形高度`);
				if (wet) warnings.push(`路${r.id}(平地)路面低于水面(泡在水里)，建议改成桥`);
			}
			if (r.type !== 'hug') {
				const dz = Math.abs((a.z || 0) - (b.z || 0));
				if (dz > 1e-6 && total > 1e-6 && 1.5 * dz / total > MAX_SLOPE) warnings.push(`路${r.id}头尾高度差${dz.toFixed(2)}但长度只有${total.toFixed(1)}，坡度太陡(最大${(150 * dz / total).toFixed(0)}%)，拉长或减小高度差`);
			}
		}
		// 不同层的交叉：高度差太小会穿模。
		for (let i = 0; i < model.roads.length; i++) {
			for (let j = i + 1; j < model.roads.length; j++) {
				const a = model.roads[i], b = model.roads[j];
				if (a.a === b.a || a.a === b.b || a.b === b.a || a.b === b.b) continue;
				const qa = quadOf(model, a), qb = quadOf(model, b);
				const cross = polyCross(polyline(qa), polyline(qb), []);
				if (!cross) continue;
				const za = zAt(model, a, ctx, cross.t1), zb = zAt(model, b, ctx, cross.t2);
				if (Math.abs(za - zb) < SAME_LEVEL) warnings.push(`路${a.id}与路${b.id}在(${cross.x.toFixed(1)},${cross.y.toFixed(1)})同层交叉但没有路口(隧道/桥不能从中间接路)`);
				else if (Math.abs(za - zb) < 0.6) warnings.push(`路${a.id}与路${b.id}在(${cross.x.toFixed(1)},${cross.y.toFixed(1)})上下净空只有${Math.abs(za - zb).toFixed(2)}`);
			}
		}
		for (const n of model.nodes) {
			if (n.extern && !(n.x <= 0.01 || n.y <= 0.01 || n.x >= model.mapW - 0.01 || n.y >= model.mapH - 0.01)) warnings.push(`城外连接点${n.id}不在地图边界上`);
		}
		return warnings;
	}

	// 旧版(version 1，路自己带height、隧道/桥导出时自动拆段)迁移成新版：节点补z，隧道/桥按旧布局拆成多条路(引道/坡=隧道口或桥，平路=隧道或桥)。
	function migrateModel(model, ctx) {
		if (!model.greens) model.greens = [];
		for (const r of model.roads) if (!r.asset) r.asset = DEFAULT_ASSET; // 旧文件没有车道资产字段，补默认资产
		if (!model.transit) model.transit = { nodes: [], conns: [], loops: [], roadLines: [] };
		if (!model.transit.roadLines) model.transit.roadLines = [];
		for (const l of model.lots) if (!l.presets) l.presets = [];
		if (model.version >= 2) return model;
		ctx = ctx || { height: () => 0 };
		for (const n of model.nodes) n.z = n.z || 0;
		for (const r of model.roads) {
			if (r.type !== 'hug') continue;
			for (const id of [r.a, r.b]) { const n = nodeOf(model, id); n.z = Math.max(n.z, ctx.height(n.x, n.y)); }
		}
		const old = model.roads;
		model.roads = [];
		for (const r of old) {
			const height = r.height;
			delete r.height;
			if (r.type !== 'tunnel' && r.type !== 'bridge') { model.roads.push(r); continue; }
			const q = quadOf(model, r), poly = polyline(q), L = poly[poly.length - 1].s;
			const h = height !== undefined ? height : (r.type === 'bridge' ? 1 : -1);
			const lay = planLayout(r.type, L, h);
			const tunnel = r.type === 'tunnel';
			const bounds = [0, lay.a, lay.a + lay.R, L - lay.a - lay.R, L - lay.a, L];
			const rampKind = tunnel ? 'mouth' : 'bridge', midKind = tunnel ? 'tunnel' : 'bridge';
			const defs = [{ z1: 0, kind: rampKind }, { z1: h, kind: rampKind }, { z1: h, kind: midKind }, { z1: 0, kind: rampKind }, { z1: 0, kind: rampKind }]
				.map((d, i) => Object.assign(d, { s0: bounds[i], s1: bounds[i + 1] })).filter((d) => d.s1 - d.s0 >= 0.05);
			if (!defs.length) { model.roads.push(r); continue; }
			let prev = nodeOf(model, r.a);
			defs.forEach((d, i) => {
				const t0 = i === 0 ? 0 : tAtS(poly, d.s0), t1 = i === defs.length - 1 ? 1 : tAtS(poly, d.s1);
				const sub = subQuad(q, t0, t1);
				const last = i === defs.length - 1;
				const next = last ? nodeOf(model, r.b) : addNode(model, sub.p1[0], sub.p1[1], false, d.z1);
				model.roads.push({ id: model.nextId++, a: prev.id, b: next.id, ctrl: sub.c ? [sub.c[0], sub.c[1]] : null, type: d.kind, asset: r.asset || DEFAULT_ASSET, lanes: r.lanes.slice(), name: r.name });
				prev = next;
			});
		}
		model.version = 2;
		return model;
	}

	ME.LANE_PRESETS = LANE_PRESETS;
	ME.DEFAULT_ASSET = DEFAULT_ASSET;
	ME.LANE_WIDTH = LANE_WIDTH;
	ME.roadWidth = roadWidth;
	ME.newModel = newModel;
	ME.cloneModel = cloneModel;
	ME.nodeOf = nodeOf;
	ME.roadOf = roadOf;
	ME.addNode = addNode;
	ME.nodeDegree = nodeDegree;
	ME.quadOf = quadOf;
	ME.evalQuad = evalQuad;
	ME.polyline = polyline;
	ME.roadLength = roadLength;
	ME.nearestOnRoad = nearestOnRoad;
	ME.tangentAt = tangentAt;
	ME.pickNode = pickNode;
	ME.angleSnapAt = angleSnapAt;
	ME.pickRoad = pickRoad;
	ME.zAt = zAt;
	ME.canSplit = canSplit;
	ME.splitRoadAt = splitRoadAt;
	ME.addRoad = addRoad;
	ME.deleteRoad = deleteRoad;
	ME.deleteNode = deleteNode;
	ME.moveNode = moveNode;
	ME.cleanup = cleanup;
	ME.planPieces = planPieces;
	ME.exportRoadnet = exportRoadnet;
	ME.validate = validate;
	ME.migrateModel = migrateModel;
	ME.profileOf = profileOf;
	ME.bridgeChains = bridgeChains;
	ME.MAX_SLOPE = MAX_SLOPE;
	if (typeof module !== 'undefined') module.exports = ME;
})(typeof window !== 'undefined' ? window : globalThis);
