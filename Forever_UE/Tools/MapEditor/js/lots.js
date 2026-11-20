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
		'AREA_GREEN', 'AREA_TEST',
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

	// 地块覆盖的element里不可建造的个数(3x3邻域不全是plain/construction)。
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
		const lot = { id: model.nextId++, a, b, depth, area: area || 'AREA_RESIDENCE_HIGH', margin: [null, null, null, null] };
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
			if (bad > 0) warnings.push(`地块${lot.id}有${bad}个格子不可建造(非平原或邻格非平原)，游戏里这块地会被丢弃`);
			output.lots.push({
				corners: info.corners.map((c) => [c[0], c[1]]),
				margin, boundary, area: lot.area,
			});
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
	if (typeof module !== 'undefined') module.exports = ME;
})(typeof window !== 'undefined' ? window : globalThis);
