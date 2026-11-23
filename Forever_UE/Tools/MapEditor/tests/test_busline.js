// node tests/test_busline.js —— 路网路线(公交)：吸附到路的哪一侧(不吸附节点/路口)、单行道空侧不能放站、行车路径(单行道/掉头/不可达/extern不穿行)、
// 建线校验(含最后一站→第一站)、路被切开后站重挂、删路清理、导出road下标/警告、存读往返。
'use strict';
const assert = require('assert');
require('../js/io.js');
require('../js/roadnet.js');
require('../js/lots.js');
require('../js/greens.js');
require('../js/transit.js');
const ME = require('../js/busline.js');

const TWO_WAY = ME.LANE_PRESETS[0].lanes;      // 双向2车道
const ONE_AB = [2, 0, 1, 0, 0, 1];             // 单向 a→b
const ONE_BA = [0, 0, 1, 2, 0, 1];             // 单向 b→a

// 方框路：(20,20)-(60,20)-(60,60)-(20,60) 四条路，a→b 依次顺时针(y向下，所以东→南→西→北)
function square(lanes) {
	const m = ME.newModel(128, 128);
	const nodes = [[20, 20], [60, 20], [60, 60], [20, 60]].map((p) => { const node = { id: m.nextId++, x: p[0], y: p[1], z: 0 }; m.nodes.push(node); return node; });
	const roads = [];
	for (let i = 0; i < 4; i++) {
		const r = { id: m.nextId++, a: nodes[i].id, b: nodes[(i + 1) % 4].id, ctrl: null, type: 'ground', asset: 'default', lanes: lanes.slice(), name: 'r' + i };
		m.roads.push(r); roads.push(r);
	}
	return { m, nodes, roads };
}

// ---- 吸附：side0 = 沿a→b的右手边(y向下坐标系里 a→b向东时右手边在南=y大)
{
	const { m, roads } = square(TWO_WAY);
	const top = roads[0]; // (20,20)->(60,20)，向东
	const s0 = ME.busSnap(m, 40, 20.3);
	assert.strictEqual(s0.road, top.id); assert.strictEqual(s0.side, 0); assert.ok(s0.ok);
	assert.ok(Math.abs(s0.t - 0.5) < 1e-6);
	const s1 = ME.busSnap(m, 40, 19.7);
	assert.strictEqual(s1.side, 1); assert.ok(s1.ok);
	assert.strictEqual(ME.busSnap(m, 40, 40), null, '离所有路太远不吸附');
	// 不吸附节点：在节点附近仍然是垂足，不是节点
	const near = ME.busSnap(m, 20.5, 20.3);
	assert.ok(Math.abs(near.x - 20.5) < 1e-6, '垂足而不是节点');
}

// ---- 单行道：点哪边都吸到有车行道的一侧；没车行道的路 ok=false
{
	const { m, roads } = square(ONE_AB);
	const top = roads[0];
	assert.ok(ME.busSnap(m, 40, 20.3).ok);          // side0有车道
	const flip = ME.busSnap(m, 40, 19.7);   // 点在另一边：单行道自动吸到有车道的一侧
	assert.strictEqual(flip.side, 0); assert.ok(flip.ok);
	const none = square([0, 0, 1, 0, 0, 1]);   // 只有人行道的路：不能放站
	assert.ok(!ME.busSnap(none.m, 40, 20.3).ok);
	// 单行道的车行道居中(Core 连线居中约定)：站就在路中线上，向东行驶
	const p = ME.busStopPoint(m, { road: top.id, t: 0.5, side: 0 });
	assert.ok(Math.abs(p.y - 20) < 1e-9 && Math.abs(p.fx - 1) < 1e-9, `单行道站在中线、向东 (${p.y},${p.fx})`);
}
// 双向路：side0 的站在中线南侧(y>20)向东，side1 在北侧向西
{
	const { m, roads } = square(TWO_WAY);
	const p0 = ME.busStopPoint(m, { road: roads[0].id, t: 0.5, side: 0 }), p1 = ME.busStopPoint(m, { road: roads[0].id, t: 0.5, side: 1 });
	assert.ok(p0.y > 20 && p0.fx > 0.99 && p1.y < 20 && p1.fx < -0.99, `${p0.y} ${p0.fx} ${p1.y} ${p1.fx}`);
}

// ---- 路径：顺时针单行方框，A在上边side0，B在右边side0
{
	const { m, roads } = square(ONE_AB);
	const A = { road: roads[0].id, t: 0.3, side: 0 }, B = { road: roads[1].id, t: 0.5, side: 0 };
	const path = ME.busPath(m, A, B);
	assert.ok(path && path.length > 10, '顺行方向通');
	// 同路下游直达，上游则必须绕一圈(单行道)
	const down = ME.busPath(m, A, { road: roads[0].id, t: 0.8, side: 0 });
	const up = ME.busPath(m, { road: roads[0].id, t: 0.8, side: 0 }, A);
	assert.ok(down && up && up.length > down.length + 100, `上游要绕一圈 (${down.length} vs ${up.length})`);
	// 逆行不通：B放到side1(b→a，但这条路b→a没有车道)
	assert.strictEqual(ME.busPath(m, A, { road: roads[1].id, t: 0.5, side: 1 }), null);
}

// ---- 双向路：掉头。一条直路的两个站，A在side0、B在side1，需要在端点掉头
{
	const m = ME.newModel(128, 128);
	const a = { id: m.nextId++, x: 20, y: 20, z: 0 }, b = { id: m.nextId++, x: 80, y: 20, z: 0 };
	m.nodes.push(a, b);
	const r = { id: m.nextId++, a: a.id, b: b.id, ctrl: null, type: 'ground', asset: 'default', lanes: TWO_WAY.slice(), name: 'r' };
	m.roads.push(r);
	const A = { road: r.id, t: 0.3, side: 0 }, B = { road: r.id, t: 0.3, side: 1 };
	const path = ME.busPath(m, A, B);
	assert.ok(path, '双向路在尽头节点掉头');
	assert.ok(path.length > 60, `要开到尽头再回来 ${path.length}`);
	// extern 节点不可穿行：把b设成extern，A(side0向b)开不出去
	b.extern = true;
	assert.strictEqual(ME.busPath(m, A, B), null);
}

// ---- 建线：每一段(含最后→第一)都要通
{
	const { m, roads } = square(ONE_AB);
	const stops = [0, 1, 2].map((i) => ({ road: roads[i].id, t: 0.5, side: 0 }));
	const ok = ME.addBusLine(m, stops);
	assert.ok(ok.line && ok.line.stops.length === 3 && ok.line.route === 'route_bus' && ok.line.stops[0].station === 'station_bus');
	assert.strictEqual(ME.busLines(m).length, 1);
	assert.ok(ME.busLinePaths(m, ok.line).every((p) => p));
	assert.ok(ME.addBusLine(m, [stops[0]]).error, '至少2站');
	// 单行道里反着放两站，没有一条通路
	const bad = ME.addBusLine(m, [{ road: roads[0].id, t: 0.5, side: 0 }, { road: roads[0].id, t: 0.5, side: 1 }]);
	assert.ok(bad.error, '不可达的线建不出来');
	assert.strictEqual(ME.busLines(m).length, 1);
	// 改站资产、删站(剩不到2站整条删)
	assert.ok(ME.setBusStopStation(m, ok.line.stops[0].id, 'station_x'));
	assert.strictEqual(ME.busStopOf(m, ok.line.stops[0].id).stop.station, 'station_x');
	ME.deleteBusStop(m, ok.line.stops[2].id);
	assert.strictEqual(ME.busLines(m).length, 1);
	ME.deleteBusStop(m, ok.line.stops[1].id);
	assert.strictEqual(ME.busLines(m).length, 0);
}

// ---- 切开：站落在后半段则改挂新路并换算参数；cleanup：路被删站被清掉
{
	const { m, roads } = square(TWO_WAY);
	const top = roads[0];
	const L = ME.addBusLine(m, [{ road: top.id, t: 0.25, side: 0 }, { road: top.id, t: 0.75, side: 0 }]).line;
	const [s0, s1] = L.stops;
	const before1 = ME.evalQuad(ME.quadOf(m, top), 0.75);
	const node = ME.splitRoadAt(m, top.id, 0.5);
	assert.ok(node);
	assert.strictEqual(s0.road, top.id); assert.ok(Math.abs(s0.t - 0.5) < 1e-9);
	assert.notStrictEqual(s1.road, top.id); assert.ok(Math.abs(s1.t - 0.5) < 1e-9);
	const after1 = ME.evalQuad(ME.quadOf(m, ME.roadOf(m, s1.road)), s1.t);
	assert.ok(Math.hypot(before1[0] - after1[0], before1[1] - after1[1]) < 1e-9, '切开前后站的位置不变');
	// 删掉站所在的路 → 站被清掉，线不足2站被删
	m.roads = m.roads.filter((r) => r.id !== s1.road);
	ME.cleanup(m);
	assert.strictEqual(ME.busLines(m).length, 0);
}

// ---- 导出：road下标按输出路段，警告
{
	const { m, roads } = square(ONE_AB);
	const L = ME.addBusLine(m, [0, 2].map((i) => ({ road: roads[i].id, t: 0.5, side: 0 }))).line;
	const out = ME.exportRoadnet(m, null);
	const rl = out.output.transit.roadLines;
	assert.strictEqual(rl.length, 1);
	assert.strictEqual(rl[0].route, 'route_bus');
	assert.strictEqual(rl[0].stops.length, 2);
	const idx = out.roadPieces.get(roads[0].id)[0];
	assert.strictEqual(rl[0].stops[0].road, idx);
	assert.ok(Math.abs(rl[0].stops[0].x - 40) < 1e-6 && Math.abs(rl[0].stops[0].y - 20) < 1e-6, '导出路中线垂足');
	assert.strictEqual(rl[0].stops[0].side, 0);
	assert.strictEqual(rl[0].stops[0].station, 'station_bus');
	// 车道改成空了那一侧 → 警告并跳过站，线不足2站被丢弃
	roads[0].lanes = ONE_BA.slice();
	const out2 = ME.exportRoadnet(m, null);
	assert.ok(out2.warnings.some((w) => w.includes('没有车行道')));
	assert.ok(!out2.output.transit || !out2.output.transit.roadLines);
}

// ---- 存读往返 + 旧模型迁移
{
	const { m, roads } = square(TWO_WAY);
	ME.addBusLine(m, [0, 2].map((i) => ({ road: roads[i].id, t: 0.4, side: 0 })));
	const back = JSON.parse(JSON.stringify(m));
	ME.migrateModel(back);
	assert.deepStrictEqual(back.transit.roadLines, m.transit.roadLines);
	const old = { version: 2, mapW: 64, mapH: 64, nodes: [], roads: [], lots: [], nextId: 1 };
	ME.migrateModel(old);
	assert.deepStrictEqual(old.transit.roadLines, []);
}

console.log('test_busline OK');
