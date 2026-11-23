// 绿化区(green)：一个成环的2D多边形(可以是凹的，不能自交)，可选地把若干条边"连"到真实道路上。
// 模型：model.greens = [{id, pts:[[x,y],...], links:[{edge, roadId, t, x, y}]}]
//   pts   顶点，不重复首点；edge=i 指 pts[i]->pts[(i+1)%n] 这条边。
//   links 这条边连到哪条用户road；(x,y)=用户点路时路上的落点，t=该点的Bezier参数。
//         用户road被切开(双击加路口)后roadId/t会失效，所以导出时优先按roadId+落点复核，不行再按落点找最近的路(容差LINK_TOL)。
// 导出格式见 Source/Basic/map/roadnet_file.md：output.greens=[{points:[[x,y]..], links:[{edge, road:output.roads下标}]}]，顶点统一成有符号面积>0。
(function (root) {
	const ME = root.ME = root.ME || {};

	const LINK_TOL = 0.6;   // 导出时落点到路的最大距离(element)
	const EPS = 1e-9;

	function signedArea(pts) {
		let s = 0;
		for (let i = 0; i < pts.length; i++) { const a = pts[i], b = pts[(i + 1) % pts.length]; s += a[0] * b[1] - b[0] * a[1]; }
		return s / 2;
	}

	function orient(a, b, c) {
		const v = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
		return v > EPS ? 1 : v < -EPS ? -1 : 0;
	}
	function onSeg(a, b, p) {
		return Math.min(a[0], b[0]) - EPS <= p[0] && p[0] <= Math.max(a[0], b[0]) + EPS && Math.min(a[1], b[1]) - EPS <= p[1] && p[1] <= Math.max(a[1], b[1]) + EPS;
	}
	// 线段ab和cd是否相交(含端点接触、共线重叠)。
	function segmentsIntersect(a, b, c, d) {
		const o1 = orient(a, b, c), o2 = orient(a, b, d), o3 = orient(c, d, a), o4 = orient(c, d, b);
		if (o1 !== o2 && o3 !== o4) return true;
		if (o1 === 0 && onSeg(a, b, c)) return true;
		if (o2 === 0 && onSeg(a, b, d)) return true;
		if (o3 === 0 && onSeg(c, d, a)) return true;
		if (o4 === 0 && onSeg(c, d, b)) return true;
		return false;
	}

	// 闭合环是否自交(相邻边只共享一个端点，不算)。
	function ringSelfIntersects(pts) {
		const n = pts.length;
		if (n < 4) return false;
		for (let i = 0; i < n; i++) {
			for (let j = i + 2; j < n; j++) {
				if (i === 0 && j === n - 1) continue;
				if (segmentsIntersect(pts[i], pts[(i + 1) % n], pts[j], pts[(j + 1) % n])) return true;
			}
		}
		return false;
	}

	// 正在画的折线(未闭合)再加一个点p后是否自交：新边 last->p 不能碰到前面除相邻边以外的任何边。
	function canAppendVertex(pts, p) {
		const n = pts.length;
		if (n === 0) return true;
		const last = pts[n - 1];
		if (Math.hypot(last[0] - p[0], last[1] - p[1]) < 1e-6) return false;
		for (let i = 0; i + 1 < n - 1; i++) {
			if (segmentsIntersect(pts[i], pts[i + 1], last, p)) return false;
		}
		// 相邻边(n-2 -> n-1)：只允许共享last这个端点，不允许折回去重叠。
		if (n >= 2 && orient(pts[n - 2], last, p) === 0 && onSeg(pts[n - 2], last, p) && !(p[0] === last[0] && p[1] === last[1])) {
			const dx1 = last[0] - pts[n - 2][0], dy1 = last[1] - pts[n - 2][1], dx2 = p[0] - last[0], dy2 = p[1] - last[1];
			if (dx1 * dx2 + dy1 * dy2 < 0) return false;
		}
		return true;
	}
	// 闭合(最后一点连回第一点)是否合法：闭合边不能碰到除两端相邻边以外的边，且至少3个点、面积不为0。
	function canClose(pts) {
		const n = pts.length;
		if (n < 3) return false;
		if (Math.abs(signedArea(pts)) < 1e-6) return false;
		return !ringSelfIntersects(pts);
	}

	function pointInRing(x, y, pts) {
		let inside = false;
		for (let i = 0, j = pts.length - 1; i < pts.length; j = i++) {
			if (((pts[i][1] > y) !== (pts[j][1] > y)) && (x < (pts[j][0] - pts[i][0]) * (y - pts[i][1]) / (pts[j][1] - pts[i][1]) + pts[i][0])) inside = !inside;
		}
		return inside;
	}

	function segDistance(a, b, x, y) {
		const dx = b[0] - a[0], dy = b[1] - a[1], l2 = dx * dx + dy * dy;
		let f = l2 > EPS ? ((x - a[0]) * dx + (y - a[1]) * dy) / l2 : 0;
		f = Math.max(0, Math.min(1, f));
		return Math.hypot(x - (a[0] + dx * f), y - (a[1] + dy * f));
	}

	function addGreen(model, pts) {
		const green = { id: model.nextId++, pts: pts.map((p) => [p[0], p[1]]), links: [] };
		model.greens.push(green);
		return green;
	}
	function greenOf(model, id) { return (model.greens || []).find((g) => g.id === id); }
	function deleteGreen(model, id) { model.greens = model.greens.filter((g) => g.id !== id); }

	// 离(x,y)最近的绿化区边(半径内)：{green, edge, dist}。
	function pickGreenEdge(model, x, y, radius, onlyGreenId) {
		let best = null;
		for (const g of model.greens || []) {
			if (onlyGreenId !== undefined && g.id !== onlyGreenId) continue;
			const n = g.pts.length;
			for (let i = 0; i < n; i++) {
				const d = segDistance(g.pts[i], g.pts[(i + 1) % n], x, y);
				if (d <= radius && (!best || d < best.dist)) best = { green: g, edge: i, dist: d };
			}
		}
		return best;
	}
	// 点在哪个绿化区内(后建的优先)。
	function pickGreen(model, x, y) {
		const list = model.greens || [];
		for (let i = list.length - 1; i >= 0; i--) if (pointInRing(x, y, list[i].pts)) return list[i];
		return null;
	}

	// 给edge设置连路(同一条边只保留一个连接，重复设置覆盖)；(x,y,t)是路上的落点。
	function setLink(green, edge, road, t, x, y) {
		green.links = green.links.filter((l) => l.edge !== edge);
		green.links.push({ edge, roadId: road.id, t, x, y });
		green.links.sort((a, b) => a.edge - b.edge);
	}
	function removeLink(green, edge) { green.links = green.links.filter((l) => l.edge !== edge); }

	// 把link解析成当前模型里的{road, t}；路已经没了/落点离路太远返回null。
	function resolveLink(model, link) {
		const road = ME.roadOf(model, link.roadId);
		if (road) {
			const hit = ME.nearestOnRoad(model, road, link.x, link.y);
			if (hit.dist <= LINK_TOL) return { road, t: hit.t };
		}
		const hit = ME.pickRoad(model, link.x, link.y, LINK_TOL);
		return hit ? { road: hit.road, t: hit.t } : null;
	}

	// 该用户road展开的第几段output road：t落在哪一段(t0..t1)里，没落进去取最近的。
	function pieceIndexAt(ranges, t) {
		if (!ranges || !ranges.length) return 0;
		let best = 0, bestGap = Infinity;
		for (let k = 0; k < ranges.length; k++) {
			const gap = t < ranges[k].t0 ? ranges[k].t0 - t : (t > ranges[k].t1 ? t - ranges[k].t1 : 0);
			if (gap < bestGap) { bestGap = gap; best = k; }
		}
		return best;
	}

	// 导出到output.greens。pieceRanges: Map(roadId -> [{t0,t1}])，和roadPieces一一对应。
	function exportGreens(model, terrain, output, roadPieces, warnings, pieceRanges) {
		output.greens = [];
		for (const g of model.greens || []) {
			if (g.pts.length < 3) { warnings.push(`绿化${g.id}少于3个顶点，已跳过`); continue; }
			if (Math.abs(signedArea(g.pts)) < 1e-6) { warnings.push(`绿化${g.id}面积为0，已跳过`); continue; }
			if (ringSelfIntersects(g.pts)) { warnings.push(`绿化${g.id}的边自相交，已跳过`); continue; }
			let pts = g.pts.map((p) => [p[0], p[1]]);
			const n = pts.length;
			const reversed = signedArea(pts) < 0;
			if (reversed) pts.reverse();
			const links = [];
			for (const link of g.links || []) {
				if (link.edge < 0 || link.edge >= n) continue;
				const hit = resolveLink(model, link);
				const pieces = hit ? roadPieces.get(hit.road.id) : null;
				if (!hit || !pieces || !pieces.length) { warnings.push(`绿化${g.id}第${link.edge}条边连的路已不存在，这条连接被忽略`); continue; }
				const k = pieceIndexAt(pieceRanges ? pieceRanges.get(hit.road.id) : null, hit.t);
				links.push({ edge: reversed ? (n - 2 - link.edge + 2 * n) % n : link.edge, road: pieces[Math.min(k, pieces.length - 1)] });
			}
			links.sort((a, b) => a.edge - b.edge);
			output.greens.push({ points: pts, links });
			if (terrain) {
				for (const p of pts) {
					if (p[0] < 0 || p[1] < 0 || p[0] > terrain.w || p[1] > terrain.h) { warnings.push(`绿化${g.id}超出地图`); break; }
				}
			}
		}
	}

	ME.greenSignedArea = signedArea;
	ME.ringSelfIntersects = ringSelfIntersects;
	ME.canAppendVertex = canAppendVertex;
	ME.canCloseGreen = canClose;
	ME.pointInRing = pointInRing;
	ME.addGreen = addGreen;
	ME.greenOf = greenOf;
	ME.deleteGreen = deleteGreen;
	ME.pickGreenEdge = pickGreenEdge;
	ME.pickGreen = pickGreen;
	ME.setGreenLink = setLink;
	ME.removeGreenLink = removeLink;
	ME.resolveGreenLink = resolveLink;
	ME.exportGreens = exportGreens;
	ME.pieceIndexAt = pieceIndexAt;
	if (typeof module !== 'undefined') module.exports = ME;
})(typeof window !== 'undefined' ? window : globalThis);
