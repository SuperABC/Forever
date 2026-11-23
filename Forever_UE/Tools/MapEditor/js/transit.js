// 独立线路(火车铁轨/飞机航线)编辑模型 + 线路环 + 导出(纯逻辑，无DOM)。
//
// 模型(source.transit，挂在 model.transit，和路网分开存，id与路网共用 model.nextId)：
//   nodes: [{id, x, y, z}]                                   线路节点，z=节点高度(element,10m)
//   conns: [{id, a, b, ctrl:[x,y]|null, type, routeId}]      线路连线(二次Bezier，同路网)；type: ground/hug/mouth/tunnel/bridge
//          routeId = route mod 的id(如 route_train / route_air)，''=还没填；同一个node上的连线routeId必须一致(连线规则保证)。
//   loops: [{id, routeId, items:[{kind:'preset', ref:预设id, stop}|{kind:'node', ref:节点id}]}]
//          线路环：依次经过 预设建筑 / 线路节点，首尾相接成环。stop只对preset有意义(node恒不停)，默认false。
// 导出(output.transit)见 Source/Basic/map/roadnet_file.md：
//   {nodes:[{x,y,z}], conns:[{from,to,ctrl,kind,route}], loops:[{route, items:[{preset:output.presets下标,stop}|{node:output.transit.nodes下标}]}]}
// 实体线路只是几何：不连车站；站↔线路之间的缺口由游戏里的route mod用扁S曲线补(编辑器里画虚线)。
(function (root) {
	const ME = root.ME = root.ME || {};

	const NODE_EPS = 0.05;
	const TYPES = ['ground', 'hug', 'mouth', 'tunnel', 'bridge'];
	const TYPE_NAMES = { ground: '平地', hug: '贴合地形', mouth: '隧道口', tunnel: '隧道', bridge: '桥' };

	function newTransit() { return { nodes: [], conns: [], loops: [] }; }
	function ensure(model) { if (!model.transit) model.transit = newTransit(); const t = model.transit; t.nodes = t.nodes || []; t.conns = t.conns || []; t.loops = t.loops || []; return t; }
	// 让 roadnet.js 里的曲线函数(quadOf/nearestOnRoad/profileOf…)直接用线路数据：nodes→nodes，conns→roads。
	function view(model) { const t = ensure(model); return { nodes: t.nodes, roads: t.conns }; }
	const nodeOf = (model, id) => ensure(model).nodes.find((n) => n.id === id);
	const connOf = (model, id) => ensure(model).conns.find((c) => c.id === id);
	const loopOf = (model, id) => ensure(model).loops.find((l) => l.id === id);

	// 路线id → 颜色(字符串哈希→色相)。空id=灰。
	function routeColor(id) {
		if (!id) return '#9aa0a6';
		let h = 2166136261 >>> 0;
		for (let i = 0; i < id.length; i++) { h ^= id.charCodeAt(i); h = Math.imul(h, 16777619) >>> 0; }
		const hue = h % 360, s = 0.7, l = 0.55;
		const c = (1 - Math.abs(2 * l - 1)) * s, x = c * (1 - Math.abs((hue / 60) % 2 - 1)), m = l - c / 2;
		const [r, g, b] = hue < 60 ? [c, x, 0] : hue < 120 ? [x, c, 0] : hue < 180 ? [0, c, x] : hue < 240 ? [0, x, c] : hue < 300 ? [x, 0, c] : [c, 0, x];
		return '#' + [r, g, b].map((v) => Math.round((v + m) * 255).toString(16).padStart(2, '0')).join('');
	}

	// ---------------- 基本操作 ----------------
	// extern=城外连接点：在地图边界上，线路从这里出城/回城(和路网的extern同概念)。
	function addNode(model, x, y, z, extern) {
		const node = { id: model.nextId++, x, y, z: z || 0 };
		if (extern) node.extern = true;
		ensure(model).nodes.push(node);
		return node;
	}
	// 这个node上连着的线路id集合(忽略空id)。
	function nodeRouteIds(model, nodeId) {
		const ids = new Set();
		for (const c of ensure(model).conns) if ((c.a === nodeId || c.b === nodeId) && c.routeId) ids.add(c.routeId);
		return ids;
	}
	function nodeDegree(model, nodeId) {
		let d = 0;
		for (const c of ensure(model).conns) { if (c.a === nodeId) d++; if (c.b === nodeId) d++; }
		return d;
	}
	const quadOf = (model, c) => ME.quadOf(view(model), c);
	const connLength = (model, c) => { const p = ME.polyline(quadOf(model, c)); return p[p.length - 1].s; };
	const zProfile = (model, c, ctx) => ME.profileOf(view(model), c, ctx || { height: () => 0 });

	function pickNode(model, x, y, radius) {
		let best = null, bestD = radius;
		for (const n of ensure(model).nodes) { const d = Math.hypot(n.x - x, n.y - y); if (d <= bestD) { best = n; bestD = d; } }
		return best;
	}
	function pickConn(model, x, y, radius) {
		let best = null;
		for (const c of ensure(model).conns) {
			const hit = ME.nearestOnRoad(view(model), c, x, y);
			if (hit.dist <= radius && (!best || hit.dist < best.dist)) best = { conn: c, t: hit.t, dist: hit.dist, x: hit.x, y: hit.y };
		}
		return best;
	}

	// 在conn的t处切开，返回新节点(已在端点附近就返回端点)。任何类型都能切；新节点高度=conn在该处的高度。
	function splitConnAt(model, connId, t, ctx) {
		const c = connOf(model, connId);
		const q = quadOf(model, c);
		const p = ME.evalQuad(q, t);
		const a = nodeOf(model, c.a), b = nodeOf(model, c.b);
		if (Math.hypot(p[0] - a.x, p[1] - a.y) < NODE_EPS) return a;
		if (Math.hypot(p[0] - b.x, p[1] - b.y) < NODE_EPS) return b;
		const node = addNode(model, p[0], p[1], zProfile(model, c, ctx)(t));
		// 二次Bezier在t处切成两段。
		const lerp = (u, v, f) => [u[0] + (v[0] - u[0]) * f, u[1] + (v[1] - u[1]) * f];
		let c1 = null, c2 = null;
		if (q.c) { c1 = lerp(q.p0, q.c, t); c2 = lerp(q.c, q.p1, t); }
		const second = Object.assign({}, c, { id: model.nextId++, a: node.id, b: c.b, ctrl: c2 });
		c.b = node.id;
		c.ctrl = c1;
		ensure(model).conns.push(second);
		return node;
	}

	// 端点引用 {node:id} | {conn:id,t} | {x,y,z?} → 这个端点上"要求/带来"的routeId('' = 不限)。
	function refRouteId(model, ref) {
		if (ref.node !== undefined) { const ids = [...nodeRouteIds(model, ref.node)]; return ids.length ? ids[0] : ''; }
		if (ref.conn !== undefined) { const c = connOf(model, ref.conn); return c ? c.routeId : ''; }
		return '';
	}
	// 连线的route id规则(见README)：
	//  - 面板指定了R：起点只能是空地/孤立node/routeId==R的线；终点同理。
	//  - 面板没指定：起点是线→新线继承它的id；终点是线且新线还没id→继承终点的id；两端都有id且不同→拒绝。
	// 返回 {routeId} 或 {error}。
	function resolveRouteId(model, panelId, aRef, bRef) {
		const ra = refRouteId(model, aRef), rb = refRouteId(model, bRef);
		if (panelId) {
			if (ra && ra !== panelId) return { error: `起点属于线路 ${ra}，和当前填的 ${panelId} 不同` };
			if (rb && rb !== panelId) return { error: `终点属于线路 ${rb}，和当前填的 ${panelId} 不同` };
			return { routeId: panelId };
		}
		if (ra && rb && ra !== rb) return { error: `起点(${ra})和终点(${rb})属于不同线路，不能连在一起` };
		return { routeId: ra || rb || '' };
	}

	function resolveEnd(model, ref, ctx) {
		if (ref.node !== undefined) return nodeOf(model, ref.node);
		if (ref.conn !== undefined) return splitConnAt(model, ref.conn, ref.t, ctx);
		return addNode(model, ref.x, ref.y, ref.z, ref.extern);
	}

	// 新建一条线路。spec: {a, b, ctrl, type, routeId(面板填的，可空)}；a/b是端点引用。返回 {conn, error?}。
	// 先校验再动模型：规则不满足时不产生任何副作用。
	function addConn(model, spec, ctx) {
		ctx = ctx || { height: () => 0 };
		ensure(model);
		if (spec.a.node !== undefined && spec.b.node !== undefined && spec.a.node === spec.b.node) return { error: '起点和终点相同' };
		if (spec.a.node !== undefined && !nodeOf(model, spec.a.node)) return { error: '起点节点不存在' };
		if (spec.b.node !== undefined && !nodeOf(model, spec.b.node)) return { error: '终点节点不存在' };
		if (spec.a.conn !== undefined && spec.a.conn === spec.b.conn) return { error: '起点和终点在同一条线路上，请先在其中一处加节点' };
		const rid = resolveRouteId(model, spec.routeId || '', spec.a, spec.b);
		if (rid.error) return { error: rid.error };
		const a = resolveEnd(model, spec.a, ctx);
		const b = resolveEnd(model, spec.b, ctx);
		if (!a || !b) return { error: '无法解析端点' };
		if (a.id === b.id) return { error: '起点和终点相同' };
		const conn = {
			id: model.nextId++, a: a.id, b: b.id,
			ctrl: spec.ctrl ? [spec.ctrl[0], spec.ctrl[1]] : null,
			type: spec.type || 'ground',
			routeId: rid.routeId,
		};
		model.transit.conns.push(conn);
		return { conn };
	}

	// 改一条连线的route id：同一个连通块里"和它原来id相同"的连线一起改(没填id的一串线只填一次)。
	// 新id会让某个节点同时连着别的id的线时拒绝。返回 {count} | {error}
	function setRouteId(model, connId, newId) {
		const t = ensure(model);
		const c0 = connOf(model, connId);
		if (!c0) return { error: '连线不存在' };
		const old = c0.routeId;
		const seen = new Set([c0.id]), stack = [c0];
		while (stack.length) {
			const c = stack.pop();
			for (const o of t.conns) {
				if (seen.has(o.id) || o.routeId !== old) continue;
				if (o.a === c.a || o.a === c.b || o.b === c.a || o.b === c.b) { seen.add(o.id); stack.push(o); }
			}
		}
		const comp = t.conns.filter((c) => seen.has(c.id));
		if (newId) {
			for (const c of comp) for (const nid of [c.a, c.b]) for (const o of t.conns) {
				if (!seen.has(o.id) && (o.a === nid || o.b === nid) && o.routeId && o.routeId !== newId) return { error: `节点${nid}上还连着线路 ${o.routeId}，不能改成 ${newId}` };
			}
		}
		const nodes = new Set();
		for (const c of comp) { c.routeId = newId; nodes.add(c.a); nodes.add(c.b); }
		if (old) for (const l of t.loops) if (l.routeId === old && l.items.some((it) => it.kind === 'node' && nodes.has(it.ref))) l.routeId = newId || old;
		return { count: comp.length };
	}

	function deleteConn(model, connId) {
		const t = ensure(model);
		t.conns = t.conns.filter((c) => c.id !== connId);
		cleanup(model);
	}
	// 删除node：连着的conn一起删；环里引用它的项被移除(少于2项的环整个删除)。
	function deleteNode(model, nodeId) {
		const t = ensure(model);
		t.conns = t.conns.filter((c) => c.a !== nodeId && c.b !== nodeId);
		t.nodes = t.nodes.filter((n) => n.id !== nodeId);
		cleanup(model);
	}
	function deleteLoop(model, loopId) { const t = ensure(model); t.loops = t.loops.filter((l) => l.id !== loopId); }
	// 城外连接点(extern)移动时贴到最近的地图边界上。
	function snapToEdge(model, x, y) {
		const W = model.mapW, H = model.mapH;
		if (!(W > 0 && H > 0)) return [x, y];
		x = Math.max(0, Math.min(W, x)); y = Math.max(0, Math.min(H, y));
		const d = [x, y, W - x, H - y], k = d.indexOf(Math.min(...d));
		return k === 0 ? [0, y] : k === 1 ? [x, 0] : k === 2 ? [W, y] : [x, H];
	}
	function moveNode(model, nodeId, x, y) {
		const n = nodeOf(model, nodeId);
		if (!n) return;
		if (n.extern) [x, y] = snapToEdge(model, x, y);
		n.x = x; n.y = y;
	}

	// 清理：去掉环里失效的项/过短的环；没连任何线、也不在任何环里的孤立node删掉。
	function cleanup(model) {
		const t = ensure(model);
		const nodeIds = new Set(t.nodes.map((n) => n.id));
		const presetIds = new Set();
		for (const lot of model.lots || []) for (const p of lot.presets || []) presetIds.add(p.id);
		for (const l of t.loops) l.items = l.items.filter((it) => it.kind === 'preset' ? presetIds.has(it.ref) : nodeIds.has(it.ref));
		t.loops = t.loops.filter((l) => l.items.length >= 2);
		const used = new Set();
		for (const c of t.conns) { used.add(c.a); used.add(c.b); }
		for (const l of t.loops) for (const it of l.items) if (it.kind === 'node') used.add(it.ref);
		t.nodes = t.nodes.filter((n) => used.has(n.id));
	}

	// ---------------- 图：同routeId的最短路 ----------------
	// 从node from到node to，只走routeId相同的conn。返回 [{conn, forward}] (forward=沿a→b)，不通返回null。
	function shortestPath(model, routeId, from, to) {
		if (from === to) return [];
		const t = ensure(model);
		const adj = new Map();
		for (const c of t.conns) {
			if (c.routeId !== routeId) continue;
			const len = connLength(model, c);
			if (!adj.has(c.a)) adj.set(c.a, []);
			if (!adj.has(c.b)) adj.set(c.b, []);
			adj.get(c.a).push({ to: c.b, conn: c, forward: true, len });
			adj.get(c.b).push({ to: c.a, conn: c, forward: false, len });
		}
		if (!adj.has(from) || !adj.has(to)) return null;
		const dist = new Map([[from, 0]]), prev = new Map(), done = new Set();
		for (;;) {
			let cur = null, best = Infinity;
			for (const [id, d] of dist) if (!done.has(id) && d < best) { best = d; cur = id; }
			if (cur === null) return null;
			if (cur === to) break;
			done.add(cur);
			for (const e of adj.get(cur) || []) {
				const nd = best + e.len;
				if (nd < (dist.has(e.to) ? dist.get(e.to) : Infinity)) { dist.set(e.to, nd); prev.set(e.to, { from: cur, conn: e.conn, forward: e.forward }); }
			}
		}
		const path = [];
		for (let cur = to; cur !== from;) { const p = prev.get(cur); path.push({ conn: p.conn, forward: p.forward }); cur = p.from; }
		return path.reverse();
	}
	// 一段路径的折线点 [[x,y]...]
	function pathPoints(model, path) {
		const pts = [];
		for (const seg of path) {
			const poly = ME.polyline(quadOf(model, seg.conn)).map((p) => [p.x, p.y]);
			if (!seg.forward) poly.reverse();
			for (let i = pts.length ? 1 : 0; i < poly.length; i++) pts.push(poly[i]);
		}
		return pts;
	}

	// ---------------- 预设(世界坐标) ----------------
	// 已成功放置的预设：Map(presetId → {id, lotId, modId, poly:[4个角], cx, cy})。
	function presetPlacements(model, roadPieces) {
		const out = new Map();
		for (const lot of model.lots || []) {
			if (!lot.presets || !lot.presets.length) continue;
			const rep = ME.replayPresets(model, lot, roadPieces);
			if (!rep) continue;
			lot.presets.forEach((p, i) => {
				const placed = rep.steps[i].placed;
				if (!placed) return;
				const poly = [[placed.x0, placed.y0], [placed.x1, placed.y0], [placed.x1, placed.y1], [placed.x0, placed.y1]].map((q) => ME.localToWorld(rep.frame, q[0], q[1]));
				out.set(p.id, { id: p.id, lotId: lot.id, modId: p.modId || '', poly, cx: poly.reduce((a, c) => a + c[0], 0) / 4, cy: poly.reduce((a, c) => a + c[1], 0) / 4 });
			});
		}
		return out;
	}
	function itemPos(model, item, presets) {
		if (item.kind === 'node') { const n = nodeOf(model, item.ref); return n ? [n.x, n.y] : null; }
		const p = presets && presets.get(item.ref);
		return p ? [p.cx, p.cy] : null;
	}

	// 环的显示分段：相邻两项(含末→首)之间 {from, to, kind:'path'|'dashed', pts}。
	// 两个都是node且同routeId的实体线路能连通→沿线路；否则(含preset)→虚线直连。
	function loopSegments(model, loop, presets) {
		const segs = [];
		const n = loop.items.length;
		for (let i = 0; i < n; i++) {
			const ia = loop.items[i], ib = loop.items[(i + 1) % n];
			const pa = itemPos(model, ia, presets), pb = itemPos(model, ib, presets);
			if (!pa || !pb) continue;
			let seg = { from: i, to: (i + 1) % n, kind: 'dashed', pts: [pa, pb] };
			if (ia.kind === 'node' && ib.kind === 'node') {
				const path = shortestPath(model, loop.routeId, ia.ref, ib.ref);
				if (path && path.length) seg = { from: i, to: (i + 1) % n, kind: 'path', pts: pathPoints(model, path) };
			}
			segs.push(seg);
		}
		return segs;
	}

	// ---------------- 线路环操作 ----------------
	// 校验并推出环的routeId：node项所在线路id必须一致，且和指定的routeId一致。返回 {routeId} | {error}
	function loopRouteId(model, items, wanted) {
		let rid = wanted || '';
		for (const it of items) {
			if (it.kind !== 'node') continue;
			const ids = nodeRouteIds(model, it.ref);
			if (ids.size > 1) return { error: '有节点同时连着不同线路' };
			const id = ids.size ? [...ids][0] : '';
			if (!id) continue;
			if (rid && rid !== id) return { error: `环里混了不同线路(${rid} 和 ${id})` };
			rid = id;
		}
		if (!rid) return { error: '环没有线路id：请在面板里填route id，或让环经过已填id的线路节点' };
		return { routeId: rid };
	}
	const sameLoopItem = (a, b) => a.kind === b.kind && a.ref === b.ref;
	// 同一个点可以在环里出现多次(比如车站往返各停一次)，只是相邻两项(含末项→首项)不能是同一个点。
	function checkLoopItems(model, items) {
		if (items.length < 2) return '环至少要有2个点';
		for (let i = 0; i < items.length; i++) {
			const it = items[i], next = items[(i + 1) % items.length];
			if (sameLoopItem(it, next)) return '环里相邻两项不能是同一个点';
			if (it.kind === 'node' && !nodeOf(model, it.ref)) return '环里有已删除的节点';
		}
		return null;
	}
	// items: [{kind:'preset'|'node', ref}]；新环每个点默认不停站。末项和首项相同(点回了起点)时自动去掉末项。返回 {loop} | {error}。
	function addLoop(model, items, wantedRouteId) {
		ensure(model);
		if (items.length > 2 && sameLoopItem(items[0], items[items.length - 1])) items = items.slice(0, -1);
		const err = checkLoopItems(model, items);
		if (err) return { error: err };
		const rid = loopRouteId(model, items, wantedRouteId);
		if (rid.error) return { error: rid.error };
		const loop = { id: model.nextId++, routeId: rid.routeId, items: items.map((it) => it.kind === 'preset' ? { kind: 'preset', ref: it.ref, stop: false } : { kind: 'node', ref: it.ref }) };
		model.transit.loops.push(loop);
		return { loop };
	}
	function setStop(model, loopId, index, stop) {
		const l = loopOf(model, loopId);
		if (!l || !l.items[index] || l.items[index].kind !== 'preset') return false;
		l.items[index].stop = !!stop;
		return true;
	}

	// ---------------- 导出 ----------------
	// output.presetIndexById(不可枚举，lots.js里写)：presetId → output.presets下标。
	function exportTransit(model, terrain, output, warnings) {
		const t = ensure(model);
		const presetIdx = output.presetIndexById || new Map();
		const outNodes = [], nodeIdx = new Map();
		const nodeIndexOf = (id) => {
			if (nodeIdx.has(id)) return nodeIdx.get(id);
			const n = nodeOf(model, id);
			const out = { x: n.x, y: n.y, z: n.z || 0 };
			if (n.extern) {
				out.extern = true;
				if (!(n.x <= 0.01 || n.y <= 0.01 || n.x >= model.mapW - 0.01 || n.y >= model.mapH - 0.01)) warnings.push(`线路节点${n.id}是城外连接点但不在地图边界上`);
			}
			outNodes.push(out);
			nodeIdx.set(id, outNodes.length - 1);
			return outNodes.length - 1;
		};
		const conns = [];
		for (const c of t.conns) {
			if (!c.routeId) { warnings.push(`线路连线${c.id}没有填route id，已跳过`); continue; }
			const a = nodeOf(model, c.a), b = nodeOf(model, c.b);
			if (!a || !b) { warnings.push(`线路连线${c.id}的节点已不存在，已跳过`); continue; }
			if (terrain && [a, b].some((n) => n.x < 0 || n.y < 0 || n.x > terrain.w || n.y > terrain.h)) warnings.push(`线路连线${c.id}(${c.routeId})有端点在地图外`);
			const len = connLength(model, c);
			const slope = len > 1e-6 ? Math.abs((b.z || 0) - (a.z || 0)) / len : 0;
			if (c.type !== 'hug' && slope > ME.MAX_SLOPE) warnings.push(`线路连线${c.id}(${c.routeId})坡度${slope.toFixed(2)}太陡`);
			conns.push({ from: nodeIndexOf(c.a), to: nodeIndexOf(c.b), ctrl: c.ctrl ? [c.ctrl[0], c.ctrl[1]] : null, kind: c.type, route: c.routeId });
		}
		const loops = [];
		for (const l of t.loops) {
			const items = [];
			for (const it of l.items) {
				if (it.kind === 'node') {
					if (!nodeOf(model, it.ref)) { warnings.push(`线路环${l.id}的节点已不存在，该项已跳过`); continue; }
					items.push({ node: nodeIndexOf(it.ref) });
				} else {
					if (!presetIdx.has(it.ref)) { warnings.push(`线路环${l.id}引用的预设${it.ref}无效(没放置成功或所在地块被跳过)，该项已跳过`); continue; }
					items.push({ preset: presetIdx.get(it.ref), stop: !!it.stop });
				}
			}
			if (items.length < 2) { warnings.push(`线路环${l.id}有效项不足2个，已跳过`); continue; }
			if (!items.some((it) => it.stop)) warnings.push(`线路环${l.id}(${l.routeId})没有设置停站，游戏里这条环会被丢弃`);
			loops.push({ route: l.routeId, items });
		}
		if (conns.length || loops.length || outNodes.length) output.transit = { nodes: outNodes, conns, loops };
	}

	ME.TRANSIT_TYPES = TYPES;
	ME.TRANSIT_TYPE_NAMES = TYPE_NAMES;
	ME.newTransit = newTransit;
	ME.ensureTransit = ensure;
	ME.transitView = view;
	ME.tNodeOf = nodeOf;
	ME.tConnOf = connOf;
	ME.tLoopOf = loopOf;
	ME.routeColor = routeColor;
	ME.addTNode = addNode;
	ME.tNodeRouteIds = nodeRouteIds;
	ME.tNodeDegree = nodeDegree;
	ME.tQuadOf = quadOf;
	ME.tConnLength = connLength;
	ME.tZProfile = zProfile;
	ME.pickTNode = pickNode;
	ME.pickTConn = pickConn;
	ME.splitTConnAt = splitConnAt;
	ME.resolveRouteId = resolveRouteId;
	ME.addTConn = addConn;
	ME.deleteTConn = deleteConn;
	ME.setTRouteId = setRouteId;
	ME.deleteTNode = deleteNode;
	ME.deleteTLoop = deleteLoop;
	ME.moveTNode = moveNode;
	ME.cleanupTransit = cleanup;
	ME.tShortestPath = shortestPath;
	ME.tPathPoints = pathPoints;
	ME.presetPlacements = presetPlacements;
	ME.loopSegments = loopSegments;
	ME.loopRouteId = loopRouteId;
	ME.addTLoop = addLoop;
	ME.setLoopStop = setStop;
	ME.exportTransit = exportTransit;
	if (typeof module !== 'undefined') module.exports = ME;
})(typeof window !== 'undefined' ? window : globalThis);
